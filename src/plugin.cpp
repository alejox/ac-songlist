#include "plugin.hpp"
#include "plugin-support.hpp"
#include "web/web-config.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <mutex>

namespace acsl {

namespace {
std::mutex g_mutex; // protege g_instance y g_pending
Plugin *g_instance = nullptr;
std::vector<std::shared_ptr<SourceShared>> g_pending;

const double kFadeOutMs = 160;
const double kFadeInMs = 260;
} // namespace

void Plugin::create()
{
	Plugin *p = new Plugin;
	std::lock_guard<std::mutex> lock(g_mutex);
	g_instance = p;
}

void Plugin::destroy()
{
	Plugin *p;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		p = g_instance;
		g_instance = nullptr;
		g_pending.clear();
	}
	delete p;
}

Plugin *Plugin::instance()
{
	return g_instance;
}

void Plugin::addSource(const std::shared_ptr<SourceShared> &shared)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_pending.push_back(shared);
	if (g_instance)
		QMetaObject::invokeMethod(g_instance, "syncSources", Qt::QueuedConnection);
}

void Plugin::notifySourcesChanged()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_instance)
		QMetaObject::invokeMethod(g_instance, "syncSources", Qt::QueuedConnection);
}

void Plugin::post(void (*fn)(Plugin *))
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (!g_instance)
		return;
	QMetaObject::invokeMethod(
		g_instance,
		[fn]() {
			if (Plugin *p = Plugin::instance())
				fn(p);
		},
		Qt::QueuedConnection);
}

Plugin::Plugin()
{
	char *path = obs_module_config_path("songlist.json");
	m_path = QString::fromUtf8(path ? path : "");
	bfree(path);
	if (!m_path.isEmpty()) {
		QDir().mkpath(QFileInfo(m_path).absolutePath());
		if (QFile::exists(m_path) && !m_store.load(m_path)) {
			// Nunca pisar un archivo que no pudimos leer: se aparta con otro nombre.
			const QString backup = m_path + QStringLiteral(".ilegible-") +
					       QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
			QFile::rename(m_path, backup);
			blog(LOG_WARNING, "[ac-songlist] no se pudo leer %s; copia en %s", qUtf8Printable(m_path),
			     qUtf8Printable(backup));
		}
	}
	blog(LOG_INFO, "[ac-songlist] %d canciones cargadas", int(m_store.songs().size()));

	// Pagina web publica: direccion del servicio (las variables de entorno son solo para pruebas).
	m_publisher = std::make_unique<Publisher>(&m_store);
	m_publisher->setEndpoint(qEnvironmentVariable("ACSL_WEB_API_URL", QStringLiteral(ACSL_WEB_API_URL)),
				 qEnvironmentVariable("ACSL_WEB_API_KEY", QStringLiteral(ACSL_WEB_API_KEY)),
				 qEnvironmentVariable("ACSL_WEB_PAGE_URL", QStringLiteral(ACSL_WEB_PAGE_URL)));
	blog(LOG_INFO, "[ac-songlist] pagina web publica: %s", m_publisher->available() ? "disponible" : "no disponible en esta version");

	m_startedAt = QDateTime::currentMSecsSinceEpoch();
	m_bot = std::make_unique<Bot>(&m_store, [](const char *key) { return T(key); });

	m_saveTimer.setSingleShot(true);
	m_saveTimer.setInterval(800);
	connect(&m_saveTimer, &QTimer::timeout, this, &Plugin::saveNow);
	connect(&m_store, &Store::changed, this, [this]() {
		m_saveTimer.start();
		refresh();
	});
	connect(&m_store, &Store::settingsChanged, this, &Plugin::applyChatSettings);
	connect(&m_store, &Store::listRequested, this, [this]() {
		const qint64 now = QDateTime::currentMSecsSinceEpoch();
		if (now >= m_listUntil)
			m_listRestart = true; // estaba oculta: empieza por la primera pagina
		// un segundo !lista alarga el tiempo en pantalla, nunca lo acorta
		m_listUntil = qMax(m_listUntil, now + qint64(qBound(5, m_store.settings().listSeconds, 600)) * 1000);
		refresh();
	});
	connect(&m_chat, &TwitchChat::message, this, &Plugin::onChatMessage);

	m_animTimer.setInterval(33);
	connect(&m_animTimer, &QTimer::timeout, this, &Plugin::refresh);
	m_slowTimer.setInterval(250); // cambios de pagina y fin de la lista "a pedido"
	connect(&m_slowTimer, &QTimer::timeout, this, &Plugin::refresh);
	m_slowTimer.start();

	applyChatSettings();
}

Plugin::~Plugin()
{
	m_publisher.reset();
	m_chat.stop();
	saveNow();
}

void Plugin::saveNow()
{
	m_saveTimer.stop();
	if (m_path.isEmpty())
		return;
	if (!m_store.save(m_path))
		blog(LOG_WARNING, "[ac-songlist] no se pudo guardar %s", qUtf8Printable(m_path));
}

void Plugin::applyChatSettings()
{
	// Solo lectura: el plugin se conecta de forma anonima y nunca escribe en el chat.
	m_chat.configure(m_store.settings().channel, QString(), QString());
}

void Plugin::onChatMessage(const ChatUser &user, const QString &text)
{
	m_bot->handle(user, text); // aplica el comando; la respuesta de texto no se envia
}

void Plugin::syncSources()
{
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		for (auto &shared : g_pending) {
			View v;
			v.shared = shared;
			m_views.push_back(std::move(v));
		}
		g_pending.clear();
	}
	m_views.erase(std::remove_if(m_views.begin(), m_views.end(), [](const View &v) { return !v.shared->alive; }),
		      m_views.end());
	refresh();
}

void Plugin::refresh()
{
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	const double dt = m_lastRefresh ? double(qBound(qint64(0), now - m_lastRefresh, qint64(200))) : 0;
	m_lastRefresh = now;

	bool animating = false;
	for (View &view : m_views)
		animating = updateView(view, now, dt) || animating;
	m_listRestart = false;

	if (animating && !m_animTimer.isActive())
		m_animTimer.start();
	else if (!animating && m_animTimer.isActive())
		m_animTimer.stop();
}

static QString joinRow(const OverlayRow &r)
{
	return QString::number(r.number) + QChar(0x1f) + r.title + QChar(0x1f) + r.artist + QChar(0x1f) + r.requester +
	       QChar(0x1e);
}

void Plugin::setLabels(OverlayData &d) const
{
	d.requestsOpen = m_store.settings().requestsOpen;
	d.lblNowPlaying = T("Overlay.NowPlaying");
	d.lblQueue = T("Overlay.Queue");
	d.lblSongList = T("Overlay.SongList");
	d.lblRequestedBy = T("Overlay.RequestedBy");
	d.lblHowTo = T("Overlay.HowTo");
	d.lblMore = T("Overlay.More");
	d.lblClosed = T("Overlay.Closed");
	d.lblEmptyQueue = T("Overlay.EmptyQueue");
	d.lblEmptyList = T("Overlay.EmptyList");
}

// Lista de canciones, por paginas. identity queda vacio si no hay que mostrarla.
void Plugin::fillSongList(View &view, qint64 now, bool enabled, OverlayData &d, QString *identity)
{
	const OverlayStyle &s = view.style;
	QList<const Song *> active;
	for (const Song &song : m_store.songs())
		if (song.active)
			active.append(&song);
	std::sort(active.begin(), active.end(), [](const Song *a, const Song *b) { return a->id < b->id; });

	const int perPage = songsPerPage(s);
	d.songsTotal = int(active.size());
	d.pages = qMax(1, (d.songsTotal + perPage - 1) / perPage);

	bool visible = enabled && !(active.isEmpty() && s.hideWhenEmpty);
	if (view.listOnDemand) {
		// se muestra al menos una vuelta completa de paginas
		const qint64 fullTurn = qint64(d.pages) * view.pageSeconds * 1000;
		if (m_listRestart) {
			view.page = 0;
			view.pageSince = now;
			m_listUntil = qMax(m_listUntil, now + fullTurn);
		}
		visible = visible && now < m_listUntil;
	}
	if (!visible) {
		*identity = QString();
		return;
	}
	if (view.pageSince == 0)
		view.pageSince = now;
	if (d.pages > 1 && now - view.pageSince >= qint64(view.pageSeconds) * 1000) {
		view.page++;
		view.pageSince = now;
	}
	if (view.page >= d.pages)
		view.page = 0;
	d.page = view.page;
	for (int i = d.page * perPage; i < active.size() && i < (d.page + 1) * perPage; i++)
		d.songs.append({active[i]->id, active[i]->title, active[i]->artist, QString()});
	*identity = QStringLiteral("list") + QString::number(d.page);
}

OverlayData Plugin::buildData(View &view, qint64 now, QString *identity)
{
	const OverlayStyle &s = view.style;
	OverlayData d;
	setLabels(d);

	const QueueItem &playing = m_store.playing();
	if (s.mode == OverlayStyle::NowPlaying) {
		d.hasPlaying = playing.valid();
		if (d.hasPlaying) {
			d.title = playing.title;
			d.artist = playing.artist;
			d.requesters = playing.requesters();
			*identity = QStringLiteral("np") + QChar(0x1f) + QString::number(playing.id) + QChar(0x1f) +
				    d.title + QChar(0x1f) + d.artist;
		} else {
			*identity = s.idleText.trimmed().isEmpty() ? QString() : QStringLiteral("idle");
		}
		return d;
	}

	if (s.mode == OverlayStyle::Queue) {
		const QList<QueueItem> &queue = m_store.queue();
		d.queueTotal = int(queue.size());
		for (int i = 0; i < queue.size() && i < s.rows; i++)
			d.queue.append({i + 1, queue[i].title, queue[i].artist, queue[i].requesters().join(QStringLiteral(", "))});
		*identity = (queue.isEmpty() && s.hideWhenEmpty) ? QString() : QStringLiteral("queue");
		return d;
	}

	fillSongList(view, now, true, d, identity);
	return d;
}

// Barra: los tres paneles comparten fuente pero cada uno tiene su propio fundido, para que
// encender o apagar uno (o cambiar de cancion) no haga parpadear a los otros dos.
bool Plugin::updateBar(View &view, qint64 now, double dtMs)
{
	const OverlayStyle &s = view.style;
	const Settings &cfg = m_store.settings();
	OverlayData target;
	QString ids[3];

	fillSongList(view, now, cfg.barList, target, &ids[OverlayStyle::BarList]);

	const QueueItem &playing = m_store.playing();
	target.hasPlaying = playing.valid();
	if (target.hasPlaying) {
		target.title = playing.title;
		target.artist = playing.artist;
		target.requesters = playing.requesters();
	}
	if (cfg.barNow) {
		if (target.hasPlaying)
			ids[OverlayStyle::BarNow] = QStringLiteral("np") + QChar(0x1f) + QString::number(playing.id) + QChar(0x1f) +
						    target.title + QChar(0x1f) + target.artist;
		else if (!s.idleText.trimmed().isEmpty())
			ids[OverlayStyle::BarNow] = QStringLiteral("idle");
	}

	const QList<QueueItem> &queue = m_store.queue();
	const int queueRows = barLayout(s).queueRows;
	target.queueTotal = int(queue.size());
	for (int i = 0; i < queue.size() && i < queueRows; i++)
		target.queue.append({i + 1, queue[i].title, queue[i].artist, queue[i].requesters().join(QStringLiteral(", "))});
	if (cfg.barQueue)
		ids[OverlayStyle::BarQueue] = QStringLiteral("queue");

	// lo que cada panel tiene en pantalla solo cambia cuando ese panel ya se desvanecio
	const auto take = [&](int section) {
		OverlayData &d = view.data;
		if (section == OverlayStyle::BarList) {
			d.songs = target.songs;
			d.page = target.page;
			d.pages = target.pages;
			d.songsTotal = target.songsTotal;
		} else if (section == OverlayStyle::BarNow) {
			d.hasPlaying = target.hasPlaying;
			d.title = target.title;
			d.artist = target.artist;
			d.requesters = target.requesters;
		} else {
			d.queue = target.queue;
			d.queueTotal = target.queueTotal;
		}
	};

	bool animating = false;
	bool anyVisible = false;
	for (int i = 0; i < 3; i++) {
		QString &identity = view.sectionIdentity[i];
		double &opacity = view.sectionOpacity[i];
		if (ids[i] != identity) {
			if (opacity > 0 && !identity.isEmpty()) {
				opacity = qMax(0.0, opacity - dtMs / kFadeOutMs);
				animating = true;
			}
			if (opacity <= 0 || identity.isEmpty()) {
				opacity = 0;
				take(i);
				identity = ids[i];
				animating = animating || !identity.isEmpty();
			}
		} else {
			take(i);
			if (identity.isEmpty()) {
				opacity = 0;
			} else if (opacity < 1) {
				opacity = qMin(1.0, opacity + dtMs / kFadeInMs);
				animating = animating || opacity < 1;
			}
		}
		anyVisible = anyVisible || !identity.isEmpty();
	}
	setLabels(view.data);
	view.identity = anyVisible ? QStringLiteral("bar") : QString();
	view.opacity = anyVisible ? 1.0 : 0.0;

	// el disco gira mientras el panel central esta en pantalla con una cancion sonando
	const bool moving = s.animate && view.data.hasPlaying && !view.sectionIdentity[OverlayStyle::BarNow].isEmpty();
	if (moving)
		animating = true;
	const qint64 frame = moving ? now / 66 : 0;

	const OverlayData &d = view.data;
	QString sig = QStringLiteral("bar") + QString::number(view.styleVersion) + QChar(0x1d) + QString::number(frame) + QChar(0x1d) +
		      d.title + QChar(0x1f) + d.artist + QChar(0x1f) + d.requesters.join(QLatin1Char(',')) + QChar(0x1f) +
		      (d.hasPlaying ? QLatin1Char('1') : QLatin1Char('0')) + QChar(0x1d) + QString::number(d.queueTotal) + QChar(0x1f) +
		      QString::number(d.songsTotal) + QChar(0x1f) + QString::number(d.page) + QChar(0x1f) + QString::number(d.pages) +
		      QChar(0x1f) + (d.requestsOpen ? QLatin1Char('1') : QLatin1Char('0')) + QChar(0x1d);
	for (int i = 0; i < 3; i++)
		sig += view.sectionIdentity[i] + QChar(0x1f) + QString::number(int(view.sectionOpacity[i] * 255)) + QChar(0x1d);
	for (const OverlayRow &r : d.queue)
		sig += joinRow(r);
	for (const OverlayRow &r : d.songs)
		sig += joinRow(r);
	if (sig == view.lastRender)
		return animating;
	view.lastRender = sig;

	for (int i = 0; i < 3; i++) { // ease-out, como en el resto de los modos
		const double o = view.sectionOpacity[i];
		view.data.section[i] = view.sectionIdentity[i].isEmpty() ? 0.0 : 1.0 - (1.0 - o) * (1.0 - o);
	}
	const double seconds = double(now - m_startedAt) / 1000.0;
	QImage image = renderOverlay(s, view.data, anyVisible ? 1.0 : 0.0, seconds, &view.cache);
	{
		std::lock_guard<std::mutex> lock(view.shared->mutex);
		view.shared->image = std::move(image);
		view.shared->imageDirty = true;
	}
	return animating;
}

// Devuelve true mientras haya un fundido en marcha.
bool Plugin::updateView(View &view, qint64 now, double dtMs)
{
	{
		std::lock_guard<std::mutex> lock(view.shared->mutex);
		if (view.styleVersion != view.shared->styleVersion) {
			view.styleVersion = view.shared->styleVersion;
			view.style = view.shared->style;
			view.listOnDemand = view.shared->listOnDemand;
			view.pageSeconds = view.shared->pageSeconds;
			view.cache.clear(); // otro estilo: lo guardado ya no vale
		}
	}
	if (view.style.mode == OverlayStyle::Bar)
		return updateBar(view, now, dtMs);

	QString targetIdentity;
	const OverlayData target = buildData(view, now, &targetIdentity);
	bool animating = false;

	if (targetIdentity != view.identity) {
		if (view.opacity > 0 && !view.identity.isEmpty()) {
			// primero se desvanece lo que habia
			view.opacity = qMax(0.0, view.opacity - dtMs / kFadeOutMs);
			animating = true;
		}
		if (view.opacity <= 0 || view.identity.isEmpty()) {
			view.opacity = 0;
			view.data = target;
			view.identity = targetIdentity;
			animating = !targetIdentity.isEmpty();
		}
	} else {
		view.data = target; // mismo "que", cambian detalles: se redibuja sin fundido
		if (view.identity.isEmpty()) {
			view.opacity = 0;
		} else if (view.opacity < 1) {
			view.opacity = qMin(1.0, view.opacity + dtMs / kFadeInMs);
			animating = view.opacity < 1;
		}
	}

	// Skins con movimiento (carretes, disco, ecualizador): se redibujan unas 15 veces por segundo
	// mientras hay algo en pantalla.
	const bool moving = isAnimated(view.style) && !view.identity.isEmpty();
	if (moving)
		animating = true;
	const qint64 frame = moving ? now / 66 : 0;

	// firma de lo que se ve: solo se redibuja si cambia
	const OverlayData &d = view.data;
	QString sig = QString::number(view.styleVersion) + QChar(0x1d) + QString::number(frame) + QChar(0x1d) +
		      QString::number(int(view.opacity * 255)) +
		      QChar(0x1d) + view.identity + QChar(0x1d) + d.title + QChar(0x1f) + d.artist + QChar(0x1f) +
		      d.requesters.join(QLatin1Char(',')) + QChar(0x1d) + QString::number(d.queueTotal) + QChar(0x1f) +
		      QString::number(d.songsTotal) + QChar(0x1f) + QString::number(d.pages) + QChar(0x1f) +
		      (d.requestsOpen ? QLatin1Char('1') : QLatin1Char('0')) + QChar(0x1d);
	for (const OverlayRow &r : d.queue)
		sig += joinRow(r);
	for (const OverlayRow &r : d.songs)
		sig += joinRow(r);
	if (sig == view.lastRender)
		return animating;
	view.lastRender = sig;

	// ease-out: el fundido arranca rapido y frena al final
	const double eased = 1.0 - (1.0 - view.opacity) * (1.0 - view.opacity);
	// reloj de la animacion: segundos desde que arranco el plugin (un numero pequeno, sin saltos)
	const double seconds = double(now - m_startedAt) / 1000.0;
	QImage image = renderOverlay(view.style, view.data, view.identity.isEmpty() ? 0.0 : eased, seconds);
	{
		std::lock_guard<std::mutex> lock(view.shared->mutex);
		view.shared->image = std::move(image);
		view.shared->imageDirty = true;
	}
	return animating;
}

} // namespace acsl

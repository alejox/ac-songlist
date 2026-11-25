#include "publisher.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRandomGenerator>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace acsl {

// Hilo que hace las peticiones de una en una, para no bloquear nunca la interfaz de OBS.
struct Publisher::Worker {
	std::mutex mutex;
	std::condition_variable wake;
	std::deque<std::function<void()>> jobs;
	bool stopping = false;
	std::atomic<bool> finished{false};
	std::thread thread;
};

Publisher::Publisher(Store *store, QObject *parent) : QObject(parent), m_store(store)
{
	m_transport = [](const QUrl &url, const HttpHeaders &headers, const QByteArray &body) {
		return httpPostJson(url, headers, body);
	};
	m_timer.setSingleShot(true);
	connect(&m_timer, &QTimer::timeout, this, &Publisher::publishNow);
	connect(store, &Store::changed, this, [this]() { schedule(m_debounceMs); });
	connect(store, &Store::settingsChanged, this, [this]() {
		m_blocked = false; // otro canal u otra clave: se vuelve a intentar
		m_failures = 0;
		schedule(m_debounceMs);
	});

	auto worker = std::make_shared<Worker>();
	m_worker = worker;
	worker->thread = std::thread([worker]() {
		for (;;) {
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(worker->mutex);
				worker->wake.wait(lock, [&]() { return worker->stopping || !worker->jobs.empty(); });
				if (worker->stopping)
					break;
				job = std::move(worker->jobs.front());
				worker->jobs.pop_front();
			}
			job();
		}
		worker->finished = true;
	});
}

Publisher::~Publisher()
{
	std::shared_ptr<Worker> worker = m_worker;
	{
		std::lock_guard<std::mutex> lock(worker->mutex);
		worker->stopping = true;
		worker->jobs.clear();
	}
	worker->wake.notify_all();
	// Si justo hay una peticion en curso se le da un momento; si la red no responde,
	// no se retiene el cierre de OBS.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (!worker->finished && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	if (worker->finished)
		worker->thread.join();
	else
		worker->thread.detach();
}

void Publisher::setEndpoint(const QString &apiUrl, const QString &apiKey, const QString &pageUrl)
{
	m_apiUrl = apiUrl.trimmed();
	while (m_apiUrl.endsWith(QLatin1Char('/')))
		m_apiUrl.chop(1);
	m_apiKey = apiKey.trimmed();
	m_pageUrl = pageUrl.trimmed();
	resetSent();
	schedule(m_debounceMs);
}

void Publisher::setTransport(Transport transport)
{
	m_transport = std::move(transport);
}

void Publisher::setDelays(int debounceMs, int retryMs)
{
	m_debounceMs = debounceMs;
	m_retryMs = retryMs;
}

bool Publisher::available() const
{
	if (m_apiUrl.isEmpty() || m_apiKey.isEmpty())
		return false;
	return m_apiUrl.startsWith(QLatin1String("http://")) || httpsAvailable();
}

bool Publisher::active() const
{
	const Settings &c = m_store->settings();
	return available() && c.webEnabled && !c.channel.isEmpty() && c.webKey.size() >= 32;
}

QString Publisher::pageUrl() const
{
	const QString channel = m_store->settings().channel;
	if (m_pageUrl.isEmpty() || channel.isEmpty())
		return QString();
	return m_pageUrl + (m_pageUrl.contains(QLatin1Char('?')) ? QLatin1Char('&') : QLatin1Char('?')) + QStringLiteral("canal=") + channel;
}

QString Publisher::newKey()
{
	QString key;
	for (int i = 0; i < 4; i++)
		key += QStringLiteral("%1").arg(QRandomGenerator::system()->generate64(), 16, 16, QLatin1Char('0'));
	return key; // 64 caracteres hexadecimales: 256 bits al azar
}

QByteArray Publisher::songsJson(const Store &store)
{
	QList<const Song *> songs;
	for (const Song &s : store.songs())
		if (s.active) // las desactivadas no se publican
			songs.append(&s);
	std::sort(songs.begin(), songs.end(), [](const Song *a, const Song *b) { return a->id < b->id; });
	QJsonArray out;
	for (const Song *s : songs) {
		QJsonObject o{{"n", s->id}, {"t", s->title}, {"a", s->artist}, {"p", s->timesPlayed}};
		if (!s->categories.isEmpty())
			o.insert("c", QJsonArray::fromStringList(s->categories));
		out.append(o);
	}
	return QJsonDocument(out).toJson(QJsonDocument::Compact);
}

static QJsonObject itemJson(const QueueItem &item)
{
	return QJsonObject{{"n", item.songId}, {"t", item.title}, {"a", item.artist}, {"by", QJsonArray::fromStringList(item.requesters())}};
}

QByteArray Publisher::liveJson(const Store &store)
{
	QJsonObject live{{"open", store.settings().requestsOpen}, {"total", int(store.queue().size())}};
	if (store.playing().valid())
		live.insert("playing", itemJson(store.playing()));
	QJsonArray queue;
	for (int i = 0; i < store.queue().size() && i < 30; i++)
		queue.append(itemJson(store.queue()[i]));
	live.insert("queue", queue);
	return QJsonDocument(live).toJson(QJsonDocument::Compact);
}

void Publisher::resetSent()
{
	m_sentSongs.clear();
	m_sentLive.clear();
	m_sentFor.clear();
}

void Publisher::setState(State state, const QString &message)
{
	if (m_state == state && m_message == message)
		return;
	m_state = state;
	m_message = message;
	emit stateChanged();
}

void Publisher::schedule(int ms)
{
	if (!m_timer.isActive() || m_timer.remainingTime() > ms)
		m_timer.start(qMax(0, ms));
}

void Publisher::publishNow()
{
	if (!active()) {
		resetSent();
		setState(State::Off);
		return;
	}
	if (m_blocked)
		return;
	if (m_inFlight) { // al terminar la que esta en curso se vuelve a mirar
		schedule(300);
		return;
	}

	const Settings &c = m_store->settings();
	const QString target = c.channel + QChar(0x1f) + c.webKey;
	if (target != m_sentFor) {
		resetSent();
		m_sentFor = target;
	}
	const QByteArray songs = songsJson(*m_store);
	const QByteArray live = liveJson(*m_store);
	const bool sendSongs = songs != m_sentSongs;
	const bool sendLive = live != m_sentLive;
	if (!sendSongs && !sendLive) {
		if (m_state != State::Error)
			setState(State::Published);
		return;
	}

	// El catalogo solo viaja cuando cambio; con cada pedido va solo lo que suena y la cola.
	QByteArray body = QJsonDocument(QJsonObject{{"p_channel", c.channel}, {"p_key", c.webKey}, {"p_title", c.channel}})
				  .toJson(QJsonDocument::Compact);
	body.chop(1); // se quita la llave final para anadir las dos partes grandes ya serializadas
	if (sendSongs)
		body += ",\"p_songs\":" + songs;
	if (sendLive)
		body += ",\"p_live\":" + live;
	body += "}";

	const QUrl url(m_apiUrl + QStringLiteral("/rest/v1/rpc/publish_catalog"));
	const HttpHeaders headers{{"apikey", m_apiKey.toLatin1()}, {"Authorization", "Bearer " + m_apiKey.toLatin1()}};
	const quint64 id = ++m_requestId;
	m_inFlight = true;
	if (m_state != State::Published)
		setState(State::Pending);

	const Transport transport = m_transport;
	const QPointer<Publisher> self(this);
	const QByteArray sentSongs = sendSongs ? songs : m_sentSongs;
	const QByteArray sentLive = sendLive ? live : m_sentLive;
	{
		std::lock_guard<std::mutex> lock(m_worker->mutex);
		m_worker->jobs.push_back([transport, url, headers, body, self, id, sentSongs, sentLive]() {
			const HttpResult result = transport(url, headers, body);
			QCoreApplication *app = QCoreApplication::instance();
			if (!app)
				return;
			QMetaObject::invokeMethod(
				app,
				[self, id, result, sentSongs, sentLive]() {
					if (self)
						self->onResult(id, result, sentSongs, sentLive);
				},
				Qt::QueuedConnection);
		});
	}
	m_worker->wake.notify_all();
}

void Publisher::onResult(quint64 id, const HttpResult &result, const QByteArray &songs, const QByteArray &live)
{
	if (id != m_requestId)
		return;
	m_inFlight = false;

	if (result.sent && result.status >= 200 && result.status < 300) {
		const QString status = QJsonDocument::fromJson(result.body).object().value("status").toString();
		if (status == QLatin1String("slow")) { // el servicio pide espaciar los avisos
			schedule(1200);
			return;
		}
		m_sentSongs = songs;
		m_sentLive = live;
		m_failures = 0;
		m_lastPublished = QDateTime::currentDateTime();
		setState(State::Published);
		schedule(0); // por si algo cambio mientras viajaba esta
		return;
	}

	// "Permiso denegado" (42501) es como el servicio dice que ese canal tiene otra clave. Segun
	// quien llame, llega como 401 o como 403, asi que se mira el codigo y no solo el numero.
	const QString errorCode = QJsonDocument::fromJson(result.body).object().value("code").toString();
	if (result.sent && (result.status == 403 || errorCode == QLatin1String("42501"))) {
		// Ese canal ya lo reclamo otra instalacion: reintentar no lo arregla.
		m_blocked = true;
		setState(State::Error, QStringLiteral("taken"));
		return;
	}
	if (result.sent && result.status >= 400 && result.status < 500 && result.status != 408 && result.status != 429) {
		m_blocked = true;
		const QString detail = QJsonDocument::fromJson(result.body).object().value("message").toString();
		setState(State::Error, detail.isEmpty() ? QStringLiteral("HTTP %1").arg(result.status) : detail);
		return;
	}

	// Sin conexion o fallo pasajero del servicio: se reintenta cada vez mas espaciado.
	m_failures++;
	setState(State::Error, result.sent ? QStringLiteral("HTTP %1").arg(result.status) : QStringLiteral("offline"));
	schedule(qMin(m_retryMs * (1 << qMin(m_failures - 1, 4)), 120000));
}

void Publisher::unpublish(const QString &channel, const QString &key)
{
	if (!available() || channel.isEmpty() || key.isEmpty())
		return;
	const QByteArray body = QJsonDocument(QJsonObject{{"p_channel", channel}, {"p_key", key}}).toJson(QJsonDocument::Compact);
	const QUrl url(m_apiUrl + QStringLiteral("/rest/v1/rpc/unpublish_catalog"));
	const HttpHeaders headers{{"apikey", m_apiKey.toLatin1()}, {"Authorization", "Bearer " + m_apiKey.toLatin1()}};
	const Transport transport = m_transport;
	{
		std::lock_guard<std::mutex> lock(m_worker->mutex);
		m_worker->jobs.push_back([transport, url, headers, body]() { transport(url, headers, body); });
	}
	m_worker->wake.notify_all();
	resetSent();
}

} // namespace acsl

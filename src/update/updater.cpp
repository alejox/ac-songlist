#include "updater.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace acsl {

namespace {

const qint64 kMaxPackage = 64LL * 1024 * 1024;
const QString kNew = QStringLiteral(".acsl-new");
const QString kOld = QStringLiteral(".acsl-old");

QString join(const QString &dir, const QString &rel)
{
	return QDir(dir).filePath(rel);
}

// Copia un arbol de carpetas conservando los permisos (el binario de macOS es ejecutable).
bool copyTree(const QString &from, const QString &to, QString *error)
{
	if (!QDir().mkpath(to)) {
		*error = QStringLiteral("no se pudo crear %1").arg(QDir::toNativeSeparators(to));
		return false;
	}
	QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		const QFileInfo info = it.fileInfo();
		const QString target = join(to, QDir(from).relativeFilePath(info.filePath()));
		if (info.isDir()) {
			if (!QDir().mkpath(target)) {
				*error = QStringLiteral("no se pudo crear %1").arg(QDir::toNativeSeparators(target));
				return false;
			}
			continue;
		}
		QDir().mkpath(QFileInfo(target).absolutePath());
		if (!QFile::copy(info.filePath(), target)) {
			*error = QStringLiteral("no se pudo escribir %1").arg(QDir::toNativeSeparators(target));
			return false;
		}
		QFile::setPermissions(target, info.permissions());
	}
	return true;
}

// Donde se prepara la carpeta nueva y se aparta la vieja al cambiarla entera: fuera de la
// carpeta de plugins (OBS cargaria cualquier copia que encontrase alli) pero en el mismo
// disco, para que el cambio sea un simple renombrado. Ej.: ~/.config/obs-studio/.ac-songlist
QString staging(const QString &root)
{
	const QDir plugins = QFileInfo(root).absoluteDir();
	QDir base = plugins;
	if (!base.cdUp())
		base = plugins;
	return base.filePath(QLatin1Char('.') + QFileInfo(root).fileName());
}

QStringList filesUnder(const QString &dir)
{
	QStringList out;
	QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext())
		out << QDir(dir).relativeFilePath(it.next());
	out.sort();
	return out;
}

} // namespace

// ------------------------------------------------------------------ piezas sueltas

int Updater::compareVersions(const QString &a, const QString &b)
{
	const auto parts = [](QString v) {
		v = v.trimmed();
		if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
			v = v.mid(1);
		QList<int> out;
		for (const QString &p : v.split(QLatin1Char('.'))) {
			int n = 0, i = 0;
			while (i < p.size() && p[i].isDigit() && n < 100000000)
				n = n * 10 + p[i++].digitValue();
			out << n;
		}
		return out;
	};
	const QList<int> x = parts(a), y = parts(b);
	for (int i = 0; i < qMax(x.size(), y.size()); i++) {
		const int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
		if (p != q)
			return p < q ? -1 : 1;
	}
	return 0;
}

QString Updater::assetName()
{
#if defined(_WIN32)
	return QStringLiteral("AC-Songlist-Windows.zip");
#elif defined(__APPLE__)
	return QStringLiteral("AC-Songlist-macOS.tar.gz");
#else
	return QStringLiteral("AC-Songlist-Linux.tar.gz");
#endif
}

QString Updater::payloadName()
{
#if defined(__APPLE__)
	return QStringLiteral("ac-songlist.plugin");
#else
	return QStringLiteral("ac-songlist");
#endif
}

QString Updater::binaryInPayload()
{
#if defined(_WIN32)
	return QStringLiteral("bin/64bit/ac-songlist.dll");
#elif defined(__APPLE__)
	return QStringLiteral("Contents/MacOS/ac-songlist");
#else
	return QStringLiteral("bin/64bit/ac-songlist.so");
#endif
}

Updater::Swap Updater::defaultSwap()
{
#if defined(_WIN32)
	return Swap::PerFile; // un .dll cargado se puede renombrar, pero no su carpeta
#else
	return Swap::WholeFolder;
#endif
}

QString Updater::rootForModule(const QString &modulePath)
{
	if (modulePath.isEmpty())
		return QString();
	const QString path = QDir::fromNativeSeparators(QFileInfo(modulePath).absoluteFilePath());
	const QString tail = QLatin1Char('/') + payloadName() + QLatin1Char('/') + binaryInPayload();
	if (!path.endsWith(tail, Qt::CaseInsensitive))
		return QString(); // instalado a mano en otro sitio: se avisa, pero no se toca
	return path.left(path.size() - binaryInPayload().size() - 1);
}

bool Updater::parseRelease(const QByteArray &json, const QString &asset, Release *out, QString *error)
{
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
	if (!doc.isObject()) {
		*error = QStringLiteral("respuesta no valida de GitHub");
		return false;
	}
	const QJsonObject o = doc.object();
	Release r;
	r.version = o.value(QStringLiteral("tag_name")).toString().trimmed();
	if (r.version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
		r.version = r.version.mid(1);
	r.pageUrl = o.value(QStringLiteral("html_url")).toString();
	if (r.version.isEmpty()) {
		*error = QStringLiteral("GitHub no dijo que version es la ultima");
		return false;
	}
	for (const QJsonValue &v : o.value(QStringLiteral("assets")).toArray()) {
		const QJsonObject a = v.toObject();
		if (a.value(QStringLiteral("name")).toString() != asset)
			continue;
		r.assetName = asset;
		r.assetUrl = a.value(QStringLiteral("browser_download_url")).toString();
		r.size = qint64(a.value(QStringLiteral("size")).toDouble());
		const QString digest = a.value(QStringLiteral("digest")).toString();
		if (digest.startsWith(QLatin1String("sha256:")))
			r.sha256 = digest.mid(7).trimmed().toLower().toLatin1();
	}
	*out = r; // aun sin paquete sirve para avisar de que hay version nueva
	if (r.assetUrl.isEmpty()) {
		*error = QStringLiteral("la version %1 no trae paquete para este sistema").arg(r.version);
		return false;
	}
	if (r.sha256.size() != 64) {
		*error = QStringLiteral("la version %1 no trae la huella del paquete").arg(r.version);
		return false;
	}
	return true;
}

bool Updater::extract(const QString &archive, const QString &dir, QString *error)
{
	QDir().mkpath(dir);
#if defined(_WIN32)
	// el tar de Windows (desde Windows 10) tambien abre .zip
	const QString tar = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32/tar.exe"));
#else
	const QString tar = QFile::exists(QStringLiteral("/usr/bin/tar")) ? QStringLiteral("/usr/bin/tar") : QStringLiteral("/bin/tar");
#endif
	if (!QFile::exists(tar)) {
		*error = QStringLiteral("este sistema no tiene tar para descomprimir");
		return false;
	}
	const bool zip = archive.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive);
	QProcess p;
	p.setProgram(tar);
	p.setArguments({zip ? QStringLiteral("-xf") : QStringLiteral("-xzf"), QDir::toNativeSeparators(archive), QStringLiteral("-C"),
			QDir::toNativeSeparators(dir)});
	p.start();
	if (!p.waitForStarted(15000) || !p.waitForFinished(120000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
		*error = QStringLiteral("no se pudo descomprimir: %1").arg(QString::fromLocal8Bit(p.readAllStandardError()).trimmed().left(200));
		p.kill();
		return false;
	}
	// nada puede quedar fuera de la carpeta ni ser un enlace
	const QString base = QFileInfo(dir).canonicalFilePath() + QLatin1Char('/');
	QDirIterator it(dir, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		const QFileInfo info = it.fileInfo();
		if (info.isSymLink() || !info.canonicalFilePath().startsWith(base)) {
			*error = QStringLiteral("el paquete trae rutas no permitidas");
			return false;
		}
	}
	return true;
}

bool Updater::apply(const QString &payload, const QString &root, Swap swap, QString *error)
{
	if (payload.isEmpty() || root.isEmpty() || !QFileInfo(payload).isDir()) {
		*error = QStringLiteral("no hay nada que instalar");
		return false;
	}

	if (swap == Swap::WholeFolder) {
		const QString fresh = staging(root) + kNew, old = staging(root) + kOld;
		QDir(fresh).removeRecursively();
		if (!copyTree(payload, fresh, error)) {
			QDir(fresh).removeRecursively();
			return false;
		}
		if (QFileInfo::exists(old) && !QDir(old).removeRecursively()) {
			QDir(fresh).removeRecursively();
			*error = QStringLiteral("no se pudo borrar %1").arg(QDir::toNativeSeparators(old));
			return false;
		}
		if (QFileInfo::exists(root) && !QDir().rename(root, old)) {
			QDir(fresh).removeRecursively();
			*error = QStringLiteral("no se pudo apartar la version actual");
			return false;
		}
		if (!QDir().rename(fresh, root)) {
			QDir().rename(old, root); // se deja todo como estaba
			QDir(fresh).removeRecursively();
			*error = QStringLiteral("no se pudo colocar la version nueva");
			return false;
		}
		return true;
	}

	// Archivo por archivo. Primero se escriben todos los nuevos a un lado (si falta permiso
	// falla aqui, sin haber tocado nada) y luego se cambian.
	const QStringList files = filesUnder(payload);
	QStringList written;
	const auto dropWritten = [&]() {
		for (const QString &rel : written)
			QFile::remove(join(root, rel) + kNew);
	};
	for (const QString &rel : files) {
		const QString target = join(root, rel);
		QDir().mkpath(QFileInfo(target).absolutePath());
		QFile::remove(target + kNew);
		if (!QFile::copy(join(payload, rel), target + kNew)) {
			dropWritten();
			*error = QStringLiteral("no se pudo escribir en %1 (falta permiso?)").arg(QDir::toNativeSeparators(QFileInfo(target).absolutePath()));
			return false;
		}
		QFile::setPermissions(target + kNew, QFileInfo(join(payload, rel)).permissions());
		written << rel;
	}

	struct Step {
		QString target, old;
	};
	QList<Step> done;
	const auto rollback = [&]() {
		for (int i = int(done.size()) - 1; i >= 0; i--) {
			QFile::remove(done[i].target);
			if (!done[i].old.isEmpty())
				QFile::rename(done[i].old, done[i].target);
		}
		dropWritten();
	};
	for (const QString &rel : files) {
		const QString target = join(root, rel);
		Step step{target, QString()};
		if (QFileInfo::exists(target)) {
			// el apartado de una actualizacion anterior puede seguir en uso: entonces otro nombre
			QString old = target + kOld;
			for (int n = 1; QFileInfo::exists(old) && !QFile::remove(old) && n < 100; n++)
				old = target + kOld + QString::number(n);
			if (!QFile::rename(target, old)) {
				rollback();
				*error = QStringLiteral("no se pudo apartar %1").arg(QDir::toNativeSeparators(target));
				return false;
			}
			step.old = old;
		}
		done << step;
		if (!QFile::rename(target + kNew, target)) {
			rollback();
			*error = QStringLiteral("no se pudo colocar %1").arg(QDir::toNativeSeparators(target));
			return false;
		}
	}
	return true;
}

void Updater::cleanup(const QString &root)
{
	if (root.isEmpty())
		return;
	QDir(staging(root) + kOld).removeRecursively();
	QDir(staging(root) + kNew).removeRecursively();
	if (!QFileInfo(root).isDir())
		return;
	QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString path = it.next();
		const QString name = it.fileName();
		if (name.endsWith(kNew) || name.contains(kOld))
			QFile::remove(path); // si sigue en uso, se borrara en el proximo arranque
	}
}

bool Updater::downloadAndApply(const Release &release, const QString &root, const Getter &get, Swap swap, QString *error)
{
	if (release.assetUrl.isEmpty() || release.sha256.size() != 64) {
		*error = QStringLiteral("no hay paquete para este sistema");
		return false;
	}
	const HttpResult r = get(QUrl(release.assetUrl), kMaxPackage);
	if (!r.sent || r.status != 200 || !r.error.isEmpty()) {
		*error = r.sent ? QStringLiteral("no se pudo descargar (HTTP %1)").arg(r.status)
				: QStringLiteral("no se pudo descargar: %1").arg(r.error);
		return false;
	}
	if ((release.size > 0 && r.body.size() != release.size) ||
	    QCryptographicHash::hash(r.body, QCryptographicHash::Sha256).toHex() != release.sha256) {
		*error = QStringLiteral("el archivo descargado no coincide con el publicado");
		return false;
	}

	QTemporaryDir tmp(QDir::tempPath() + QStringLiteral("/ac-songlist-update-XXXXXX"));
	if (!tmp.isValid()) {
		*error = QStringLiteral("no se pudo crear una carpeta temporal");
		return false;
	}
	const QString archive = tmp.filePath(release.assetName.isEmpty() ? assetName() : release.assetName);
	QFile file(archive);
	if (!file.open(QIODevice::WriteOnly) || file.write(r.body) != r.body.size()) {
		*error = QStringLiteral("no se pudo guardar la descarga");
		return false;
	}
	file.close();
	const QString unpacked = tmp.filePath(QStringLiteral("x"));
	if (!extract(archive, unpacked, error))
		return false;
	const QString payload = join(unpacked, payloadName());
	if (!QFileInfo::exists(join(payload, binaryInPayload()))) {
		*error = QStringLiteral("el paquete no trae el plugin");
		return false;
	}
	return apply(payload, root, swap, error);
}

// ------------------------------------------------------------------ el objeto

// Hilo de trabajo: las descargas nunca bloquean la interfaz de OBS.
struct Updater::Worker {
	std::mutex mutex;
	std::condition_variable wake;
	std::deque<std::function<void()>> jobs;
	bool stopping = false;
	std::atomic<bool> finished{false};
	std::thread thread;
};

Updater::Updater(const QString &currentVersion, QObject *parent) : QObject(parent), m_current(currentVersion)
{
	const QByteArray agent = "ac-songlist/" + currentVersion.toLatin1();
	m_get = [agent](const QUrl &url, qint64 maxBytes) {
		HttpHeaders headers{{"User-Agent", agent}};
		const bool api = maxBytes <= 1024 * 1024;
		if (api)
			headers.append(qMakePair(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/vnd.github+json")));
		return httpGet(url, headers, api ? 20000 : 180000, maxBytes);
	};
	m_timer.setSingleShot(true);
	connect(&m_timer, &QTimer::timeout, this, [this]() {
		check();
		if (m_every > 0)
			m_timer.start(m_every);
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

Updater::~Updater()
{
	std::shared_ptr<Worker> worker = m_worker;
	{
		std::lock_guard<std::mutex> lock(worker->mutex);
		worker->stopping = true;
		worker->jobs.clear();
	}
	worker->wake.notify_all();
	// una descarga en curso no retiene el cierre de OBS mas de unos segundos
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (!worker->finished && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	if (worker->finished)
		worker->thread.join();
	else
		worker->thread.detach();
}

void Updater::setApiUrl(const QString &url)
{
	m_apiUrl = url.trimmed();
}

void Updater::setInstallRoot(const QString &root)
{
	m_root = root;
}

void Updater::setAutoInstall(bool on)
{
	if (m_auto == on)
		return;
	m_auto = on;
	emit stateChanged();
	if (on && m_state == State::Available && canInstall())
		install();
}

void Updater::setGetter(Getter getter)
{
	m_get = std::move(getter);
}

void Updater::startSchedule(int firstDelayMs, int everyMs)
{
	m_every = everyMs;
	m_timer.start(qMax(0, firstDelayMs));
}

void Updater::setState(State state, const QString &message)
{
	if (m_state == state && m_message == message)
		return;
	m_state = state;
	m_message = message;
	emit stateChanged();
}

void Updater::run(std::function<void()> job)
{
	{
		std::lock_guard<std::mutex> lock(m_worker->mutex);
		m_worker->jobs.push_back(std::move(job));
	}
	m_worker->wake.notify_one();
}

void Updater::check()
{
	if (!enabled() || m_busy)
		return;
	m_busy = true;
	if (m_state != State::Installed)
		setState(State::Checking);
	const Getter get = m_get;
	const QUrl url(m_apiUrl);
	const QString asset = assetName();
	const QPointer<Updater> self(this);
	run([get, url, asset, self]() {
		const HttpResult r = get(url, 1024 * 1024);
		Release release;
		QString error;
		bool ok = false;
		if (!r.sent)
			error = QStringLiteral("sin conexion con GitHub");
		else if (r.status != 200)
			error = QStringLiteral("GitHub respondio HTTP %1").arg(r.status);
		else
			ok = parseRelease(r.body, asset, &release, &error);
		if (!QCoreApplication::instance())
			return;
		QMetaObject::invokeMethod(
			QCoreApplication::instance(),
			[self, ok, release, error]() {
				Updater *u = self.data();
				if (!u)
					return;
				u->m_busy = false;
				u->m_lastCheck = QDateTime::currentDateTime();
				const QString have = u->m_installed.isEmpty() ? u->m_current : u->m_installed;
				const bool newer = !release.version.isEmpty() && compareVersions(release.version, have) > 0;
				if (newer)
					u->m_latest = release;
				if (newer && ok && u->m_auto && u->canInstall()) {
					u->install();
				} else if (newer) {
					u->setState(State::Available, ok ? QString() : error); // sin paquete: solo se avisa
				} else if (!ok && release.version.isEmpty()) {
					if (u->m_state != State::Installed)
						u->setState(State::Error, error);
				} else if (u->m_state != State::Installed) {
					u->setState(State::UpToDate);
				} else {
					emit u->stateChanged(); // cambia la hora de la ultima comprobacion
				}
			},
			Qt::QueuedConnection);
	});
}

void Updater::install()
{
	if (m_busy || m_latest.version.isEmpty() || !canInstall())
		return;
	m_busy = true;
	setState(State::Installing);
	const Getter get = m_get;
	const Release release = m_latest;
	const QString root = m_root;
	const QPointer<Updater> self(this);
	run([get, release, root, self]() {
		QString error;
		const bool ok = downloadAndApply(release, root, get, defaultSwap(), &error);
		if (!QCoreApplication::instance())
			return;
		QMetaObject::invokeMethod(
			QCoreApplication::instance(),
			[self, ok, error, version = release.version]() {
				Updater *u = self.data();
				if (!u)
					return;
				u->m_busy = false;
				if (ok) {
					u->m_installed = version;
					u->setState(State::Installed);
				} else {
					u->setState(State::Error, error);
				}
			},
			Qt::QueuedConnection);
	});
}

} // namespace acsl

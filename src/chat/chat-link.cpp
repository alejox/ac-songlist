#include "chat-link.hpp"

#include <QCoreApplication>
#include <QTcpSocket>
#if QT_CONFIG(ssl)
#include <QSslSocket>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace acsl {

// ------------------------------------------------------------------ QtLink

QtLink::QtLink(Mode mode, const QString &host, quint16 port, QObject *parent)
	: ChatLink(parent), m_mode(mode), m_host(host), m_port(port)
{
}

QtLink::~QtLink()
{
	stop();
}

bool QtLink::tlsAvailable()
{
#if QT_CONFIG(ssl)
	return QSslSocket::supportsSsl();
#else
	return false;
#endif
}

void QtLink::start()
{
	stop();
#if QT_CONFIG(ssl)
	if (m_mode == Mode::Tls) {
		auto *ssl = new QSslSocket(this);
		m_socket = ssl;
		connect(ssl, &QSslSocket::encrypted, this, &ChatLink::opened);
		connect(ssl, &QAbstractSocket::readyRead, this, &QtLink::onReadyRead);
		connect(ssl, &QAbstractSocket::disconnected, this, &QtLink::onGone);
		connect(ssl, &QAbstractSocket::errorOccurred, this, &QtLink::onGone);
		ssl->connectToHostEncrypted(m_host, m_port);
		return;
	}
#endif
	m_socket = new QTcpSocket(this);
	connect(m_socket, &QAbstractSocket::connected, this, &ChatLink::opened);
	connect(m_socket, &QAbstractSocket::readyRead, this, &QtLink::onReadyRead);
	connect(m_socket, &QAbstractSocket::disconnected, this, &QtLink::onGone);
	connect(m_socket, &QAbstractSocket::errorOccurred, this, &QtLink::onGone);
	m_socket->connectToHost(m_host, m_port);
}

void QtLink::stop()
{
	m_buffer.clear();
	if (!m_socket)
		return;
	m_socket->disconnect(this);
	m_socket->abort();
	m_socket->deleteLater();
	m_socket = nullptr;
}

void QtLink::sendLine(const QString &line)
{
	if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
		m_socket->write(line.toUtf8() + "\r\n");
}

void QtLink::onGone()
{
	if (!m_socket)
		return;
	stop();
	emit closed();
}

void QtLink::onReadyRead()
{
	if (!m_socket)
		return;
	m_buffer.append(m_socket->readAll());
	if (m_buffer.size() > 1024 * 1024) { // nunca deberia pasar
		m_buffer.clear();
		return;
	}
	QPointer<QtLink> self(this);
	int nl;
	while ((nl = m_buffer.indexOf('\n')) >= 0) {
		QByteArray raw = m_buffer.left(nl);
		m_buffer.remove(0, nl + 1);
		if (raw.endsWith('\r'))
			raw.chop(1);
		if (!raw.isEmpty())
			emit lineReceived(QString::fromUtf8(raw));
		if (!self || !m_socket)
			return; // quien recibio la linea cerro el enlace
	}
}

// -------------------------------------------------------------- NativeLink

struct NativeLink::Core {
	std::unique_ptr<NativeConn> conn;
	std::mutex mutex;
	std::condition_variable wake;
	std::deque<std::string> outbox;
	bool ready = false;   // open() termino bien
	bool stopping = false;
	std::atomic<int> threads{0};
	std::thread reader, writer;
};

namespace {
// Entrega algo al hilo de la interfaz; se descarta si el enlace ya no existe.
template <typename Fn> void post(const QPointer<NativeLink> &link, Fn fn)
{
	QCoreApplication *app = QCoreApplication::instance();
	if (!app)
		return;
	QMetaObject::invokeMethod(
		app,
		[link, fn]() {
			if (link)
				fn(link.data());
		},
		Qt::QueuedConnection);
}
} // namespace

NativeLink::NativeLink(Factory factory, QObject *parent) : ChatLink(parent), m_factory(std::move(factory)) {}

NativeLink::~NativeLink()
{
	stop();
}

void NativeLink::start()
{
	stop();
	auto core = std::make_shared<Core>();
	core->conn = m_factory();
	m_core = core;
	const QPointer<NativeLink> self(this);
	if (!core->conn) {
		post(self, [](NativeLink *link) { emit link->closed(); });
		return;
	}

	core->threads = 2;
	core->reader = std::thread([core, self]() {
		std::string error;
		if (core->conn->open(&error)) {
			{
				std::lock_guard<std::mutex> lock(core->mutex);
				core->ready = true;
			}
			core->wake.notify_all();
			post(self, [](NativeLink *link) { emit link->opened(); });
			std::string line;
			while (core->conn->readLine(&line)) {
				const QString text = QString::fromUtf8(line.data(), qsizetype(line.size()));
				post(self, [text](NativeLink *link) { emit link->lineReceived(text); });
			}
		}
		bool wasStopping;
		{
			std::lock_guard<std::mutex> lock(core->mutex);
			wasStopping = core->stopping;
			core->stopping = true; // el hilo que escribe tambien termina
		}
		core->wake.notify_all();
		if (!wasStopping)
			post(self, [](NativeLink *link) { emit link->closed(); });
		core->threads--;
	});
	core->writer = std::thread([core]() {
		for (;;) {
			std::string line;
			{
				std::unique_lock<std::mutex> lock(core->mutex);
				core->wake.wait(lock, [&]() { return core->stopping || (core->ready && !core->outbox.empty()); });
				if (core->stopping)
					break;
				line = std::move(core->outbox.front());
				core->outbox.pop_front();
			}
			if (!core->conn->sendLine(line)) {
				core->conn->cancel(); // la lectura fallara y avisara del cierre
				break;
			}
		}
		core->threads--;
	});
}

void NativeLink::stop()
{
	std::shared_ptr<Core> core = std::move(m_core);
	m_core.reset();
	if (!core)
		return;
	{
		std::lock_guard<std::mutex> lock(core->mutex);
		core->stopping = true;
	}
	if (core->conn)
		core->conn->cancel(); // desbloquea open(), readLine() y sendLine()
	core->wake.notify_all();

	// Lo normal es que los hilos terminen en milisegundos. Si la red del sistema tardara
	// en soltar una llamada, no se congela OBS: se dejan terminar solos.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (core->threads.load() > 0 && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	const bool finished = core->threads.load() == 0;
	for (std::thread *t : {&core->reader, &core->writer}) {
		if (!t->joinable())
			continue;
		if (finished)
			t->join();
		else
			t->detach();
	}
}

void NativeLink::sendLine(const QString &line)
{
	if (!m_core)
		return;
	{
		std::lock_guard<std::mutex> lock(m_core->mutex);
		if (m_core->stopping || m_core->outbox.size() > 200)
			return;
		m_core->outbox.push_back(line.toStdString());
	}
	m_core->wake.notify_all();
}

} // namespace acsl

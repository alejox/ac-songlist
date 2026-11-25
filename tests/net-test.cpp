// Prueba de conexion real con el chat de Twitch. No forma parte de las pruebas normales
// (necesita internet): la ejecuta el flujo de GitHub en Windows, macOS y Linux para comprobar
// que la conexion segura funciona de verdad en cada sistema. Solo lee; nunca escribe en un chat.
#include "chat/chat-link.hpp"
#include "chat/twitch-chat.hpp"
#include "net/https.hpp"
#include "web/web-config.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QThread>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace acsl;

static int g_failed = 0;

static void report(const char *name, bool ok, const std::string &detail = {})
{
	std::printf("[%s] %s%s%s\n", ok ? " OK  " : "FALLO", name, detail.empty() ? "" : " - ", detail.c_str());
	std::fflush(stdout);
	if (!ok)
		g_failed++;
}

// Corta la conexion si la prueba tarda demasiado, para que nunca se quede colgada.
struct Watchdog {
	Watchdog(NativeConn *conn, int seconds)
		: thread([this, conn, seconds]() {
			  for (int i = 0; i < seconds * 20 && !done; i++)
				  std::this_thread::sleep_for(std::chrono::milliseconds(50));
			  if (!done) {
				  fired = true;
				  conn->cancel();
			  }
		  })
	{
	}
	~Watchdog()
	{
		done = true;
		thread.join();
	}
	std::atomic<bool> done{false}, fired{false};
	std::thread thread;
};

static std::string anonymousNick()
{
	return "justinfan" + std::to_string(QRandomGenerator::global()->bounded(10000, 99999));
}

static bool readUntil(NativeConn *conn, const char *needle, std::string *found, int maxLines = 40)
{
	std::string line;
	for (int i = 0; i < maxLines && conn->readLine(&line); i++) {
		if (line.find(needle) != std::string::npos) {
			*found = line;
			return true;
		}
	}
	return false;
}

static void testNativeTransport()
{
	if (!NativeConn::available()) {
		std::printf("[ --- ] Esta plataforma no usa transporte nativo (Qt ya trae TLS)\n");
		return;
	}

	// 1. sesion anonima cifrada
	std::unique_ptr<NativeConn> conn = NativeConn::createTwitch();
	std::string error, line;
	bool welcomed = false;
	{
		Watchdog guard(conn.get(), 25);
		const bool opened = conn->open(&error);
		report("conexion segura con la red del sistema", opened, error);
		if (!opened)
			return;
		conn->sendLine("NICK " + anonymousNick());
		welcomed = readUntil(conn.get(), " 001 ", &line);
		report("Twitch acepta la sesion anonima", welcomed, guard.fired ? "tiempo agotado" : line.substr(0, 60));
	}
	if (!welcomed)
		return;

	// 2. escribir mientras otro hilo esta bloqueado leyendo (asi trabaja el plugin)
	std::atomic<bool> pong{false}, readerDone{false};
	std::thread reader([&]() {
		std::string l;
		while (conn->readLine(&l))
			if (l.find("PONG") != std::string::npos && l.find("acsl-prueba") != std::string::npos)
				pong = true;
		readerDone = true;
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(700)); // el lector ya esta esperando datos
	const bool sent = conn->sendLine("PING :acsl-prueba");
	for (int i = 0; i < 200 && !pong; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	report("enviar mientras otro hilo lee", sent && pong, sent ? (pong ? "PONG recibido" : "sin respuesta") : "el envio fallo");

	// 3. cancelar desbloquea al lector enseguida
	QElapsedTimer timer;
	timer.start();
	conn->cancel();
	for (int i = 0; i < 200 && !readerDone; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	report("cancelar desbloquea la lectura", readerDone, std::to_string(timer.elapsed()) + " ms");
	if (readerDone)
		reader.join();
	else
		reader.detach();
	conn.reset();

	// 4. un token invalido se detecta (no se usa ninguna cuenta real)
	std::unique_ptr<NativeConn> bad = NativeConn::createTwitch();
	{
		Watchdog guard(bad.get(), 25);
		if (bad->open(&error)) {
			bad->sendLine("PASS oauth:abcdefghijklmnopqrstuvwxyz1234");
			bad->sendLine("NICK acsl_prueba");
			const bool rejected = readUntil(bad.get(), "NOTICE", &line, 10);
			report("Twitch rechaza un token invalido", rejected, line.substr(0, 80));
		} else {
			report("segunda conexion", false, error);
		}
	}
}

static bool waitFor(const std::function<bool()> &condition, int ms)
{
	QElapsedTimer timer;
	timer.start();
	while (timer.elapsed() < ms) {
		QCoreApplication::processEvents();
		if (condition())
			return true;
		QThread::msleep(5);
	}
	return condition();
}

// El cliente completo, igual que dentro de OBS: conectar, identificarse y entrar a un canal.
static void testChatClient(const char *label)
{
	TwitchChat chat;
	chat.configure("twitch", "", "");
	const bool connected = waitFor([&]() { return chat.state() == TwitchChat::State::Connected; }, 30000);
	const std::string detail = "transporte: " + chat.transport().toStdString() + (chat.secure() ? ", cifrado" : ", SIN cifrar");
	report(label, connected && chat.secure(), detail);
	chat.stop();
}

// Informativo: el acceso antiguo sin cifrar, que es lo unico que podia usar la version 1.0.0
// en Windows y macOS.
static void probePlain()
{
	QTcpSocket socket;
	socket.connectToHost(QStringLiteral("irc.chat.twitch.tv"), 6667);
	if (!socket.waitForConnected(8000)) {
		std::printf("[ --- ] acceso sin cifrar (puerto 6667): no conecta (%s)\n", qPrintable(socket.errorString()));
		return;
	}
	socket.write(("NICK " + anonymousNick() + "\r\n").c_str());
	QByteArray data;
	QElapsedTimer timer;
	timer.start();
	while (timer.elapsed() < 8000 && !data.contains(" 001 ")) {
		if (socket.waitForReadyRead(500))
			data += socket.readAll();
		else if (socket.state() != QAbstractSocket::ConnectedState)
			break;
	}
	std::printf("[ --- ] acceso sin cifrar (puerto 6667): conecta, %s\n",
		    data.contains(" 001 ") ? "y Twitch SI acepta la sesion" : "pero Twitch NO responde a la sesion");
}

// HTTPS con la red del sistema, contra el servicio de la pagina web. Sin clave valida el
// servicio contesta con un error, y eso basta: demuestra que la conexion segura funciona.
static void testHttps()
{
	const QUrl url(QStringLiteral(ACSL_WEB_API_URL) + QStringLiteral("/rest/v1/rpc/publish_catalog"));
	const QByteArray key = QByteArray(ACSL_WEB_API_KEY).isEmpty() ? QByteArray("sin-clave") : QByteArray(ACSL_WEB_API_KEY);
	const HttpResult r = httpPostJson(url, {{"apikey", key}}, "{\"p_channel\":\"x\",\"p_key\":\"corta\"}");
	report("HTTPS con la red del sistema", r.sent,
	       r.sent ? "respuesta HTTP " + std::to_string(r.status) + " " + r.body.left(90).toStdString() : r.error.toStdString());
	if (!QByteArray(ACSL_WEB_API_KEY).isEmpty())
		// con la clave publica, una peticion mal formada debe ser rechazada por el servicio (400), no por falta de permiso
		report("el servicio valida lo que recibe", r.status == 400, "HTTP " + std::to_string(r.status));
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	std::printf("TLS de Qt disponible: %s | transporte nativo disponible: %s\n", QtLink::tlsAvailable() ? "si" : "no",
		    NativeConn::available() ? "si" : "no");

	testNativeTransport();
	testChatClient("cliente de chat (configuracion por defecto)");
	if (NativeConn::available()) {
		// Asi es dentro de OBS en Windows y macOS: su Qt no trae TLS.
		qputenv("ACSL_NO_QT_TLS", "1");
		testChatClient("cliente de chat como dentro de OBS (sin TLS de Qt)");
		qunsetenv("ACSL_NO_QT_TLS");
	}
	probePlain();
	std::printf("HTTPS disponible: %s\n", httpsAvailable() ? "si" : "no");
	testHttps();

	std::printf("%s\n", g_failed ? "RESULTADO: hay fallos" : "RESULTADO: todo correcto");
	return g_failed ? 1 : 0;
}

// Windows: WebSocket seguro con WinHTTP (wss://irc-ws.chat.twitch.tv). El protocolo que
// viaja dentro es el mismo IRC de siempre, una linea por mensaje.
#include "native-conn.hpp"

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <mutex>

namespace acsl {

namespace {

class WinHttpConn : public NativeConn {
public:
	~WinHttpConn() override { cancel(); }

	bool open(std::string *error) override
	{
		HINTERNET session = WinHttpOpen(L"ac-songlist", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
						WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!keep(&m_session, session))
			return fail(error, "WinHttpOpen");
		// Sin limite de espera al leer: en un chat tranquilo pueden pasar minutos sin datos.
		// Quien corta una conexion colgada es cancel(), llamado desde fuera.
		WinHttpSetTimeouts(session, 15000, 15000, 15000, 0);

		HINTERNET connect = WinHttpConnect(session, L"irc-ws.chat.twitch.tv", INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!keep(&m_connect, connect))
			return fail(error, "WinHttpConnect");

		HINTERNET request = WinHttpOpenRequest(connect, L"GET", L"/", nullptr, WINHTTP_NO_REFERER,
						       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!keep(&m_request, request))
			return fail(error, "WinHttpOpenRequest");

		if (!WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0))
			return fail(error, "WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET");
		if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
			return fail(error, "WinHttpSendRequest");
		if (!WinHttpReceiveResponse(request, nullptr))
			return fail(error, "WinHttpReceiveResponse");

		HINTERNET socket = WinHttpWebSocketCompleteUpgrade(request, 0);
		if (!keep(&m_socket, socket))
			return fail(error, "WinHttpWebSocketCompleteUpgrade");
		return true;
	}

	bool sendLine(const std::string &line) override
	{
		std::lock_guard<std::mutex> sending(m_sendMutex); // un envio a la vez
		const HINTERNET socket = current();
		if (!socket)
			return false;
		return WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
					    const_cast<char *>(line.data()), DWORD(line.size())) == NO_ERROR;
	}

	bool readLine(std::string *line) override
	{
		for (;;) {
			const size_t nl = m_in.find('\n');
			if (nl != std::string::npos) {
				size_t end = nl;
				if (end > 0 && m_in[end - 1] == '\r')
					end--;
				line->assign(m_in, 0, end);
				m_in.erase(0, nl + 1);
				if (line->empty())
					continue;
				return true;
			}
			const HINTERNET socket = current();
			if (!socket)
				return false;
			char buffer[4096];
			DWORD read = 0;
			WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
			if (WinHttpWebSocketReceive(socket, buffer, sizeof(buffer), &read, &type) != NO_ERROR)
				return false;
			if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
				return false;
			m_in.append(buffer, read);
			// Fin de un mensaje WebSocket: si no acababa en salto de linea, se cierra la linea aqui.
			const bool endOfMessage = type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
						  type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
			if (endOfMessage && !m_in.empty() && m_in.back() != '\n')
				m_in.push_back('\n');
			if (m_in.size() > 1024 * 1024)
				return false;
		}
	}

	void cancel() override
	{
		HINTERNET handles[4];
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_cancelled = true;
			handles[0] = m_socket;
			handles[1] = m_request;
			handles[2] = m_connect;
			handles[3] = m_session;
			m_socket = m_request = m_connect = m_session = nullptr;
		}
		// Cerrar los manejadores hace que las llamadas bloqueadas en otros hilos terminen.
		for (HINTERNET h : handles)
			if (h)
				WinHttpCloseHandle(h);
	}

private:
	// Guarda el manejador para que cancel() pueda cerrarlo. Devuelve false si no es valido
	// o si ya se cancelo mientras tanto.
	bool keep(HINTERNET *slot, HINTERNET handle)
	{
		if (!handle)
			return false;
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_cancelled) {
			WinHttpCloseHandle(handle);
			return false;
		}
		*slot = handle;
		return true;
	}

	HINTERNET current()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_socket;
	}

	bool fail(std::string *error, const char *what)
	{
		if (error)
			*error = std::string(what) + " (" + std::to_string(GetLastError()) + ")";
		cancel();
		return false;
	}

	std::mutex m_mutex, m_sendMutex;
	bool m_cancelled = false;
	HINTERNET m_session = nullptr, m_connect = nullptr, m_request = nullptr, m_socket = nullptr;
	std::string m_in; // solo lo toca el hilo que lee
};

} // namespace

std::unique_ptr<NativeConn> NativeConn::createTwitch()
{
	return std::make_unique<WinHttpConn>();
}

bool NativeConn::available()
{
	return true;
}

std::unique_ptr<NativeConn> NativeConn::createLocalTest(int)
{
	return nullptr; // el servidor simulado solo se usa en las pruebas de Linux y macOS
}

} // namespace acsl

#endif // _WIN32

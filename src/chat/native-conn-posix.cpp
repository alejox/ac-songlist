// Linux (y pruebas en macOS): aqui Qt ya trae TLS, asi que no hace falta transporte nativo.
// Lo unico que hay es la conexion local sin cifrar que usan las pruebas automaticas.
#include "native-conn.hpp"

#ifndef _WIN32

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <mutex>

namespace acsl {

namespace {

class PosixTestConn : public NativeConn {
public:
	explicit PosixTestConn(int port) : m_port(port) {}
	~PosixTestConn() override
	{
		cancel();
		const int fd = m_fd.exchange(-1);
		if (fd >= 0)
			::close(fd);
	}

	bool open(std::string *error) override
	{
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			return false;
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(uint16_t(m_port));
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
			::close(fd);
			if (error)
				*error = "connect";
			return false;
		}
		m_fd = fd;
		if (m_cancelled)
			::shutdown(fd, SHUT_RDWR);
		return !m_cancelled;
	}

	bool sendLine(const std::string &line) override
	{
		std::lock_guard<std::mutex> sending(m_sendMutex);
		const int fd = m_fd;
		if (fd < 0 || m_cancelled)
			return false;
		const std::string bytes = line + "\r\n";
		size_t sent = 0;
		while (sent < bytes.size()) {
#ifdef MSG_NOSIGNAL
			const ssize_t n = ::send(fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
#else
			const ssize_t n = ::send(fd, bytes.data() + sent, bytes.size() - sent, 0);
#endif
			if (n <= 0)
				return false;
			sent += size_t(n);
		}
		return true;
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
			const int fd = m_fd;
			if (fd < 0 || m_cancelled)
				return false;
			char buffer[4096];
			const ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);
			if (n <= 0)
				return false;
			m_in.append(buffer, size_t(n));
		}
	}

	void cancel() override
	{
		m_cancelled = true;
		const int fd = m_fd;
		if (fd >= 0)
			::shutdown(fd, SHUT_RDWR); // despierta al hilo bloqueado en recv()
	}

private:
	int m_port;
	std::atomic<int> m_fd{-1};
	std::atomic<bool> m_cancelled{false};
	std::mutex m_sendMutex;
	std::string m_in;
};

} // namespace

std::unique_ptr<NativeConn> makePosixTestConn(int port)
{
	return std::make_unique<PosixTestConn>(port);
}

#ifndef __APPLE__
std::unique_ptr<NativeConn> NativeConn::createTwitch()
{
	return nullptr;
}

bool NativeConn::available()
{
	return false;
}

std::unique_ptr<NativeConn> NativeConn::createLocalTest(int port)
{
	return makePosixTestConn(port);
}
#endif

} // namespace acsl

#endif // !_WIN32

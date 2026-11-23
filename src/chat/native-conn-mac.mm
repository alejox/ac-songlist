// macOS: TLS con Network.framework, directo a irc.chat.twitch.tv:6697.
// Este archivo se compila con ARC (-fobjc-arc): los objetos nw_* y dispatch_* se liberan solos.
#include "native-conn.hpp"

#import <Network/Network.h>
#import <dispatch/dispatch.h>

#include <atomic>
#include <mutex>

namespace acsl {

std::unique_ptr<NativeConn> makePosixTestConn(int port);

namespace {

class NetworkConn : public NativeConn {
public:
	~NetworkConn() override { cancel(); }

	bool open(std::string *error) override
	{
		nw_endpoint_t endpoint = nw_endpoint_create_host("irc.chat.twitch.tv", "6697");
		nw_parameters_t parameters = nw_parameters_create_secure_tcp(NW_PARAMETERS_DEFAULT_CONFIGURATION,
									     NW_PARAMETERS_DEFAULT_CONFIGURATION);
		nw_connection_t connection = nw_connection_create(endpoint, parameters);
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_cancelled || !connection)
				return fail(error, "nw_connection_create");
			m_connection = connection;
		}

		dispatch_semaphore_t ready = dispatch_semaphore_create(0);
		auto state = std::make_shared<std::atomic<int>>(0); // 0 esperando, 1 lista, 2 fallo
		nw_connection_set_queue(connection, dispatch_queue_create("ac-songlist.chat", DISPATCH_QUEUE_SERIAL));
		nw_connection_set_state_changed_handler(connection, ^(nw_connection_state_t now, nw_error_t) {
			int expected = 0;
			if (now == nw_connection_state_ready) {
				if (state->compare_exchange_strong(expected, 1))
					dispatch_semaphore_signal(ready);
			} else if (now == nw_connection_state_failed || now == nw_connection_state_cancelled) {
				if (state->compare_exchange_strong(expected, 2))
					dispatch_semaphore_signal(ready);
			}
		});
		nw_connection_start(connection);

		const bool signalled =
			dispatch_semaphore_wait(ready, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)) == 0;
		if (!signalled || state->load() != 1) {
			cancel();
			return fail(error, signalled ? "no se pudo conectar" : "tiempo de espera agotado");
		}
		return true;
	}

	bool sendLine(const std::string &line) override
	{
		std::lock_guard<std::mutex> sending(m_sendMutex);
		nw_connection_t connection = current();
		if (!connection)
			return false;
		const std::string bytes = line + "\r\n";
		dispatch_data_t data = dispatch_data_create(bytes.data(), bytes.size(), nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
		dispatch_semaphore_t done = dispatch_semaphore_create(0);
		auto ok = std::make_shared<std::atomic<bool>>(false);
		nw_connection_send(connection, data, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, ^(nw_error_t sendError) {
			ok->store(sendError == nil);
			dispatch_semaphore_signal(done);
		});
		if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC)) != 0)
			return false;
		return ok->load();
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
			nw_connection_t connection = current();
			if (!connection)
				return false;

			dispatch_semaphore_t done = dispatch_semaphore_create(0);
			auto chunk = std::make_shared<std::string>();
			auto alive = std::make_shared<std::atomic<bool>>(true);
			nw_connection_receive(connection, 1, 16384,
					      ^(dispatch_data_t content, nw_content_context_t, bool isComplete, nw_error_t receiveError) {
						      if (content) {
							      dispatch_data_apply(content, ^bool(dispatch_data_t, size_t, const void *buffer, size_t size) {
								      chunk->append(static_cast<const char *>(buffer), size);
								      return true;
							      });
						      }
						      if (receiveError != nil || (isComplete && chunk->empty()))
							      alive->store(false);
						      dispatch_semaphore_signal(done);
					      });
			// Sin limite: cancel() hace que la lectura pendiente termine con error.
			dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
			m_in.append(*chunk);
			if (!alive->load() && chunk->empty())
				return false;
			if (m_in.size() > 1024 * 1024)
				return false;
		}
	}

	void cancel() override
	{
		nw_connection_t connection;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_cancelled)
				return;
			m_cancelled = true;
			connection = m_connection;
		}
		if (connection)
			nw_connection_cancel(connection);
	}

private:
	nw_connection_t current()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_cancelled ? nil : m_connection;
	}

	bool fail(std::string *error, const char *what)
	{
		if (error)
			*error = what;
		return false;
	}

	std::mutex m_mutex, m_sendMutex;
	bool m_cancelled = false;
	nw_connection_t m_connection = nil;
	std::string m_in; // solo lo toca el hilo que lee
};

} // namespace

std::unique_ptr<NativeConn> NativeConn::createTwitch()
{
	return std::make_unique<NetworkConn>();
}

bool NativeConn::available()
{
	return true;
}

std::unique_ptr<NativeConn> NativeConn::createLocalTest(int port)
{
	return makePosixTestConn(port);
}

} // namespace acsl

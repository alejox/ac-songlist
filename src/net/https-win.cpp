// Windows: WinHTTP.
#include "https.hpp"

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>

#include <string>

namespace acsl {

namespace {
struct Handle {
	HINTERNET h = nullptr;
	~Handle()
	{
		if (h)
			WinHttpCloseHandle(h);
	}
};

QString lastError(const char *what)
{
	return QStringLiteral("%1 (%2)").arg(QString::fromLatin1(what)).arg(GetLastError());
}
} // namespace

HttpResult httpPostJson(const QUrl &url, const HttpHeaders &headers, const QByteArray &body, int timeoutMs)
{
	HttpResult result;
	const bool secure = url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0;
	const std::wstring host = url.host().toStdWString();
	const INTERNET_PORT port = INTERNET_PORT(url.port(secure ? 443 : 80));
	QString target = url.path(QUrl::FullyEncoded);
	if (target.isEmpty())
		target = QStringLiteral("/");
	if (url.hasQuery())
		target += QLatin1Char('?') + url.query(QUrl::FullyEncoded);
	const std::wstring path = target.toStdWString();

	Handle session, connection, request;
	session.h = WinHttpOpen(L"ac-songlist", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session.h) {
		result.error = lastError("WinHttpOpen");
		return result;
	}
	WinHttpSetTimeouts(session.h, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
	connection.h = WinHttpConnect(session.h, host.c_str(), port, 0);
	if (!connection.h) {
		result.error = lastError("WinHttpConnect");
		return result;
	}
	request.h = WinHttpOpenRequest(connection.h, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
				       secure ? WINHTTP_FLAG_SECURE : 0);
	if (!request.h) {
		result.error = lastError("WinHttpOpenRequest");
		return result;
	}

	QString headerText = QStringLiteral("Content-Type: application/json\r\n");
	for (const auto &h : headers)
		headerText += QString::fromLatin1(h.first) + QStringLiteral(": ") + QString::fromLatin1(h.second) + QStringLiteral("\r\n");
	const std::wstring headerW = headerText.toStdWString();

	if (!WinHttpSendRequest(request.h, headerW.c_str(), DWORD(-1L), const_cast<char *>(body.constData()), DWORD(body.size()),
				DWORD(body.size()), 0)) {
		result.error = lastError("WinHttpSendRequest");
		return result;
	}
	if (!WinHttpReceiveResponse(request.h, nullptr)) {
		result.error = lastError("WinHttpReceiveResponse");
		return result;
	}

	DWORD status = 0, size = sizeof(status);
	WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
			    WINHTTP_NO_HEADER_INDEX);
	result.status = int(status);
	result.sent = status > 0;

	for (;;) {
		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(request.h, &available) || available == 0)
			break;
		char buffer[8192];
		DWORD read = 0;
		if (!WinHttpReadData(request.h, buffer, available < sizeof(buffer) ? available : DWORD(sizeof(buffer)), &read) || read == 0)
			break;
		result.body.append(buffer, int(read));
		if (result.body.size() > 1024 * 1024)
			break;
	}
	return result;
}

bool httpsAvailable()
{
	return true;
}

} // namespace acsl

#endif // _WIN32

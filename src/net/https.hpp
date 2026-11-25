// Peticiones HTTPS sencillas (POST con JSON y GET) usando la red de cada sistema: WinHTTP en
// Windows, NSURLSession en macOS y Qt en Linux. El Qt que trae OBS en Windows y macOS no incluye TLS.
#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

namespace acsl {

struct HttpResult {
	bool sent = false; // hubo respuesta del servidor (aunque sea un error)
	int status = 0;
	QByteArray body;
	QString error; // por que no hubo respuesta
};

using HttpHeaders = QList<QPair<QByteArray, QByteArray>>;

// Bloquean hasta tener respuesta o agotar el tiempo: llamar siempre desde un hilo de trabajo.
HttpResult httpPostJson(const QUrl &url, const HttpHeaders &headers, const QByteArray &body, int timeoutMs = 15000);
// GET que sigue redirecciones (las descargas de GitHub pasan por otra direccion). El cuerpo se
// corta en maxBytes; si se corto, error lo dice.
HttpResult httpGet(const QUrl &url, const HttpHeaders &headers, int timeoutMs = 15000, qint64 maxBytes = 1024 * 1024);

namespace detail {
HttpResult httpRequest(const char *method, const QUrl &url, const HttpHeaders &headers, const QByteArray &body,
		       int timeoutMs, qint64 maxBytes);
}

inline HttpResult httpPostJson(const QUrl &url, const HttpHeaders &headers, const QByteArray &body, int timeoutMs)
{
	HttpHeaders h = headers;
	h.prepend({QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json")});
	return detail::httpRequest("POST", url, h, body, timeoutMs, 1024 * 1024);
}

inline HttpResult httpGet(const QUrl &url, const HttpHeaders &headers, int timeoutMs, qint64 maxBytes)
{
	return detail::httpRequest("GET", url, headers, QByteArray(), timeoutMs, maxBytes);
}

// false si en esta instalacion no hay forma de hacer HTTPS.
bool httpsAvailable();

} // namespace acsl

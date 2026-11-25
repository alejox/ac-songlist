// Peticion HTTPS sencilla (POST con JSON) usando la red de cada sistema: WinHTTP en Windows,
// NSURLSession en macOS y Qt en Linux. El Qt que trae OBS en Windows y macOS no incluye TLS.
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

// Bloquea hasta tener respuesta o agotar el tiempo: llamar siempre desde un hilo de trabajo.
HttpResult httpPostJson(const QUrl &url, const HttpHeaders &headers, const QByteArray &body, int timeoutMs = 15000);

// false si en esta instalacion no hay forma de hacer HTTPS.
bool httpsAvailable();

} // namespace acsl

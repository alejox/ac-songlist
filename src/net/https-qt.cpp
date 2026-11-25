// Linux: Qt trae TLS, asi que se usa su propio cliente HTTP.
#include "https.hpp"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#if QT_CONFIG(ssl)
#include <QSslSocket>
#endif

namespace acsl {

namespace detail {

HttpResult httpRequest(const char *method, const QUrl &url, const HttpHeaders &headers, const QByteArray &body, int timeoutMs,
		       qint64 maxBytes)
{
	HttpResult result;
	QNetworkAccessManager manager; // vive en el hilo que llama, con su propio bucle de eventos
	QNetworkRequest request(url);
	for (const auto &h : headers)
		request.setRawHeader(h.first, h.second);
	request.setTransferTimeout(timeoutMs);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setMaximumRedirectsAllowed(5);

	QEventLoop loop;
	QNetworkReply *reply = qstrcmp(method, "GET") == 0 ? manager.get(request) : manager.sendCustomRequest(request, method, body);
	// se corta si se pasa del tamano maximo
	QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [reply, maxBytes](qint64 received, qint64) {
		if (received > maxBytes)
			reply->abort();
	});
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	result.sent = result.status > 0;
	result.body = reply->read(maxBytes + 1);
	if (result.body.size() > maxBytes) {
		result.body.truncate(int(maxBytes));
		result.error = QStringLiteral("respuesta demasiado grande");
	} else if (reply->error() == QNetworkReply::OperationCanceledError) {
		result.error = QStringLiteral("respuesta demasiado grande");
	} else if (!result.sent) {
		result.error = reply->errorString();
	}
	delete reply;
	return result;
}

} // namespace detail

bool httpsAvailable()
{
#if QT_CONFIG(ssl)
	return QSslSocket::supportsSsl();
#else
	return false;
#endif
}

} // namespace acsl

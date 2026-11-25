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

HttpResult httpPostJson(const QUrl &url, const HttpHeaders &headers, const QByteArray &body, int timeoutMs)
{
	HttpResult result;
	QNetworkAccessManager manager; // vive en el hilo que llama, con su propio bucle de eventos
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
	for (const auto &h : headers)
		request.setRawHeader(h.first, h.second);
	request.setTransferTimeout(timeoutMs);

	QEventLoop loop;
	QNetworkReply *reply = manager.post(request, body);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	result.sent = result.status > 0;
	result.body = reply->read(1024 * 1024);
	if (!result.sent)
		result.error = reply->errorString();
	delete reply;
	return result;
}

bool httpsAvailable()
{
#if QT_CONFIG(ssl)
	return QSslSocket::supportsSsl();
#else
	return false;
#endif
}

} // namespace acsl

// Publica la lista de canciones (y lo que suena / la cola) en el servicio web, para que
// la pagina publica del canal se mantenga al dia sola. Vive en el hilo de la interfaz;
// las peticiones salen desde un hilo propio.
#pragma once

#include "../core/store.hpp"
#include "../net/https.hpp"

#include <QDateTime>
#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>

namespace acsl {

class Publisher : public QObject {
	Q_OBJECT
public:
	enum class State { Off, Pending, Published, Error };
	using Transport = std::function<HttpResult(const QUrl &, const HttpHeaders &, const QByteArray &)>;

	explicit Publisher(Store *store, QObject *parent = nullptr);
	~Publisher() override;

	// Direccion del servicio, su clave publica y la direccion de la pagina.
	void setEndpoint(const QString &apiUrl, const QString &apiKey, const QString &pageUrl);
	void setTransport(Transport transport); // para pruebas
	void setDelays(int debounceMs, int retryMs);

	bool available() const;  // hay servicio configurado y forma de hablar con el
	bool active() const;     // ademas, el usuario lo activo y hay canal
	QString pageUrl() const;   // direccion publica de la lista de este canal
	QString pageLabel() const; // la misma sin "https://", para mostrarla en pantalla
	State state() const { return m_state; }
	QString message() const { return m_message; } // detalle cuando hay error
	QDateTime lastPublished() const { return m_lastPublished; }

	static QString newKey();
	// Lo que se envia: utiles tambien para las pruebas.
	static QByteArray songsJson(const Store &store);
	static QByteArray liveJson(const Store &store);

	// Borra la lista publicada de un canal (al desactivar la opcion).
	void unpublish(const QString &channel, const QString &key);

signals:
	void stateChanged();

private:
	struct Worker;
	void schedule(int ms);
	void publishNow();
	void onResult(quint64 id, const HttpResult &result, const QByteArray &songs, const QByteArray &live);
	void setState(State state, const QString &message = QString());
	void resetSent();

	Store *m_store;
	std::shared_ptr<Worker> m_worker;
	Transport m_transport;
	QTimer m_timer;
	QString m_apiUrl, m_apiKey, m_pageUrl;
	State m_state = State::Off;
	QString m_message;
	QDateTime m_lastPublished;
	QByteArray m_sentSongs, m_sentLive; // lo ultimo que el servicio confirmo
	QString m_sentFor;                  // canal + clave a los que corresponde
	bool m_inFlight = false;
	bool m_blocked = false; // error que no se arregla reintentando (otro equipo publica ese canal)
	quint64 m_requestId = 0;
	int m_debounceMs = 1500, m_retryMs = 5000, m_failures = 0;
};

} // namespace acsl

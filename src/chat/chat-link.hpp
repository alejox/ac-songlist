// Un "enlace" es una conexion con el servidor de chat que entrega y envia lineas IRC.
// Hay dos: QtLink (sockets de Qt) y NativeLink (red segura del sistema operativo).
// Ambos viven en el hilo de la interfaz y avisan con senales.
#pragma once

#include "native-conn.hpp"

#include <QObject>
#include <QPointer>

#include <functional>
#include <memory>
#include <thread>

class QTcpSocket;

namespace acsl {

class ChatLink : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;

	virtual void start() = 0;
	virtual void stop() = 0; // tras stop() no llegan mas senales
	virtual void sendLine(const QString &line) = 0;
	virtual bool secure() const = 0; // cifrada: se puede enviar el token del bot

signals:
	void opened();
	void closed();
	void lineReceived(const QString &line);
};

// Sockets de Qt: TLS si Qt lo trae; si no, texto plano (solo sirve para lectura anonima).
class QtLink : public ChatLink {
	Q_OBJECT
public:
	enum class Mode { Tls, Plain, LocalTest };
	QtLink(Mode mode, const QString &host, quint16 port, QObject *parent = nullptr);
	~QtLink() override;

	static bool tlsAvailable();

	void start() override;
	void stop() override;
	void sendLine(const QString &line) override;
	bool secure() const override { return m_mode != Mode::Plain; }

private:
	void onReadyRead();
	void onGone();

	Mode m_mode;
	QString m_host;
	quint16 m_port;
	QTcpSocket *m_socket = nullptr;
	QByteArray m_buffer;
};

// Red del sistema (NativeConn) en hilos propios: uno lee, otro escribe.
class NativeLink : public ChatLink {
	Q_OBJECT
public:
	using Factory = std::function<std::unique_ptr<NativeConn>()>;
	explicit NativeLink(Factory factory, QObject *parent = nullptr);
	~NativeLink() override;

	void start() override;
	void stop() override;
	void sendLine(const QString &line) override;
	bool secure() const override { return true; }

private:
	struct Core;
	Factory m_factory;
	std::shared_ptr<Core> m_core;
};

} // namespace acsl

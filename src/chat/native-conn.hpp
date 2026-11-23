// Conexion segura (TLS) con el chat de Twitch usando la red del sistema operativo.
// Hace falta porque el Qt que trae OBS en Windows y macOS no incluye TLS.
//
// Uso: un hilo llama a open() y luego a readLine() en bucle; otro hilo puede llamar
// a sendLine() a la vez; cancel() se puede llamar desde cualquier hilo y desbloquea todo.
#pragma once

#include <memory>
#include <string>

namespace acsl {

class NativeConn {
public:
	virtual ~NativeConn() = default;

	virtual bool open(std::string *error) = 0;          // bloquea hasta conectar o fallar
	virtual bool sendLine(const std::string &line) = 0; // una linea IRC, sin salto final
	virtual bool readLine(std::string *line) = 0;       // bloquea; false = conexion cerrada
	virtual void cancel() = 0;

	// Conexion al chat de Twitch con la red del sistema. nullptr si esta plataforma no tiene.
	static std::unique_ptr<NativeConn> createTwitch();
	static bool available();

	// Solo para pruebas: TCP sin cifrar a un servidor simulado en este mismo equipo.
	static std::unique_ptr<NativeConn> createLocalTest(int port);
};

} // namespace acsl

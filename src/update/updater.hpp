// Actualizaciones automaticas desde las versiones publicadas en GitHub.
//
// Cada cierto tiempo pregunta cual es la ultima version publicada. Si es mas nueva, descarga
// el paquete de este sistema, comprueba su huella (sha256) contra la que da GitHub, lo
// descomprime y deja los archivos nuevos en la carpeta del plugin. OBS tiene el plugin
// cargado, asi que el cambio se aplica al volver a abrir OBS:
//  - Windows no deja sobrescribir un .dll en uso pero si renombrarlo: cada archivo viejo se
//    aparta con otro nombre y el nuevo ocupa su lugar.
//  - macOS y Linux: se cambia la carpeta entera de una vez (el proceso conserva la vieja
//    mientras siga abierta). Lo apartado va fuera de la carpeta de plugins: OBS cargaria
//    cualquier copia del plugin que encontrase alli.
// Lo apartado se borra en el siguiente arranque. Si algo falla a mitad, se deshace.
#pragma once

#include "../net/https.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>

namespace acsl {

class Updater : public QObject {
	Q_OBJECT
public:
	enum class State {
		Idle,       // aun no se comprobo
		Checking,   // preguntando a GitHub
		UpToDate,   // esta es la ultima
		Available,  // hay una nueva (sin instalar)
		Installing, // descargando e instalando
		Installed,  // instalada: se usa al reiniciar OBS
		Error,
	};

	struct Release {
		QString version; // sin la "v"
		QString pageUrl; // pagina de la version (novedades y descarga a mano)
		QString assetName;
		QString assetUrl;
		QByteArray sha256; // en hexadecimal
		qint64 size = 0;
	};

	// Descarga una direccion (en las pruebas se sustituye).
	using Getter = std::function<HttpResult(const QUrl &, qint64 maxBytes)>;

	explicit Updater(const QString &currentVersion, QObject *parent = nullptr);
	~Updater() override;

	void setApiUrl(const QString &url);         // vacio = no se buscan actualizaciones
	void setInstallRoot(const QString &root);   // carpeta del plugin; vacio = no se puede instalar solo
	void setAutoInstall(bool on);               // instalar sin preguntar al encontrar una nueva
	void setGetter(Getter getter);
	void startSchedule(int firstDelayMs, int everyMs); // comprobaciones periodicas

	void check();   // pregunta por la ultima version (y la instala si autoInstall)
	void install(); // instala la que se encontro

	State state() const { return m_state; }
	QString message() const { return m_message; } // detalle del error
	Release latest() const { return m_latest; }
	QString currentVersion() const { return m_current; }
	QString installedVersion() const { return m_installed; } // la que espera al reinicio
	QDateTime lastCheck() const { return m_lastCheck; }
	bool enabled() const { return !m_apiUrl.isEmpty(); }
	bool canInstall() const { return !m_root.isEmpty(); }
	bool autoInstall() const { return m_auto; }

	// --- piezas sueltas (publicas para las pruebas) ---
	// <0, 0 o >0 como strcmp; admite "v1.2.3" y numeros de distinta longitud.
	static int compareVersions(const QString &a, const QString &b);
	static QString assetName();       // paquete de este sistema en cada version publicada
	static QString payloadName();     // carpeta del paquete que contiene el plugin
	static QString binaryInPayload(); // donde esta el binario dentro de esa carpeta
	// Carpeta del plugin a partir del binario cargado; vacio si esta instalado de otra forma.
	static QString rootForModule(const QString &modulePath);
	static bool parseRelease(const QByteArray &json, const QString &assetName, Release *out, QString *error);

	enum class Swap { PerFile, WholeFolder };
	static Swap defaultSwap();
	static bool extract(const QString &archive, const QString &dir, QString *error);
	static bool apply(const QString &payload, const QString &root, Swap swap, QString *error);
	static void cleanup(const QString &root); // borra lo apartado en una actualizacion anterior
	static bool downloadAndApply(const Release &release, const QString &root, const Getter &get, Swap swap, QString *error);

signals:
	void stateChanged();

private:
	struct Worker;
	void setState(State state, const QString &message = QString());
	void run(std::function<void()> job);

	std::shared_ptr<Worker> m_worker;
	Getter m_get;
	QTimer m_timer;
	QString m_current, m_apiUrl, m_root, m_installed;
	State m_state = State::Idle;
	QString m_message;
	Release m_latest;
	QDateTime m_lastCheck;
	bool m_auto = true;
	bool m_busy = false;
	int m_every = 0;
};

} // namespace acsl

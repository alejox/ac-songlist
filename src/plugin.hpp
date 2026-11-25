// Corazon del plugin: une biblioteca, chat, overlay y panel. Vive en el hilo de la interfaz.
#pragma once

#include "chat/twitch-chat.hpp"
#include "core/bot.hpp"
#include "core/store.hpp"
#include "overlay/overlay-source.hpp"
#include "web/publisher.hpp"

#include <QObject>
#include <QTimer>
#include <memory>
#include <vector>

namespace acsl {

class Plugin : public QObject {
	Q_OBJECT
public:
	static void create();
	static void destroy();
	static Plugin *instance();

	// Se pueden llamar desde cualquier hilo.
	static void addSource(const std::shared_ptr<SourceShared> &shared);
	static void notifySourcesChanged();
	// Ejecuta fn en el hilo de la interfaz (si el plugin sigue vivo).
	static void post(void (*fn)(Plugin *));

	Store *store() { return &m_store; }
	TwitchChat *chat() { return &m_chat; }
	Publisher *publisher() { return m_publisher.get(); }
	QString dataPath() const { return m_path; }
	void saveNow();

public slots:
	void syncSources();

private:
	Plugin();
	~Plugin() override;

	struct View {
		std::shared_ptr<SourceShared> shared;
		int styleVersion = -1;
		OverlayStyle style;
		bool listOnDemand = false;
		int pageSeconds = 8;
		// lo que se esta mostrando
		OverlayData data;
		QString identity; // vacio = oculto; al cambiar se hace un fundido
		double opacity = 0;
		QString lastRender;
		int page = 0;
		qint64 pageSince = 0;
		// barra: cada panel aparece y desaparece por su cuenta
		QString sectionIdentity[3];
		double sectionOpacity[3] = {0, 0, 0};
		OverlayCache cache; // capa quieta de la barra
		double discSeconds = 0; // reloj del disco: solo avanza mientras suena algo
	};

	void refresh();
	bool updateView(View &view, qint64 now, double dtMs);
	bool updateBar(View &view, qint64 now, double dtMs);
	OverlayData buildData(View &view, qint64 now, QString *identity);
	void setLabels(OverlayData &d) const;
	void fillSongList(View &view, qint64 now, bool enabled, OverlayData &d, QString *identity);
	void onChatMessage(const ChatUser &user, const QString &text);
	void applyChatSettings();

	Store m_store;
	TwitchChat m_chat;
	std::unique_ptr<Bot> m_bot;
	std::unique_ptr<Publisher> m_publisher;
	QString m_path;
	QTimer m_saveTimer, m_animTimer, m_slowTimer;
	std::vector<View> m_views;
	qint64 m_lastRefresh = 0;
	qint64 m_startedAt = 0;
	// ultima cancion que sono: la barra la sigue mostrando, con el disco quieto, hasta la siguiente
	struct LastSong {
		int id = 0;
		QString title, artist;
		QStringList requesters;
	} m_lastSong;
	qint64 m_listUntil = 0; // la lista "a pedido" se muestra hasta este momento
	bool m_listRestart = false;
};

} // namespace acsl

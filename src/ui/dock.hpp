// Panel acoplable dentro de OBS: cola de pedidos, biblioteca, historial y ajustes.
#pragma once

#include "../core/store.hpp"

#include <QPointer>
#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QToolButton;

namespace acsl {

class TwitchChat;
class Publisher;
class Updater;

class Dock : public QWidget {
	Q_OBJECT
public:
	Dock(Store *store, TwitchChat *chat, Publisher *publisher, Updater *updater, QWidget *parent = nullptr);

private:
	QWidget *buildQueueTab();
	QWidget *buildSongsTab();
	QWidget *buildHistoryTab();
	QWidget *buildSettingsTab();

	void refresh();
	void refreshNowPlaying();
	void refreshQueue();
	void refreshSongs();
	void refreshHistory();
	void refreshChatStatus();
	void refreshWebStatus();
	void refreshUpdate();
	void loadSettings();
	void saveSettings();
	void setWebEnabled(bool on);
	void webTopClicked();

	QList<int> selectedSongIds() const;
	int selectedQueueId() const;
	void quickAdd();
	void editSelectedSong();
	void deleteSelectedSongs();
	void importCsv();
	void exportCsv();

	QPointer<Store> m_store;
	QPointer<TwitchChat> m_chat;
	QPointer<Publisher> m_publisher;
	QPointer<Updater> m_updater;
	QTimer m_refreshTimer;

	QLabel *m_chatStatus;
	QPushButton *m_requestsToggle;
	QCheckBox *m_barSection[3] = {nullptr, nullptr, nullptr}; // paneles de la barra: lista, sonando, cola
	QLabel *m_npTitle, *m_npSub;
	QPushButton *m_btnNext, *m_btnFinish, *m_btnSkip;
	QTabWidget *m_tabs;

	QTableWidget *m_queue;
	QPushButton *m_btnPlayQueue, *m_btnRemoveQueue, *m_btnClearQueue;
	QToolButton *m_btnUp, *m_btnDown;

	QLineEdit *m_addTitle, *m_addArtist, *m_search;
	QComboBox *m_category;
	QTableWidget *m_songs;
	QLabel *m_songCount;
	QPushButton *m_btnPlaySong, *m_btnQueueSong, *m_btnEdit, *m_btnDelete;

	QTableWidget *m_history;

	QLineEdit *m_channel;
	QCheckBox *m_offList;
	QCheckBox *m_webEnabled = nullptr;
	QLabel *m_webLink = nullptr, *m_webStatus = nullptr;
	QPushButton *m_webCopy = nullptr;
	// fila de la pagina web, siempre a la vista: publicar con un clic y copiar el enlace
	QWidget *m_webTop = nullptr;
	QLabel *m_webTopLabel = nullptr;
	QPushButton *m_webTopButton = nullptr;
	bool m_webCopied = false;
	// actualizaciones: aviso arriba (solo cuando hay algo que contar) y seccion en Ajustes
	QWidget *m_updateRow = nullptr;
	QLabel *m_updateLabel = nullptr;
	QPushButton *m_updateButton = nullptr;
	QCheckBox *m_autoUpdate = nullptr;
	QLabel *m_updateStatus = nullptr;
	QPushButton *m_updateCheck = nullptr;
	QSpinBox *m_perUser, *m_maxQueue, *m_listSeconds;
	QLabel *m_settingsSaved;
};

} // namespace acsl

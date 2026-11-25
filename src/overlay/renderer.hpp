// Dibuja el overlay (tarjeta "sonando ahora", cola o lista) en una imagen.
// Solo QtGui: se puede probar sin OBS.
#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

namespace acsl {

struct OverlayStyle {
	enum Mode { NowPlaying = 0, Queue = 1, SongList = 2 };
	enum Align { Left = 0, Center = 1, Right = 2 };
	enum VAlign { Top = 0, Middle = 1, Bottom = 2 };
	// Aspecto. Los retro traen su propia paleta; de los colores elegidos solo usan el de acento.
	enum Skin { Minimal = 0, Cassette = 1, Led = 2, Vinyl = 3 };

	Mode mode = NowPlaying;
	Skin skin = Minimal;
	bool animate = true; // carretes, disco y ecualizador en movimiento (solo skins retro)
	int width = 900;
	int height = 180;

	QString fontFamily;
	int fontSize = 40; // tamano del titulo en px; el resto se calcula a partir de este
	bool bold = true;
	bool italic = false;

	QColor textColor = QColor(255, 255, 255);
	QColor mutedColor = QColor(255, 255, 255, 170);
	QColor accentColor = QColor(56, 189, 248);
	QColor backgroundColor = QColor(16, 16, 20);
	int backgroundOpacity = 85; // 0 = sin fondo, 100 = opaco
	int radius = 14;
	int padding = 22;
	Align align = Left;
	VAlign valign = Top;

	bool accentBar = true;
	bool showLabel = true;
	bool showArtist = true;
	bool showRequester = true;
	bool showNumbers = true;
	bool hideWhenEmpty = false;
	bool showHowTo = true; // "Pide con !sr" siempre visible en la cabecera de cola y lista
	QString label; // vacio = texto por defecto
	QString idleText; // "sonando ahora": que mostrar cuando no suena nada (vacio = nada)

	int rows = 6;    // filas de la cola / de la lista
	int columns = 2; // columnas de la lista
};

struct OverlayRow {
	int number = 0;
	QString title;
	QString artist;
	QString requester;
};

struct OverlayData {
	bool hasPlaying = false;
	QString title;
	QString artist;
	QStringList requesters;

	QList<OverlayRow> queue; // ya recortada a las filas visibles
	int queueTotal = 0;

	QList<OverlayRow> songs; // pagina actual
	int page = 0;
	int pages = 1;
	int songsTotal = 0;

	bool requestsOpen = true;

	// textos ya traducidos
	QString lblNowPlaying, lblQueue, lblSongList, lblRequestedBy, lblHowTo, lblMore, lblClosed, lblEmptyQueue,
		lblEmptyList;
};

// Imagen RGBA premultiplicada del tamano style.width x style.height.
// opacity (0..1) se usa para los fundidos; seconds es el reloj de las animaciones.
QImage renderOverlay(const OverlayStyle &style, const OverlayData &data, double opacity = 1.0, double seconds = 0.0);

// true si ese estilo se mueve solo y hay que redibujarlo varias veces por segundo.
inline bool isAnimated(const OverlayStyle &s)
{
	return s.animate && s.skin != OverlayStyle::Minimal && s.mode == OverlayStyle::NowPlaying;
}

// Canciones por pagina en el modo lista.
inline int songsPerPage(const OverlayStyle &s)
{
	return qMax(1, s.rows) * qMax(1, s.columns);
}

} // namespace acsl

// Dibuja el overlay (tarjeta "sonando ahora", cola, lista o la barra con las tres) en una imagen.
// Solo QtGui: se puede probar sin OBS.
#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

namespace acsl {

struct OverlayStyle {
	// Bar: una sola franja de cristal con sonando ahora, la lista y la cola.
	enum Mode { NowPlaying = 0, Queue = 1, SongList = 2, Bar = 3 };
	enum BarSection { BarList = 0, BarNow = 1, BarQueue = 2 };
	enum Align { Left = 0, Center = 1, Right = 2 };
	enum VAlign { Top = 0, Middle = 1, Bottom = 2 };
	// Aspecto. Los retro traen su propia paleta; de los colores elegidos solo usan el de acento.
	// Tocadiscos: funda con su portada, vinilo y brazo; los colores salen de la portada y al
	// cambiar de cancion se ve como se cambia el disco.
	enum Skin { Minimal = 0, Cassette = 1, Led = 2, Vinyl = 3, Turntable = 4 };
	// Barra: cristal de borde a borde, o tarjeta oscura con linea de acento, portada, cinta
	// con la cola y barra de progreso de paginas.
	enum BarStyle { BarGlass = 0, BarCard = 1 };

	Mode mode = NowPlaying;
	Skin skin = Minimal;
	BarStyle barStyle = BarGlass;
	bool animate = true; // carretes, disco y ecualizador en movimiento (skins retro y barra)
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
	bool showWeb = true;   // direccion de la pagina web en la cabecera de la lista (si esta publicada)
	QString label; // vacio = texto por defecto
	QString idleText; // "sonando ahora": que mostrar cuando no suena nada (vacio = nada)

	int rows = 6;    // filas de la cola / de la lista
	int columns = 2; // columnas de la lista
	int barCenterPercent = 34; // barra: ancho de "sonando ahora", en % del total
};

struct OverlayRow {
	int number = 0;
	QString title;
	QString artist;
	QString requester;
};

struct OverlayData {
	bool hasPlaying = false;
	// barra: no suena nada pero se sigue mostrando la ultima cancion (con el disco quieto)
	bool lastSong = false;
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
	double elapsed = -1;      // segundos que lleva sonando la cancion (-1 = no se sabe)
	double clock = 0;         // reloj continuo, en segundos (la cinta de la cola de la tarjeta)
	QString nextTitle, nextArtist; // la siguiente de la cola (vacio = no hay)

	// Tocadiscos: cambio de disco. changeT son los segundos desde que empezo el cambio
	// (grande = no hay cambio en curso); old* es la cancion que se va.
	double changeT = 1e9;
	bool changeFromNothing = false; // no habia disco: solo se ve entrar el nuevo
	QString oldTitle, oldArtist;
	QStringList oldRequesters;
	double arm = 1.0; // brazo del tocadiscos: 1 = sobre el disco, 0 = levantado
	QString webUrl; // direccion de la pagina publica, sin "https://"; vacio = no publicada

	// barra, por parte (lista, sonando, cola): si ocupa sitio y con que opacidad se ve
	// (una parte que cambia de contenido se desvanece sin dejar su hueco)
	bool sectionOn[3] = {true, true, true};
	double section[3] = {1.0, 1.0, 1.0};

	// textos ya traducidos
	QString lblNowPlaying, lblQueue, lblSongList, lblRequestedBy, lblHowTo, lblMore, lblClosed, lblEmptyQueue,
		lblEmptyList, lblWeb, lblSongs, lblLastSong, lblNext, lblTicker, lblNothing;
};

// Lo que no cambia de un fotograma al siguiente (paneles y texto de la barra) se guarda aqui
// para no volver a dibujarlo mientras solo gira el disco. Quien lo usa debe vaciarlo con
// clear() cada vez que cambie el estilo; los cambios en los datos los detecta solo.
struct OverlayCache {
	QString key;
	QImage base;
	void clear()
	{
		key.clear();
		base = QImage();
	}
};

// Imagen RGBA premultiplicada del tamano style.width x style.height.
// opacity (0..1) se usa para los fundidos; seconds es el reloj de las animaciones.
QImage renderOverlay(const OverlayStyle &style, const OverlayData &data, double opacity = 1.0, double seconds = 0.0,
		     OverlayCache *cache = nullptr);

// true si ese estilo se mueve solo y hay que redibujarlo varias veces por segundo.
inline bool isAnimated(const OverlayStyle &s)
{
	if (s.mode == OverlayStyle::Bar)
		return s.animate; // el disco y, en la tarjeta, la cinta de la cola
	return s.animate && s.skin != OverlayStyle::Minimal && s.mode == OverlayStyle::NowPlaying;
}

// Duracion del cambio de disco del tocadiscos, en segundos.
constexpr double kTurntableChange = 2.7;

// Portada inventada para una cancion (no hay portadas de verdad): siempre la misma para el mismo
// titulo y artista. dark/light son los colores de la tarjeta y del texto de acento.
QImage coverArt(const QString &title, const QString &artist, int size);
QColor coverDark(const QString &title, const QString &artist);
QColor coverLight(const QString &title, const QString &artist);

// Barra: cuantas filas caben en cada panel con ese tamano y esa letra.
struct BarLayout {
	int listRows = 1;
	int listColumns = 1;
	int queueRows = 1;
};
BarLayout barLayout(const OverlayStyle &style);

// Canciones por pagina en el modo lista (y en el panel de lista de la barra).
inline int songsPerPage(const OverlayStyle &s)
{
	if (s.mode == OverlayStyle::Bar) {
		const BarLayout b = barLayout(s);
		return b.listRows * b.listColumns;
	}
	return qMax(1, s.rows) * qMax(1, s.columns);
}

} // namespace acsl

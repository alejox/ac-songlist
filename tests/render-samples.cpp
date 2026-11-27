// Genera imagenes de muestra del overlay para revisar el diseno sin abrir OBS.
#include "overlay/renderer.hpp"

#include <QDir>
#include <QGuiApplication>
#include <QPainter>

using namespace acsl;

static OverlayData sampleData()
{
	OverlayData d;
	d.hasPlaying = true;
	d.title = QStringLiteral("Mi Coraz\u00f3n Encantado");
	d.artist = "Dragon Ball GT";
	d.requesters = {"luna_gamer"};
	d.queue = {{1, "Dancing Queen", "ABBA", "pianofan22"},
		   {2, "Take On Me", "A-HA", "luna_gamer, el_mati"},
		   {3, "The Girl Who Stole The Stars", "Chrono Cross", "un_nombre_de_usuario_largo"},
		   {4, QStringLiteral("Sof\u00eda"), QStringLiteral("\u00c1lvaro Soler"), "sofi_music"}};
	d.queueTotal = 7;
	const char *songs[][2] = {{"Summer Moved On", "a-ha"}, {"Dancing Queen", "ABBA"}, {"Gimme Gimme Gimme", "ABBA"},
				  {"Quest for Butter (Main Theme)", "Age Of Empires II - The Age Of Kings"},
				  {"Take On Me", "A-HA"}, {"Part of Your World", "Alan Menken"},
				  {"Un Mundo Ideal", "Alan Menken"}, {"La Bella y la Bestia", "Alan Menken"},
				  {"Faded", "Alan Walker"}, {"Frenesi", "Alberto Dominguez"},
				  {"Caminando en el aire", "Aled Jones"}, {"Sofia", "Alvaro Soler"},
				  {"El Fantasma de la Opera", "Andrew Lloyd Webber"}, {"Sayonara no Natsu", "Aoi Teshima"},
				  {"How Deep Is Your Love", "Bee Gees"}, {"Words", "Bee Gees"},
				  {"Just the Two of Us", "Bill Withers"}, {"Habanera", "Bizet"},
				  {"Sunny", "Bobby Hebb"}, {"Bed of Roses", "Bon Jovi"},
				  {"Count On Me", "Bruno Mars"}, {"Marry You", "Bruno Mars"},
				  {"Treasure", "Bruno Mars"}, {"Close To You", "Carpenters"}};
	int n = 1;
	for (auto &s : songs)
		d.songs.append({n++, s[0], s[1], {}});
	d.songsTotal = 141;
	d.page = 0;
	d.pages = 6;
	d.lblNowPlaying = "Sonando ahora";
	d.lblQueue = "Cola de pedidos";
	d.lblSongList = "Lista de canciones";
	d.lblRequestedBy = "pedida por %1";
	d.lblHowTo = QStringLiteral("Pide con !sr n\u00famero");
	d.lblMore = QStringLiteral("+%1 m\u00e1s");
	d.lblClosed = "Pedidos cerrados";
	d.lblEmptyQueue = "Sin pedidos";
	d.lblEmptyList = QStringLiteral("A\u00fan no hay canciones");
	d.lblWeb = "Lista completa: %1";
	d.lblSongs = "Canciones";
	d.lblLastSong = QStringLiteral("\u00daltima canci\u00f3n");
	d.lblNext = "Siguiente: %1";
	d.lblNothing = "Nada sonando";
	d.lblTicker = "Pide con !sr n\u00famero o nombre  \u2022  Escribe !lista para ver las canciones";
	return d;
}

// Fondo tipo "captura de juego" para ver el overlay como en un stream.
static void saveOnBackdrop(const QImage &overlay, const QString &path, bool light = false)
{
	QImage out(overlay.width() + 80, overlay.height() + 80, QImage::Format_ARGB32_Premultiplied);
	QPainter p(&out);
	QLinearGradient g(0, 0, out.width(), out.height());
	if (light) {
		g.setColorAt(0, QColor(214, 222, 235));
		g.setColorAt(1, QColor(240, 226, 208));
	} else {
		g.setColorAt(0, QColor(38, 52, 84));
		g.setColorAt(0.5, QColor(92, 54, 96));
		g.setColorAt(1, QColor(30, 34, 46));
	}
	p.fillRect(out.rect(), g);
	p.drawImage(40, 40, overlay);
	p.end();
	out.save(path);
}

int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	const QString dir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
	QDir().mkpath(dir);
	const OverlayData d = sampleData();

	OverlayStyle np;
	np.mode = OverlayStyle::NowPlaying;
	np.width = 900;
	np.height = 170;
	saveOnBackdrop(renderOverlay(np, d), dir + "/01-sonando.png");

	OverlayStyle np2 = np; // sin fondo, alineada a la derecha, otro acento
	np2.backgroundOpacity = 0;
	np2.align = OverlayStyle::Right;
	np2.accentColor = QColor(255, 196, 0);
	np2.fontSize = 46;
	saveOnBackdrop(renderOverlay(np2, d), dir + "/02-sonando-sin-fondo.png");

	OverlayStyle np3 = np; // claro
	np3.backgroundColor = QColor(255, 255, 255);
	np3.backgroundOpacity = 92;
	np3.textColor = QColor(20, 22, 28);
	np3.mutedColor = QColor(20, 22, 28, 160);
	np3.accentColor = QColor(225, 29, 72);
	np3.radius = 4;
	saveOnBackdrop(renderOverlay(np3, d), dir + "/03-sonando-claro.png", true);

	OverlayData longData = d; // titulo larguisimo: tiene que recortarse con "..."
	longData.title = "Quest for Butter (Main Theme) - Version extendida para piano y orquesta sinfonica";
	longData.artist = "Age Of Empires II - The Age Of Kings Definitive Edition Soundtrack";
	longData.requesters = {"un_nombre_de_usuario_largo", "otro_usuario_mas"};
	saveOnBackdrop(renderOverlay(np, longData), dir + "/04-sonando-largo.png");

	OverlayStyle q;
	q.mode = OverlayStyle::Queue;
	q.width = 640;
	q.height = 420;
	q.rows = 4;
	saveOnBackdrop(renderOverlay(q, d), dir + "/05-cola.png");

	OverlayData empty = d;
	empty.queue.clear();
	empty.queueTotal = 0;
	saveOnBackdrop(renderOverlay(q, empty), dir + "/06-cola-vacia.png");

	OverlayStyle list;
	list.mode = OverlayStyle::SongList;
	list.width = 1500;
	list.height = 560;
	list.rows = 8;
	list.columns = 3;
	saveOnBackdrop(renderOverlay(list, d), dir + "/07-lista.png");

	// skins retro: sonando ahora (con titulo normal y con uno largo), cola y lista
	const char *names[] = {"minimal", "casete", "equipo", "vinilo"};
	const QColor accents[] = {QColor(56, 189, 248), QColor(214, 73, 51), QColor(255, 176, 0), QColor(196, 148, 72)};
	for (int skin = 1; skin <= 3; skin++) {
		OverlayStyle now = np;
		now.skin = OverlayStyle::Skin(skin);
		now.accentColor = accents[skin];
		now.height = 200;
		saveOnBackdrop(renderOverlay(now, d, 1.0, 0.7), dir + QString("/1%1-%2-sonando.png").arg(skin).arg(names[skin]));
		saveOnBackdrop(renderOverlay(now, longData, 1.0, 6.3), dir + QString("/1%1-%2-sonando-largo.png").arg(skin).arg(names[skin]));

		OverlayStyle queue = q;
		queue.skin = OverlayStyle::Skin(skin);
		queue.accentColor = accents[skin];
		saveOnBackdrop(renderOverlay(queue, d), dir + QString("/1%1-%2-cola.png").arg(skin).arg(names[skin]));

		OverlayStyle songs = list;
		songs.skin = OverlayStyle::Skin(skin);
		songs.accentColor = accents[skin];
		saveOnBackdrop(renderOverlay(songs, d), dir + QString("/1%1-%2-lista.png").arg(skin).arg(names[skin]));
	}

	// barra inferior: tres paneles de cristal sobre una escena con formas y luces
	const auto saveOnScene = [](const QImage &overlay, const QString &path) {
		QImage out(overlay.width(), overlay.height() + 360, QImage::Format_ARGB32_Premultiplied);
		QPainter p(&out);
		p.setRenderHint(QPainter::Antialiasing, true);
		QLinearGradient g(0, 0, out.width(), out.height());
		g.setColorAt(0, QColor(24, 30, 52));
		g.setColorAt(0.45, QColor(120, 62, 88));
		g.setColorAt(1, QColor(232, 150, 84));
		p.fillRect(out.rect(), g);
		p.setPen(Qt::NoPen);
		const QColor blobs[] = {QColor(255, 214, 120, 150), QColor(70, 160, 220, 130), QColor(240, 240, 240, 110), QColor(30, 20, 40, 170),
					QColor(250, 110, 90, 140)};
		for (int i = 0; i < 14; i++) {
			p.setBrush(blobs[i % 5]);
			const double x = (i * 397) % out.width(), y = out.height() - 60 - (i * 131) % 330, r = 40 + (i * 53) % 150;
			p.drawEllipse(QPointF(x, y), r, r * 0.8);
		}
		p.drawImage(0, 360, overlay);
		p.end();
		out.save(path);
	};
	OverlayStyle bar;
	bar.mode = OverlayStyle::Bar;
	bar.width = 1920;
	bar.height = 200;
	bar.columns = 2;
	OverlayData barData = d;
	const BarLayout bl = barLayout(bar);
	while (barData.songs.size() > bl.listRows * bl.listColumns)
		barData.songs.removeLast();
	while (barData.queue.size() > bl.queueRows)
		barData.queue.removeLast();
	barData.pages = 12;
	saveOnScene(renderOverlay(bar, barData, 1.0, 0.4), dir + "/20-barra.png");

	// opacidad del cristal: 100 = solido; al bajarla empieza a verse lo de detras
	for (int opacity : {100, 95, 85}) {
		OverlayStyle o = bar;
		o.backgroundOpacity = opacity;
		saveOnScene(renderOverlay(o, barData, 1.0, 0.4), dir + QString("/27-barra-opacidad-%1.png").arg(opacity));
	}
	OverlayStyle big = bar; // letra grande (Tamano 56 con el alto por defecto)
	big.fontSize = 56;
	big.backgroundOpacity = 100;
	OverlayData bigData = d;
	const BarLayout bigLayout = barLayout(big);
	while (bigData.songs.size() > bigLayout.listRows * bigLayout.listColumns)
		bigData.songs.removeLast();
	while (bigData.queue.size() > bigLayout.queueRows)
		bigData.queue.removeLast();
	saveOnScene(renderOverlay(big, bigData, 1.0, 0.4), dir + "/28-barra-letra-grande.png");

	OverlayData barWeb = barData; // con la lista publicada: la direccion va en la cabecera
	barWeb.webUrl = "ac-songlist.vercel.app/argeplaying";
	saveOnScene(renderOverlay(bar, barWeb, 1.0, 0.4), dir + "/25-barra-con-web.png");
	OverlayData listWeb = d;
	listWeb.webUrl = barWeb.webUrl;
	saveOnBackdrop(renderOverlay(list, listWeb), dir + "/26-lista-con-web.png");

	OverlayData barLong = barData;
	barLong.title = longData.title;
	barLong.artist = longData.artist;
	barLong.requesters = longData.requesters;
	saveOnScene(renderOverlay(bar, barLong, 1.0, 1.1), dir + "/21-barra-titulo-largo.png");

	OverlayData onlyNow = barData; // sonando y cola: la cola ocupa el sitio de la lista
	onlyNow.sectionOn[OverlayStyle::BarList] = false;
	onlyNow.section[OverlayStyle::BarList] = 0;
	saveOnScene(renderOverlay(bar, onlyNow, 1.0, 2.0), dir + "/22-barra-solo-sonando.png");

	OverlayData sides = barData; // nada sonando, cola vacia y pedidos cerrados
	sides.hasPlaying = false;
	sides.lastSong = true; // termino la cancion: se queda su informacion con el disco quieto
	sides.queue.clear();
	sides.queueTotal = 0;
	sides.requestsOpen = false;
	saveOnScene(renderOverlay(bar, sides, 1.0, 0.0), dir + "/23-barra-sin-sonar.png");

	OverlayStyle barLight = bar; // cristal claro, otro acento, 720p
	barLight.width = 1280;
	barLight.height = 140;
	barLight.backgroundColor = QColor(255, 255, 255);
	barLight.backgroundOpacity = 70;
	barLight.textColor = QColor(18, 20, 28);
	barLight.mutedColor = QColor(18, 20, 28, 165);
	barLight.accentColor = QColor(225, 29, 72);
	OverlayData lightData = d;
	const BarLayout ll = barLayout(barLight);
	while (lightData.songs.size() > ll.listRows * ll.listColumns)
		lightData.songs.removeLast();
	while (lightData.queue.size() > ll.queueRows)
		lightData.queue.removeLast();
	saveOnScene(renderOverlay(barLight, lightData, 1.0, 0.9), dir + "/24-barra-clara-720.png");

	// tocadiscos: quieto, momentos del cambio de disco y "ultima cancion"
	OverlayStyle tt = np;
	tt.skin = OverlayStyle::Turntable;
	tt.width = 1100;
	tt.height = 200;
	OverlayData ttData = d;
	ttData.elapsed = 75;
	ttData.nextTitle = "Take On Me";
	ttData.nextArtist = "A-HA";
	saveOnBackdrop(renderOverlay(tt, ttData, 1.0, 0.3), dir + "/30-tocadiscos.png");
	OverlayData change = ttData;
	change.oldTitle = "Neon Lullaby";
	change.oldArtist = "Parade of Ghosts";
	change.oldRequesters = {"pianofan22"};
	change.title = "Static Garden";
	change.artist = "Moth Choir";
	change.requesters = {"sofi_music"};
	int frame = 0;
	for (double ct : {0.0, 0.2, 0.45, 0.7, 0.9, 1.15, 1.45, 1.7, 1.9, 2.2, 2.7}) {
		change.changeT = ct;
		saveOnBackdrop(renderOverlay(tt, change, 1.0, 0.3 + ct), dir + QString("/31-cambio-%1.png").arg(frame++, 2, 10, QLatin1Char('0')));
	}
	// barra con el diseno tarjeta: seis colores de acento, uno con tema claro
	{
		OverlayStyle card;
		card.mode = OverlayStyle::Bar;
		card.barStyle = OverlayStyle::BarCard;
		card.width = 1920;
		card.height = 200;
		card.columns = 2;
		card.backgroundOpacity = 100;
		const BarLayout cl = barLayout(card);
		OverlayData cd = d;
		cd.elapsed = 185;
		cd.clock = 3.0;
		while (cd.songs.size() > cl.listRows * cl.listColumns)
			cd.songs.removeLast();
		cd.pages = 12;
		cd.page = 2;
		for (int i = 0; i <= kCardThemes; i++) { // los seis temas y "con mis colores"
			OverlayStyle c = card;
			c.cardTheme = i;
			c.accentColor = QColor(244, 114, 182);
			saveOnScene(renderOverlay(c, cd, 1.0, 0.6 + i), dir + QString("/40-tarjeta-%1.png").arg(i));
		}
		{ // instrucciones bajo la lista con letra grande (no caben: pasan como cinta)
			OverlayStyle c = card;
			c.howtoSize = 22;
			c.howtoFamily = "DejaVu Serif";
			OverlayData web = cd;
			web.webUrl = "ac-songlist.vercel.app/argeplaying";
			saveOnScene(renderOverlay(c, web, 1.0, 0.6), dir + "/43-instrucciones-grandes.png");
		}
		for (int op : {100, 60, 0}) { // opacidad del fondo, en todos los temas
			for (int i = 0; i <= kCardThemes; i++) {
				OverlayStyle c = card;
				c.cardTheme = i;
				c.cardOpacity = op;
				saveOnScene(renderOverlay(c, cd, 1.0, 0.6), dir + QString("/42-opacidad-%1-tema-%2.png").arg(op).arg(i));
			}
		}
		// diseno de la cola: galeria, vinilos y abanico, en varios temas; y con una sola cancion
		for (int look = OverlayStyle::QueueCovers; look <= OverlayStyle::QueueFan; look++) {
			for (int theme : {1, 2, 3, 6}) {
				OverlayStyle c = card;
				c.cardTheme = theme;
				c.queueLook = OverlayStyle::QueueLook(look);
				saveOnScene(renderOverlay(c, cd, 1.0, 0.6), dir + QString("/44-cola-%1-tema-%2.png").arg(look).arg(theme));
			}
			OverlayStyle c = card;
			c.queueLook = OverlayStyle::QueueLook(look);
			OverlayData one = cd;
			while (one.queue.size() > 1)
				one.queue.removeLast();
			one.queueTotal = 1;
			saveOnScene(renderOverlay(c, one, 1.0, 0.6), dir + QString("/45-cola-%1-una.png").arg(look));
			OverlayCache cache; // con cache: la capa quieta mas lo que gira debe dar lo mismo
			OverlayData two = cd;
			two.clock = 7.3;
			const QImage a = renderOverlay(c, two, 1.0, 0.6);
			renderOverlay(c, two, 1.0, 0.6, &cache);
			const QImage b = renderOverlay(c, two, 1.0, 0.6, &cache);
			b.save(dir + QString("/46-cola-%1-cache.png").arg(look));
			a.save(dir + QString("/46-cola-%1-sincache.png").arg(look));
		}
		OverlayData idle = cd; // sin nada sonando, cola vacia
		idle.hasPlaying = false;
		idle.lastSong = false;
		idle.queue.clear();
		idle.queueTotal = 0;
		saveOnScene(renderOverlay(card, idle, 1.0, 0.0), dir + "/41-tarjeta-vacia.png");
	}

	OverlayData last = ttData; // termino la cancion: brazo arriba, se queda la informacion
	last.hasPlaying = false;
	last.lastSong = true;
	last.arm = 0;
	last.elapsed = -1;
	saveOnBackdrop(renderOverlay(tt, last, 1.0, 0.3), dir + "/32-tocadiscos-ultima.png");
	return 0;
}

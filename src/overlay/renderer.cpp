#include "renderer.hpp"

#include <QFontDatabase>
#include <QHash>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace acsl {

namespace {

const QString kDash = QStringLiteral(" — ");
const QString kDot = QStringLiteral("  ·  ");

QFont makeFont(const OverlayStyle &s, double px, bool bold, double spacing = 0)
{
	QFont f;
	if (!s.fontFamily.isEmpty())
		f.setFamily(s.fontFamily);
	f.setPixelSize(qMax(8, int(px + 0.5)));
	f.setBold(bold);
	f.setItalic(s.italic);
	f.setHintingPreference(QFont::PreferNoHinting);
	if (spacing > 0)
		f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
	return f;
}

double textWidth(const QFont &f, const QString &t)
{
	return QFontMetricsF(f).horizontalAdvance(t);
}

QString elide(const QFont &f, const QString &t, double width)
{
	return QFontMetricsF(f).elidedText(t, Qt::ElideRight, qMax(0.0, width));
}

void drawCard(QPainter &p, const OverlayStyle &s, const QRectF &rect)
{
	if (s.backgroundOpacity <= 0)
		return;
	QColor bg = s.backgroundColor;
	bg.setAlphaF(qBound(0, s.backgroundOpacity, 100) / 100.0);
	QPainterPath path;
	const double r = qMin(double(s.radius), qMin(rect.width(), rect.height()) / 2.0);
	path.addRoundedRect(rect, r, r);
	p.fillPath(path, bg);
}

// Coloca un bloque de (w x h) dentro del lienzo segun la alineacion elegida.
QRectF placeCard(const OverlayStyle &s, double w, double h)
{
	w = qMin(w, double(s.width));
	h = qMin(h, double(s.height));
	double x = 0, y = 0;
	if (s.align == OverlayStyle::Center)
		x = (s.width - w) / 2.0;
	else if (s.align == OverlayStyle::Right)
		x = s.width - w;
	if (s.valign == OverlayStyle::Middle)
		y = (s.height - h) / 2.0;
	else if (s.valign == OverlayStyle::Bottom)
		y = s.height - h;
	return QRectF(x, y, w, h);
}

// Una linea con dos partes: principal (color normal) y secundaria (apagada).
void drawTwoTone(QPainter &p, const OverlayStyle &s, const QFont &f, const QRectF &r, const QString &main,
		 const QString &secondary)
{
	const QFontMetricsF fm(f);
	p.setFont(f);
	const double mainW = fm.horizontalAdvance(main);
	if (secondary.isEmpty() || mainW >= r.width() - fm.averageCharWidth() * 3) {
		p.setPen(s.textColor);
		p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(f, main, r.width()));
		return;
	}
	p.setPen(s.textColor);
	p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, main);
	QRectF rest = r;
	rest.setLeft(r.left() + mainW);
	p.setPen(s.mutedColor);
	p.drawText(rest, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(f, secondary, rest.width()));
}

void drawNowPlaying(QPainter &p, const OverlayStyle &s, const OverlayData &d)
{
	const bool idle = !d.hasPlaying;
	if (idle && s.idleText.trimmed().isEmpty())
		return;

	const QString title = idle ? s.idleText.trimmed() : d.title;
	QString sub;
	if (!idle) {
		if (s.showArtist && !d.artist.isEmpty())
			sub = d.artist;
		if (s.showRequester && !d.requesters.isEmpty()) {
			const QString by = d.lblRequestedBy.arg(d.requesters.join(QStringLiteral(", ")));
			sub = sub.isEmpty() ? by : sub + kDot + by;
		}
	}
	const QString label = (s.label.trimmed().isEmpty() ? d.lblNowPlaying : s.label.trimmed()).toUpper();
	const bool showLabel = s.showLabel && !idle && !label.isEmpty();

	const QFont titleFont = makeFont(s, s.fontSize, s.bold);
	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const QFont subFont = makeFont(s, s.fontSize * 0.56, false);
	const QFontMetricsF tfm(titleFont), lfm(labelFont), sfm(subFont);

	const double pad = s.padding;
	const double bar = s.accentBar ? qMax(3.0, s.fontSize * 0.11) : 0;
	const double barGap = s.accentBar ? s.fontSize * 0.42 : 0;
	const double gap = s.fontSize * 0.14;

	double textH = tfm.height();
	if (showLabel)
		textH += lfm.height() + gap;
	if (!sub.isEmpty())
		textH += sfm.height() + gap * 0.6;

	const double maxTextW = s.width - pad * 2 - bar - barGap;
	double textW = textWidth(titleFont, title);
	if (showLabel)
		textW = qMax(textW, textWidth(labelFont, label));
	if (!sub.isEmpty())
		textW = qMax(textW, textWidth(subFont, sub));
	textW = qMin(textW + 2, maxTextW);

	const QRectF card = placeCard(s, textW + pad * 2 + bar + barGap, textH + pad * 2);
	drawCard(p, s, card);

	double x = card.left() + pad;
	double y = card.top() + pad;
	if (s.accentBar) {
		QPainterPath barPath;
		barPath.addRoundedRect(QRectF(x, y, bar, textH), bar / 2, bar / 2);
		p.fillPath(barPath, s.accentColor);
		x += bar + barGap;
	}
	const double w = card.right() - pad - x;

	if (showLabel) {
		p.setFont(labelFont);
		p.setPen(s.accentColor);
		p.drawText(QRectF(x, y, w, lfm.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(labelFont, label, w));
		y += lfm.height() + gap;
	}
	p.setFont(titleFont);
	p.setPen(idle ? s.mutedColor : s.textColor);
	p.drawText(QRectF(x, y, w, tfm.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
		   elide(titleFont, title, w));
	y += tfm.height() + gap * 0.6;
	if (!sub.isEmpty()) {
		p.setFont(subFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(x, y, w, sfm.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(subFont, sub, w));
	}
}

// ---------------------------------------------------------------- skins retro
// Disenos propios inspirados en objetos de musica de otra epoca: una cinta de casete,
// el display de un equipo de sonido y un disco de vinilo. De los colores elegidos por
// el usuario solo toman el de acento; el resto es la paleta del skin.

QColor mix(const QColor &a, const QColor &b, double t)
{
	return QColor(int(a.red() + (b.red() - a.red()) * t), int(a.green() + (b.green() - a.green()) * t),
		      int(a.blue() + (b.blue() - a.blue()) * t));
}

bool isLight(const QColor &c)
{
	return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF() > 0.62;
}

const QColor kPaper(241, 231, 211);
const QColor kInk(34, 30, 26);
const QColor kCream(243, 235, 221);
const QColor kSleeve(32, 28, 26);
const QColor kGlass(6, 9, 8);

QString monoFamily()
{
	return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

// Paleta y tipografia de cada skin para la cola y la lista.
OverlayStyle skinned(const OverlayStyle &in)
{
	OverlayStyle s = in;
	switch (in.skin) {
	case OverlayStyle::Minimal:
		return s;
	case OverlayStyle::Cassette: // la tarjeta de papel de la caja del casete
		s.backgroundColor = kPaper;
		s.textColor = kInk;
		s.mutedColor = QColor(kInk.red(), kInk.green(), kInk.blue(), 150);
		if (isLight(in.accentColor)) // un acento claro no se leeria sobre papel
			s.accentColor = mix(in.accentColor, Qt::black, 0.5);
		s.radius = int(in.fontSize * 0.12);
		break;
	case OverlayStyle::Led: // la pantalla del equipo
		s.backgroundColor = kGlass;
		s.textColor = mix(in.accentColor, Qt::white, 0.3);
		s.mutedColor = QColor(in.accentColor.red(), in.accentColor.green(), in.accentColor.blue(), 175);
		s.fontFamily = monoFamily();
		s.bold = false;
		s.radius = int(in.fontSize * 0.22);
		break;
	case OverlayStyle::Vinyl: // la contraportada del disco
	case OverlayStyle::Turntable:
		s.backgroundColor = kSleeve;
		s.textColor = kCream;
		s.mutedColor = QColor(kCream.red(), kCream.green(), kCream.blue(), 150);
		s.radius = int(in.fontSize * 0.08);
		break;
	}
	s.backgroundOpacity = 100;
	s.padding = qMax(in.padding, int(in.fontSize * 0.6));
	return s;
}

// Fondo del panel de cola / lista.
void drawPanel(QPainter &p, const OverlayStyle &s, const QRectF &card)
{
	if (s.skin == OverlayStyle::Minimal) {
		drawCard(p, s, card);
		return;
	}
	const double f = s.fontSize;
	const double r = qMin(double(s.radius), qMin(card.width(), card.height()) / 2.0);
	QPainterPath path;
	path.addRoundedRect(card, r, r);

	if (s.skin == OverlayStyle::Cassette) {
		p.fillPath(path, s.backgroundColor);
		p.save();
		p.setClipPath(path);
		p.fillRect(QRectF(card.left(), card.top(), card.width(), f * 0.17), s.accentColor); // franja de color
		p.fillRect(QRectF(card.left() + s.padding * 0.5, card.top(), qMax(1.0, f * 0.035), card.height()),
			   QColor(196, 62, 52, 150)); // margen rojo de cuaderno
		p.restore();
	} else if (s.skin == OverlayStyle::Led) {
		QLinearGradient frame(card.topLeft(), card.bottomLeft()); // marco metalico
		frame.setColorAt(0, QColor(62, 64, 70));
		frame.setColorAt(1, QColor(24, 25, 28));
		p.fillPath(path, frame);
		const double b = f * 0.14;
		QPainterPath glass;
		glass.addRoundedRect(card.adjusted(b, b, -b, -b), r * 0.6, r * 0.6);
		p.fillPath(glass, s.backgroundColor);
		QColor glow = s.accentColor;
		glow.setAlpha(16);
		p.fillPath(glass, glow);
	} else { // Vinyl
		p.fillPath(path, s.backgroundColor);
		p.save();
		p.setBrush(Qt::NoBrush);
		QColor line = s.accentColor;
		line.setAlpha(210);
		const double m = f * 0.2;
		p.setPen(QPen(line, qMax(1.0, f * 0.04)));
		p.drawRect(card.adjusted(m, m, -m, -m));
		line.setAlpha(90);
		p.setPen(QPen(line, qMax(1.0, f * 0.02)));
		p.drawRect(card.adjusted(m * 1.55, m * 1.55, -m * 1.55, -m * 1.55));
		p.restore();
	}
}

// Lineas de barrido del display, por encima del texto.
void drawScanlines(QPainter &p, const QRectF &area, double step)
{
	step = qMax(2.0, step);
	for (double y = area.top(); y < area.bottom(); y += step)
		p.fillRect(QRectF(area.left(), y + step * 0.6, area.width(), step * 0.4), QColor(0, 0, 0, 70));
}

void finishPanel(QPainter &p, const OverlayStyle &s, const QRectF &card)
{
	if (s.skin != OverlayStyle::Led)
		return;
	const double b = s.fontSize * 0.14;
	drawScanlines(p, card.adjusted(b, b, -b, -b), s.fontSize * 0.09);
}

// Renglon de papel bajo cada fila en el skin de casete.
void rowRule(QPainter &p, const OverlayStyle &s, double left, double right, double bottom)
{
	if (s.skin == OverlayStyle::Cassette)
		p.fillRect(QRectF(left, bottom - 1, right - left, 1), QColor(70, 96, 140, 70));
}

struct NowText {
	bool visible = false;
	bool idle = false;
	QString label, title, sub;
};

NowText nowText(const OverlayStyle &s, const OverlayData &d)
{
	NowText n;
	n.idle = !d.hasPlaying && !d.lastSong;
	if (n.idle && s.idleText.trimmed().isEmpty())
		return n;
	n.visible = true;
	n.title = n.idle ? s.idleText.trimmed() : d.title;
	if (!n.idle) {
		if (s.showArtist && !d.artist.isEmpty())
			n.sub = d.artist;
		if (s.showRequester && !d.requesters.isEmpty()) {
			const QString by = d.lblRequestedBy.arg(d.requesters.join(QStringLiteral(", ")));
			n.sub = n.sub.isEmpty() ? by : n.sub + kDot + by;
		}
		if (s.showLabel) {
			if (d.lastSong && !d.lblLastSong.isEmpty())
				n.label = d.lblLastSong.toUpper();
			else
				n.label = (s.label.trimmed().isEmpty() ? d.lblNowPlaying : s.label.trimmed()).toUpper();
		}
	}
	return n;
}

void drawLine(QPainter &p, const QFont &font, const QColor &color, const QRectF &r, const QString &text)
{
	p.setFont(font);
	p.setPen(color);
	p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(font, text, r.width()));
}

// --------------------------------------------------------------------- casete
void drawCassette(QPainter &p, const OverlayStyle &s, const OverlayData &d, double t)
{
	const NowText n = nowText(s, d);
	if (!n.visible)
		return;
	const double f = s.fontSize;
	const QFont titleFont = makeFont(s, f * 0.9, true);
	const QFont labelFont = makeFont(s, f * 0.32, true, f * 0.06);
	const QFont subFont = makeFont(s, f * 0.5, false);
	const QFontMetricsF tfm(titleFont), lfm(labelFont), sfm(subFont);

	const double pad = f * 0.3, inPad = f * 0.3, gap = f * 0.3;
	const double stripeH = lfm.height() + f * 0.2;
	const double paperH = stripeH + f * 0.14 + tfm.height() + (n.sub.isEmpty() ? 0 : sfm.height() + f * 0.04) + f * 0.2;
	const double windowH = paperH * 0.68, windowW = windowH * 2.3;
	const double maxPaperW = qMax(f * 3.0, s.width - pad * 2 - gap - windowW);
	const double wanted = qMax(qMax(textWidth(titleFont, n.title), textWidth(subFont, n.sub)), textWidth(labelFont, n.label)) +
			      inPad * 2 + 2;
	const double paperW = qMin(qMax(wanted, f * 4.5), maxPaperW);
	const QRectF card = placeCard(s, pad + paperW + gap + windowW + pad, paperH + pad * 2);

	// cuerpo de plastico
	QPainterPath body;
	body.addRoundedRect(card, f * 0.22, f * 0.22);
	QLinearGradient plastic(card.topLeft(), card.bottomLeft());
	plastic.setColorAt(0, QColor(56, 56, 62));
	plastic.setColorAt(0.5, QColor(37, 37, 42));
	plastic.setColorAt(1, QColor(26, 26, 30));
	p.fillPath(body, plastic);
	p.setPen(QPen(QColor(255, 255, 255, 28), 1));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), f * 0.22, f * 0.22);

	// tornillos en las esquinas
	const double screw = f * 0.065, inset = f * 0.15;
	p.setPen(Qt::NoPen);
	for (const QPointF &c : {QPointF(card.left() + inset, card.top() + inset), QPointF(card.right() - inset, card.top() + inset),
				 QPointF(card.left() + inset, card.bottom() - inset), QPointF(card.right() - inset, card.bottom() - inset)}) {
		p.setBrush(QColor(118, 118, 124));
		p.drawEllipse(c, screw, screw);
		p.fillRect(QRectF(c.x() - screw * 0.7, c.y() - screw * 0.14, screw * 1.4, screw * 0.28), QColor(40, 40, 44));
	}

	// etiqueta de papel con franja de color
	const QRectF paper(card.left() + pad, card.top() + pad, paperW, paperH);
	QPainterPath paperPath;
	paperPath.addRoundedRect(paper, f * 0.1, f * 0.1);
	p.fillPath(paperPath, kPaper);
	p.save();
	p.setClipPath(paperPath);
	p.fillRect(QRectF(paper.left(), paper.top(), paper.width(), stripeH), s.accentColor);
	p.restore();
	const double textW = paper.width() - inPad * 2;
	if (!n.label.isEmpty())
		drawLine(p, labelFont, isLight(s.accentColor) ? kInk : QColor(255, 255, 255),
			 QRectF(paper.left() + inPad, paper.top(), textW, stripeH), n.label);
	double y = paper.top() + stripeH + f * 0.14;
	drawLine(p, titleFont, n.idle ? QColor(kInk.red(), kInk.green(), kInk.blue(), 150) : kInk,
		 QRectF(paper.left() + inPad, y, textW, tfm.height()), n.title);
	y += tfm.height() + f * 0.04;
	if (!n.sub.isEmpty())
		drawLine(p, subFont, QColor(kInk.red(), kInk.green(), kInk.blue(), 170), QRectF(paper.left() + inPad, y, textW, sfm.height()),
			 n.sub);

	// ventana con los dos carretes
	const QRectF window(paper.right() + gap, card.center().y() - windowH / 2, windowW, windowH);
	QPainterPath windowPath;
	windowPath.addRoundedRect(window, windowH * 0.16, windowH * 0.16);
	p.fillPath(windowPath, QColor(12, 12, 14));
	p.setPen(QPen(QColor(255, 255, 255, 36), 1));
	p.setBrush(Qt::NoBrush);
	p.drawPath(windowPath);

	const double r = windowH * 0.27;
	const double cy = window.center().y();
	const double centers[2] = {window.left() + windowW * 0.27, window.right() - windowW * 0.27};
	const QColor tape(66, 46, 34);
	p.save();
	p.setClipPath(windowPath);
	p.setPen(Qt::NoPen);
	p.setBrush(tape); // cinta enrollada: mas en el carrete de la izquierda
	p.drawEllipse(QPointF(centers[0], cy), r * 1.62, r * 1.62);
	p.drawEllipse(QPointF(centers[1], cy), r * 1.22, r * 1.22);
	p.restore();
	const double angle = t * 140.0;
	for (double cx : centers) {
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(232, 226, 213));
		p.drawEllipse(QPointF(cx, cy), r, r);
		p.setBrush(QColor(12, 12, 14));
		p.drawEllipse(QPointF(cx, cy), r * 0.5, r * 0.5);
		p.save(); // seis dientes que giran
		p.translate(cx, cy);
		p.rotate(angle);
		for (int i = 0; i < 6; i++) {
			p.rotate(60);
			p.fillRect(QRectF(-r * 0.08, -r * 0.52, r * 0.16, r * 0.22), QColor(232, 226, 213));
		}
		p.restore();
	}
}

// --------------------------------------------------------- equipo de sonido
// Texto como matriz de puntos: se dibuja pequeno y cada pixel pasa a ser un punto del display.
QImage dotMask(const OverlayStyle &s, const QString &text, int rows)
{
	QFont font;
	if (!s.fontFamily.isEmpty())
		font.setFamily(s.fontFamily);
	// Sin suavizado y con ajuste a la rejilla: trazos de un punto de ancho, como un display real.
	font.setPixelSize(qMax(6, int(rows * 0.8)));
	font.setStyleStrategy(QFont::NoAntialias);
	font.setHintingPreference(QFont::PreferFullHinting);
	const QFontMetrics fm(font);
	const int w = qMax(1, fm.horizontalAdvance(text) + 2);
	QImage mask(w, rows, QImage::Format_Grayscale8);
	mask.fill(0);
	QPainter mp(&mask);
	mp.setFont(font);
	mp.setPen(Qt::white);
	mp.drawText(QRect(1, 0, w, rows), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
	mp.end();
	return mask;
}

// Dibuja la matriz. Si el texto no cabe, se desplaza como una marquesina.
void drawDots(QPainter &p, const QImage &mask, const QRectF &area, int rows, double t, bool animate, const QColor &lit,
	      const QColor &dim)
{
	const double pitch = area.height() / rows;
	const int cols = qMax(1, int(area.width() / pitch));
	const double dot = pitch * 0.8;
	const bool scroll = mask.width() > cols;
	const int total = mask.width() + 14; // hueco entre vuelta y vuelta
	// arranca quieto un par de segundos para que se lea el principio
	const double cycle = total / 11.0 + 2.0;
	const double phase = animate ? std::fmod(t, cycle) : 0.0;
	const int offset = scroll ? int(qMax(0.0, phase - 2.0) * 11.0) : 0;
	for (int r = 0; r < rows; r++) {
		const uchar *line = mask.constScanLine(r);
		for (int c = 0; c < cols; c++) {
			const int src = scroll ? (c + offset) % total : c;
			const bool on = src < mask.width() && line[src] > 100;
			p.fillRect(QRectF(area.left() + c * pitch, area.top() + r * pitch, dot, dot), on ? lit : dim);
		}
	}
}

void drawLed(QPainter &p, const OverlayStyle &s, const OverlayData &d, double t)
{
	const NowText n = nowText(s, d);
	if (!n.visible)
		return;
	const double f = s.fontSize;
	OverlayStyle mono = s;
	mono.fontFamily = monoFamily();
	const QFont labelFont = makeFont(mono, f * 0.32, true, f * 0.05);
	const QFont subFont = makeFont(mono, f * 0.44, false);
	const QFontMetricsF lfm(labelFont), sfm(subFont);

	const int rows = 15;
	const double titleH = f * 1.2, pitch = titleH / rows;
	const QImage mask = dotMask(s, n.title, rows);
	const double bezel = f * 0.16, pad = f * 0.34, gap = f * 0.42;
	const double glassH = pad + (n.label.isEmpty() ? 0 : lfm.height() + f * 0.14) + titleH +
			      (n.sub.isEmpty() ? 0 : f * 0.16 + sfm.height()) + pad;
	const double spectrumW = f * 2.5;
	const double maxTextW = qMax(f * 4.0, s.width - bezel * 2 - pad * 2 - spectrumW - gap);
	const double wanted = qMax(qMax(mask.width() * pitch, textWidth(subFont, n.sub)), textWidth(labelFont, n.label) + f * 0.6);
	const double textW = qMin(qMax(wanted, f * 6.0), maxTextW);
	const QRectF card = placeCard(s, bezel * 2 + pad + spectrumW + gap + textW + pad, glassH + bezel * 2);

	// marco y cristal
	QPainterPath frame;
	frame.addRoundedRect(card, f * 0.22, f * 0.22);
	QLinearGradient metal(card.topLeft(), card.bottomLeft());
	metal.setColorAt(0, QColor(66, 68, 74));
	metal.setColorAt(0.5, QColor(38, 39, 43));
	metal.setColorAt(1, QColor(22, 23, 26));
	p.fillPath(frame, metal);
	const QRectF glass = card.adjusted(bezel, bezel, -bezel, -bezel);
	QPainterPath glassPath;
	glassPath.addRoundedRect(glass, f * 0.12, f * 0.12);
	p.fillPath(glassPath, kGlass);
	QColor glow = s.accentColor;
	glow.setAlpha(16);
	p.fillPath(glassPath, glow);

	const QColor lit = mix(s.accentColor, Qt::white, 0.3);
	QColor dim = s.accentColor;
	dim.setAlpha(17);
	QColor soft = s.accentColor;
	soft.setAlpha(190);

	// ecualizador: barras de segmentos que suben y bajan
	const int bars = 9, segments = 7;
	const QRectF spectrum(glass.left() + pad, glass.top() + pad, spectrumW, glassH - pad * 2);
	const double barW = spectrum.width() / (bars * 1.3 - 0.3);
	const double segH = spectrum.height() / (segments * 1.35 - 0.35);
	const double time = s.animate ? t : 0.0;
	for (int i = 0; i < bars; i++) {
		double v = 0.52 + 0.3 * std::sin(time * (2.3 + 0.41 * i) + i * 1.7) + 0.2 * std::sin(time * (4.1 + 0.23 * i) + i * 0.6);
		if (n.idle)
			v = 0.0;
		const int on = qBound(n.idle ? 0 : 1, int(v * segments + 0.5), segments);
		for (int k = 0; k < segments; k++) {
			const QRectF seg(spectrum.left() + i * barW * 1.3, spectrum.bottom() - (k + 1) * segH * 1.35 + segH * 0.35, barW, segH);
			QColor c = dim;
			if (k < on)
				c = k >= segments - 2 ? mix(s.accentColor, Qt::white, 0.6) : lit;
			p.fillRect(seg, c);
		}
	}

	// texto
	const double x = spectrum.right() + gap;
	double y = glass.top() + pad;
	if (!n.label.isEmpty()) {
		QPolygonF play; // triangulo de "reproduciendo"
		const double th = lfm.height() * 0.5;
		play << QPointF(x, y + lfm.height() / 2 - th / 2) << QPointF(x, y + lfm.height() / 2 + th / 2)
		     << QPointF(x + th * 0.85, y + lfm.height() / 2);
		p.setPen(Qt::NoPen);
		p.setBrush(soft);
		p.drawPolygon(play);
		drawLine(p, labelFont, soft, QRectF(x + th * 1.5, y, textW - th * 1.5, lfm.height()), n.label);
		y += lfm.height() + f * 0.14;
	}
	drawDots(p, mask, QRectF(x, y, textW, titleH), rows, t, s.animate, n.idle ? soft : lit, dim);
	y += titleH + f * 0.16;
	if (!n.sub.isEmpty())
		drawLine(p, subFont, soft, QRectF(x, y, textW, sfm.height()), n.sub);

	p.save();
	p.setClipPath(glassPath);
	drawScanlines(p, glass, f * 0.09);
	p.restore();
}

// ---------------------------------------------------------------------- vinilo
// Disco de vinilo visto de frente, girado `angle` grados. Se puede dibujar entero o por
// partes: lo que esta quieto (el disco) y lo unico que gira (lo impreso en la etiqueta).
enum DiscPart { DiscAll, DiscStill, DiscPrint };

void drawDiscPrint(QPainter &p, const QPointF &center, double discR, const QColor &accent, double angle)
{
	const double labelR = discR * 0.37;
	p.save(); // lo impreso en la etiqueta es lo que delata el giro
	p.translate(center);
	p.rotate(angle);
	const QColor print = isLight(accent) ? QColor(0, 0, 0, 150) : QColor(255, 255, 255, 190);
	p.fillRect(QRectF(-labelR * 0.55, -labelR * 0.62, labelR * 1.1, labelR * 0.13), print);
	p.fillRect(QRectF(-labelR * 0.38, -labelR * 0.4, labelR * 0.76, labelR * 0.08), print);
	p.fillRect(QRectF(-labelR * 0.45, labelR * 0.42, labelR * 0.9, labelR * 0.08), print);
	p.restore();
}

void drawDisc(QPainter &p, const QPointF &center, double discR, const QColor &accent, double angle, DiscPart part = DiscAll)
{
	if (part == DiscPrint) {
		drawDiscPrint(p, center, discR, accent, angle);
		return;
	}
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(12, 12, 13));
	p.drawEllipse(center, discR, discR);
	p.setBrush(Qt::NoBrush);
	for (int k = 0; k < 14; k++) { // surcos
		const double rr = discR * (0.44 + 0.52 * k / 13.0);
		p.setPen(QPen(QColor(255, 255, 255, k % 3 == 0 ? 26 : 12), 1));
		p.drawEllipse(center, rr, rr);
	}
	QConicalGradient sheen(center, 28); // brillo fijo: el disco gira, la luz no
	sheen.setColorAt(0.00, QColor(255, 255, 255, 44));
	sheen.setColorAt(0.07, QColor(255, 255, 255, 0));
	sheen.setColorAt(0.43, QColor(255, 255, 255, 0));
	sheen.setColorAt(0.50, QColor(255, 255, 255, 44));
	sheen.setColorAt(0.57, QColor(255, 255, 255, 0));
	sheen.setColorAt(0.93, QColor(255, 255, 255, 0));
	sheen.setColorAt(1.00, QColor(255, 255, 255, 44));
	p.setPen(Qt::NoPen);
	p.setBrush(sheen);
	p.drawEllipse(center, discR, discR);

	const double labelR = discR * 0.37;
	p.setBrush(accent);
	p.drawEllipse(center, labelR, labelR);
	if (part == DiscAll)
		drawDiscPrint(p, center, discR, accent, angle);
	p.setBrush(QColor(20, 18, 17));
	p.drawEllipse(center, discR * 0.045, discR * 0.045);
}

void drawVinyl(QPainter &p, const OverlayStyle &s, const OverlayData &d, double t)
{
	const NowText n = nowText(s, d);
	if (!n.visible)
		return;
	const double f = s.fontSize;
	const QFont titleFont = makeFont(s, f * 0.92, true);
	const QFont labelFont = makeFont(s, f * 0.32, true, f * 0.08);
	const QFont subFont = makeFont(s, f * 0.5, false);
	const QFontMetricsF tfm(titleFont), lfm(labelFont), sfm(subFont);

	const double pad = f * 0.46;
	const double textH = (n.label.isEmpty() ? 0 : lfm.height() + f * 0.1) + tfm.height() + (n.sub.isEmpty() ? 0 : sfm.height() + f * 0.04);
	const double cardH = qMax(textH + pad * 2, f * 2.9);
	const double discR = cardH / 2 - 1;
	const double sleeveX = discR * 1.32; // el disco asoma de la funda lo justo para ver la etiqueta girar
	const double maxTextW = qMax(f * 3.0, s.width - sleeveX - pad * 2);
	const double wanted = qMax(qMax(textWidth(titleFont, n.title), textWidth(subFont, n.sub)), textWidth(labelFont, n.label)) + 2;
	const double textW = qMin(qMax(wanted, f * 4.5), maxTextW);
	const QRectF card = placeCard(s, sleeveX + pad * 2 + textW, cardH);
	const QPointF center(card.left() + discR + 1, card.center().y());
	drawDisc(p, center, discR, s.accentColor, s.animate ? t * 200.0 : 0.0);

	// funda
	const QRectF sleeve(card.left() + sleeveX, card.top(), card.width() - sleeveX, cardH);
	p.fillRect(sleeve, kSleeve);
	QLinearGradient shade(sleeve.topLeft(), QPointF(sleeve.left() + f * 0.3, sleeve.top())); // la boca de la funda
	shade.setColorAt(0, QColor(0, 0, 0, 120));
	shade.setColorAt(1, QColor(0, 0, 0, 0));
	p.fillRect(QRectF(sleeve.left(), sleeve.top(), f * 0.3, sleeve.height()), shade);
	QColor line = s.accentColor;
	line.setAlpha(210);
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(line, qMax(1.0, f * 0.04)));
	const double m = f * 0.16;
	p.drawRect(sleeve.adjusted(m, m, -m, -m));

	const double x = sleeve.left() + pad;
	double y = sleeve.center().y() - textH / 2;
	if (!n.label.isEmpty()) {
		drawLine(p, labelFont, isLight(s.accentColor) ? s.accentColor : mix(s.accentColor, Qt::white, 0.25),
			 QRectF(x, y, textW, lfm.height()), n.label);
		y += lfm.height() + f * 0.1;
	}
	drawLine(p, titleFont, n.idle ? QColor(kCream.red(), kCream.green(), kCream.blue(), 150) : kCream,
		 QRectF(x, y, textW, tfm.height()), n.title);
	y += tfm.height() + f * 0.04;
	if (!n.sub.isEmpty())
		drawLine(p, subFont, QColor(kCream.red(), kCream.green(), kCream.blue(), 165), QRectF(x, y, textW, sfm.height()), n.sub);
}

// ------------------------------------------------------------------- portadas
// Las canciones no traen portada: se inventa una para cada una (cielo, sol o luna y un
// paisaje) a partir del titulo y el artista, asi que una cancion siempre tiene la misma.

quint32 songHash(const QString &title, const QString &artist)
{
	quint32 h = 2166136261u; // FNV-1a: igual en todos los sistemas y versiones
	const QString key = title.trimmed().toLower() + QChar(0x1f) + artist.trimmed().toLower();
	for (const QChar c : key) {
		h ^= c.unicode();
		h *= 16777619u;
	}
	return h;
}

struct Rng { // numeros al azar repetibles a partir de la cancion
	quint32 x;
	double next()
	{
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		return (x % 10000) / 10000.0;
	}
};

int coverHue(quint32 h)
{
	return int(h % 360);
}

QImage renderCover(quint32 h, int size)
{
	const int hue = coverHue(h);
	const QColor sky1 = QColor::fromHsv((hue + 330) % 360, 140, 240);
	const QColor sky2 = QColor::fromHsv(hue, 200, 165);
	const QColor sun = QColor::fromHsv((hue + 30) % 360, 70, 255);
	const QColor land = QColor::fromHsv((hue + 15) % 360, 170, 70);
	Rng rng{h | 1u};

	QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing, true);
	const double sz = size;
	QLinearGradient sky(0, 0, 0, sz);
	sky.setColorAt(0, sky1);
	sky.setColorAt(0.75, sky2);
	p.fillRect(img.rect(), sky);
	p.setPen(Qt::NoPen);
	for (int i = 0; i < 16; i++) { // estrellas
		p.setBrush(QColor(255, 255, 255, 60 + int(rng.next() * 120)));
		const double r = sz * (0.003 + rng.next() * 0.007);
		p.drawEllipse(QPointF(rng.next() * sz, rng.next() * sz * 0.55), r, r);
	}
	const QPointF c(sz * (0.3 + 0.4 * rng.next()), sz * (0.24 + 0.16 * rng.next()));
	const double sr = sz * (0.13 + 0.08 * rng.next());
	QRadialGradient glow(c, sr * 2.8);
	glow.setColorAt(0, QColor(sun.red(), sun.green(), sun.blue(), 110));
	glow.setColorAt(1, QColor(sun.red(), sun.green(), sun.blue(), 0));
	p.setBrush(glow);
	p.drawEllipse(c, sr * 2.8, sr * 2.8);
	QLinearGradient disk(c.x(), c.y() - sr, c.x(), c.y() + sr);
	disk.setColorAt(0, sun.lighter(110));
	disk.setColorAt(1, mix(sun, sky2, 0.35));
	p.setBrush(disk);
	p.drawEllipse(c, sr, sr);

	// paisaje: montanas, ciudad u olas segun la cancion
	const int kind = int((h >> 9) % 3);
	for (int layer = 0; layer < 3; layer++) {
		const double base = sz * (0.62 + layer * 0.12);
		const QColor col = mix(land, sky2, 0.55 - layer * 0.25);
		QPainterPath path;
		path.moveTo(0, sz);
		if (kind == 0) { // montanas
			path.lineTo(0, base);
			double x = 0;
			while (x < sz) {
				const double w = sz * (0.12 + rng.next() * 0.16);
				path.lineTo(x + w / 2, base - sz * (0.05 + rng.next() * 0.16));
				path.lineTo(x + w, base);
				x += w;
			}
		} else if (kind == 1) { // ciudad
			double x = 0;
			path.lineTo(0, base);
			while (x < sz) {
				const double w = sz * (0.04 + rng.next() * 0.07);
				const double top = base - sz * (0.02 + rng.next() * (0.24 - layer * 0.06));
				path.lineTo(x, top);
				path.lineTo(x + w, top);
				path.lineTo(x + w, base);
				x += w;
			}
		} else { // olas o dunas
			path.lineTo(0, base);
			const double amp = sz * (0.03 + rng.next() * 0.03), phase = rng.next() * 6.28;
			for (int i = 0; i <= 24; i++) {
				const double x = sz * i / 24.0;
				path.lineTo(x, base + amp * std::sin(phase + i * 0.55 + layer));
			}
		}
		path.lineTo(sz, sz);
		path.closeSubpath();
		p.setBrush(col);
		p.drawPath(path);
		if (kind == 1 && layer == 1) { // ventanas encendidas
			p.setBrush(QColor(255, 230, 170, 150));
			for (int i = 0; i < 18; i++)
				p.drawRect(QRectF(rng.next() * sz, base - rng.next() * sz * 0.16, sz * 0.008, sz * 0.012));
		}
	}
	// vineta y canto: aspecto de carton impreso
	QRadialGradient vignette(QPointF(sz / 2, sz / 2), sz * 0.75);
	vignette.setColorAt(0.6, QColor(0, 0, 0, 0));
	vignette.setColorAt(1, QColor(0, 0, 0, 90));
	p.setBrush(vignette);
	p.drawRect(img.rect());
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(QColor(255, 255, 255, 45), qMax(1.0, sz * 0.006)));
	p.drawRect(QRectF(0.5, 0.5, sz - 1, sz - 1));
	p.end();
	return img;
}

// ---------------------------------------------------------------- tocadiscos

double smooth(double a, double b, double x) // 0 antes de a, 1 despues de b, suave en medio
{
	if (x <= a)
		return 0;
	if (x >= b)
		return 1;
	const double t = (x - a) / (b - a);
	return t * t * (3 - 2 * t);
}

// Momentos del cambio de disco (segundos desde que empieza; kTurntableChange en total).
struct TurntableTimes {
	double armUp[2] = {0.0, 0.35}, textOut[2] = {0.0, 0.3}, discIn[2] = {0.3, 0.75}, sleeveOut[2] = {0.65, 1.05};
	double tint[2] = {0.7, 1.2}, sleeveIn[2] = {1.0, 1.45}, discOut[2] = {1.3, 1.75}, armDown[2] = {1.65, 2.0};
	double textIn[2] = {1.7, 2.1}, sheen[2] = {1.9, 2.7};
};

// Sobre que disco y en que posicion estan las piezas en un momento del cambio.
struct TurntablePose {
	bool showOld = false; // todavia se ve el disco que se va
	double discOut = 1;   // 1 = el disco asoma de la funda; 0 = guardado
	double sleeveShift = 0; // 0 = en su sitio; 1 = fuera, a la izquierda
	double arm = 1;       // 1 = brazo sobre el disco; 0 = levantado
	double oldText = 0, newText = 1, tint = 1, sheen = -1;
};

TurntablePose turntablePose(const OverlayData &d)
{
	const TurntableTimes T;
	TurntablePose pose;
	const double ct = d.changeT;
	if (ct < kTurntableChange) {
		pose.showOld = ct < T.sleeveIn[0] && !d.changeFromNothing && !d.oldTitle.isEmpty();
		if (pose.showOld) {
			pose.discOut = 1 - smooth(T.discIn[0], T.discIn[1], ct);
			pose.sleeveShift = smooth(T.sleeveOut[0], T.sleeveOut[1], ct);
		} else {
			pose.sleeveShift = 1 - smooth(T.sleeveIn[0], T.sleeveIn[1], ct);
			pose.discOut = smooth(T.discOut[0], T.discOut[1], ct);
		}
		pose.arm = ct < T.armDown[0] ? 1 - smooth(T.armUp[0], T.armUp[1], ct) : smooth(T.armDown[0], T.armDown[1], ct);
		pose.oldText = (d.changeFromNothing || d.oldTitle.isEmpty()) ? 0 : 1 - smooth(T.textOut[0], T.textOut[1], ct);
		pose.newText = smooth(T.textIn[0], T.textIn[1], ct);
		pose.tint = (d.changeFromNothing || d.oldTitle.isEmpty()) ? 1 : smooth(T.tint[0], T.tint[1], ct);
		if (ct >= T.sheen[0] && ct < T.sheen[1])
			pose.sheen = (ct - T.sheen[0]) / (T.sheen[1] - T.sheen[0]);
	}
	pose.arm = qMin(pose.arm, qBound(0.0, d.arm, 1.0));
	return pose;
}

void drawTurntable(QPainter &p, const OverlayStyle &s, const OverlayData &d, double t)
{
	if (!d.hasPlaying && !d.lastSong && d.changeT >= kTurntableChange)
		return;
	const TurntablePose pose = turntablePose(d);
	const double f = qMax(10.0, qMin(double(s.fontSize), (s.height - 6) / 3.5));
	const double S = f * 3.4; // lado de la funda
	const double R = S * 0.45; // radio del disco
	const double opacity = p.opacity();

	const QFont labelFont = makeFont(s, f * 0.3, true, f * 0.08);
	const QFont titleFont = makeFont(s, f * 0.95, true);
	const QFont subFont = makeFont(s, f * 0.46, false);
	const QFont infoFont = makeFont(s, f * 0.32, false);
	const QFontMetricsF lfm(labelFont), tfm(titleFont), sfm(subFont), ifm(infoFont);

	// textos de la cancion que entra y de la que se va
	const auto sub = [&](const QString &artist, const QStringList &by) {
		QString out = s.showArtist ? artist : QString();
		if (s.showRequester && !by.isEmpty()) {
			const QString who = d.lblRequestedBy.arg(by.join(QStringLiteral(", ")));
			out = out.isEmpty() ? who : out + kDot + who;
		}
		return out;
	};
	const QString label = (d.lastSong ? d.lblLastSong : (s.label.trimmed().isEmpty() ? d.lblNowPlaying : s.label.trimmed())).toUpper();
	QString info;
	if (d.hasPlaying && d.elapsed >= 0)
		info = QStringLiteral("%1:%2").arg(int(d.elapsed) / 60).arg(int(d.elapsed) % 60, 2, 10, QLatin1Char('0'));
	if (!d.nextTitle.isEmpty()) {
		const QString next = d.lblNext.arg(d.nextArtist.isEmpty() ? d.nextTitle : d.nextTitle + kDash + d.nextArtist);
		info = info.isEmpty() ? next : info + kDot + next;
	}

	// medidas: funda y disco a la izquierda, tarjeta detras, texto a la derecha del brazo
	const double textX0 = S * 0.55 + R * 2.2 + f * 0.9;
	// la tarjeta ocupa todo el ancho de la fuente: no cambia de tamano de una cancion a otra
	const double textW = qMax(f * 5.0, s.width - 4 - textX0 - f * 0.8);
	const QRectF block = placeCard(s, textX0 + textW + f * 0.8, S + 4);
	const double x0 = block.left() + 2, y0 = block.top() + 2;
	const QRectF card(x0 + S * 0.5, y0 + S * 0.12, block.right() - 2 - (x0 + S * 0.5), S * 0.88);

	// tarjeta: el color sale de la portada (y pasa de una a otra durante el cambio)
	const QColor newDark = coverDark(d.title, d.artist);
	const QColor tint = pose.tint >= 1 ? newDark : mix(coverDark(d.oldTitle, d.oldArtist), newDark, pose.tint);
	QPainterPath cardPath;
	cardPath.addRoundedRect(card, f * 0.3, f * 0.3);
	QLinearGradient cardGrad(card.topLeft(), card.bottomRight());
	cardGrad.setColorAt(0, tint.lighter(125));
	cardGrad.setColorAt(1, tint.darker(150));
	p.fillPath(cardPath, cardGrad);
	p.save();
	p.setClipPath(cardPath);
	QLinearGradient edge(card.topLeft(), card.topRight());
	edge.setColorAt(0, QColor(255, 255, 255, 0));
	edge.setColorAt(0.5, QColor(255, 255, 255, 120));
	edge.setColorAt(1, QColor(255, 255, 255, 30));
	p.fillRect(QRectF(card.left(), card.top(), card.width(), qMax(1.0, f * 0.04)), edge);
	p.restore();

	// funda y disco (el disco va detras de la funda)
	const QString sleeveTitle = pose.showOld ? d.oldTitle : d.title;
	const QString sleeveArtist = pose.showOld ? d.oldArtist : d.artist;
	const double sleeveDx = -pose.sleeveShift * (x0 + S * 1.25);
	const QRectF sleeve(x0 + sleeveDx, y0, S, S);
	const QPointF discC(sleeve.center().x() + S * 0.55 * pose.discOut, sleeve.center().y());
	const double spin = s.animate ? t * 200.0 : 0.0;
	if (pose.sleeveShift < 1) {
		p.save();
		p.setOpacity(opacity * (1 - pose.sleeveShift));
		drawDisc(p, discC, R, coverLight(sleeveTitle, sleeveArtist), spin, DiscStill);
		// la etiqueta del disco es la portada en pequeno, girando
		const double labelR = R * 0.37;
		p.save();
		QPainterPath labelPath;
		labelPath.addEllipse(discC, labelR, labelR);
		p.setClipPath(labelPath);
		p.translate(discC);
		p.rotate(spin);
		p.drawImage(QRectF(-labelR, -labelR, labelR * 2, labelR * 2), coverArt(sleeveTitle, sleeveArtist, int(labelR * 2 + 1)));
		p.restore();
		p.setBrush(QColor(20, 18, 17));
		p.setPen(Qt::NoPen);
		p.drawEllipse(discC, R * 0.04, R * 0.04);

		// la funda: entra y sale ladeada, con sombra
		p.translate(sleeve.center());
		p.rotate(-9.0 * pose.sleeveShift);
		const QRectF local(-S / 2, -S / 2, S, S);
		QPainterPath sleevePath;
		sleevePath.addRoundedRect(local, f * 0.12, f * 0.12);
		for (int i = 3; i >= 1; i--) {
			QPainterPath shadow;
			shadow.addRoundedRect(local.adjusted(-i, -i * 0.3, i * 1.5, i * 2.0), f * 0.12 + i, f * 0.12 + i);
			p.fillPath(shadow, QColor(0, 0, 0, 22));
		}
		p.save();
		p.setClipPath(sleevePath);
		p.drawImage(local, coverArt(sleeveTitle, sleeveArtist, int(S + 1)));
		if (pose.sheen >= 0) { // reflejo que cruza la funda nueva
			const double bx = -S + pose.sheen * S * 2.2;
			QLinearGradient band(QPointF(bx - S * 0.2, 0), QPointF(bx + S * 0.2, 0));
			band.setColorAt(0, QColor(255, 255, 255, 0));
			band.setColorAt(0.5, QColor(255, 255, 255, 90));
			band.setColorAt(1, QColor(255, 255, 255, 0));
			p.rotate(15);
			p.fillRect(QRectF(bx - S * 0.2, -S, S * 0.4, S * 2), band);
		}
		p.restore();
		p.restore();
	}

	// brazo: pivote arriba a la derecha del disco; reposa vertical y baja hasta el disco
	const QPointF homeDisc(x0 + S * 0.5 + S * 0.55, y0 + S * 0.5);
	const QPointF pivot(homeDisc.x() + R * 1.02, y0 + S * 0.2);
	const QPointF playTip(homeDisc.x() + R * 0.22, homeDisc.y() + R * 0.6);
	const double L = std::hypot(playTip.x() - pivot.x(), playTip.y() - pivot.y());
	const double playAngle = std::atan2(pivot.x() - playTip.x(), playTip.y() - pivot.y());
	const double angle = playAngle * pose.arm;
	const QPointF tip(pivot.x() - L * std::sin(angle), pivot.y() + L * std::cos(angle));
	p.save();
	p.setOpacity(opacity);
	QLinearGradient metal(pivot, tip);
	metal.setColorAt(0, QColor(235, 235, 240));
	metal.setColorAt(1, QColor(170, 172, 180));
	p.setPen(QPen(QBrush(metal), qMax(2.0, f * 0.09), Qt::SolidLine, Qt::RoundCap));
	p.drawLine(pivot, tip);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(200, 200, 206));
	p.translate(tip);
	p.rotate(-angle * 180.0 / M_PI);
	p.drawRoundedRect(QRectF(-f * 0.11, -f * 0.05, f * 0.22, f * 0.2), f * 0.04, f * 0.04);
	p.resetTransform();
	QRadialGradient ball(pivot - QPointF(f * 0.08, f * 0.08), f * 0.36);
	ball.setColorAt(0, QColor(255, 255, 255));
	ball.setColorAt(1, QColor(150, 152, 160));
	p.setBrush(ball);
	p.drawEllipse(pivot, f * 0.27, f * 0.27);
	p.restore();

	// texto: el de la cancion que se va se desvanece y el nuevo aparece al bajar el brazo
	const QColor accent = coverLight(d.title, d.artist);
	const double tx = x0 + textX0;
	const double tw = card.right() - f * 0.8 - tx + 4;
	const auto drawText = [&](double alpha, const QString &title, const QString &artist, const QStringList &by, const QColor &acc,
				  bool withInfo) {
		if (alpha <= 0.001)
			return;
		const QString subText = sub(artist, by);
		const double gap = f * 0.08;
		double h = lfm.height() + gap + tfm.height() + (subText.isEmpty() ? 0 : sfm.height()) +
			   ((withInfo && !info.isEmpty()) ? gap * 2 + ifm.height() : 0);
		double y = card.center().y() - h / 2;
		p.save();
		p.setOpacity(opacity * alpha);
		if (!label.isEmpty() && s.showLabel) {
			// ecualizador pequeno junto a la etiqueta; se mueve mientras suena
			const double bw = f * 0.07, bh = lfm.height() * 0.7;
			for (int i = 0; i < 4; i++) {
				const double v = d.hasPlaying && s.animate ? 0.35 + 0.65 * std::fabs(std::sin(t * (3.1 + i * 0.9) + i * 1.3)) : 0.35;
				p.fillRect(QRectF(tx + i * bw * 1.7, y + lfm.height() / 2 + bh / 2 - bh * v, bw, bh * v), acc);
			}
			drawLine(p, labelFont, acc, QRectF(tx + bw * 8, y, tw - bw * 8, lfm.height()), label);
		}
		y += lfm.height() + gap;
		drawLine(p, titleFont, s.textColor, QRectF(tx, y, tw, tfm.height()), title);
		y += tfm.height();
		if (!subText.isEmpty()) {
			drawLine(p, subFont, QColor(255, 255, 255, 190), QRectF(tx, y, tw, sfm.height()), subText);
			y += sfm.height();
		}
		if (withInfo && !info.isEmpty())
			drawLine(p, infoFont, QColor(255, 255, 255, 140), QRectF(tx, y + gap * 2, tw, ifm.height()), info);
		p.restore();
	};
	drawText(pose.oldText, d.oldTitle, d.oldArtist, d.oldRequesters, coverLight(d.oldTitle, d.oldArtist), false);
	drawText(pose.newText, d.title, d.artist, d.requesters, accent, true);
}

// Cabecera comun de cola y lista. Devuelve la altura ocupada.
double drawHeader(QPainter &p, const OverlayStyle &s, const QRectF &inner, const QString &left, const QString &right)
{
	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const QFont rightFont = makeFont(s, s.fontSize * 0.36, false);
	const QFontMetricsF lfm(labelFont);
	const double h = lfm.height();
	double rightW = 0;
	if (!right.isEmpty()) {
		rightW = qMin(textWidth(rightFont, right) + 2, inner.width() * 0.6);
		p.setFont(rightFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.right() - rightW, inner.top(), rightW, h),
			   Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, elide(rightFont, right, rightW));
	}
	p.setFont(labelFont);
	p.setPen(s.accentColor);
	const double leftW = inner.width() - rightW - (rightW > 0 ? s.fontSize * 0.4 : 0);
	p.drawText(QRectF(inner.left(), inner.top(), leftW, h), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
		   elide(labelFont, left.toUpper(), leftW));

	const double lineY = inner.top() + h + s.fontSize * 0.2;
	QColor line = s.mutedColor;
	line.setAlphaF(line.alphaF() * 0.35);
	p.fillRect(QRectF(inner.left(), lineY, inner.width(), qMax(1.0, s.fontSize * 0.03)), line);
	return h + s.fontSize * 0.42;
}

void drawQueue(QPainter &p, const OverlayStyle &s, const OverlayData &d)
{
	if (d.queue.isEmpty() && s.hideWhenEmpty)
		return;

	const QFont rowFont = makeFont(s, s.fontSize * 0.6, s.bold);
	const QFont rowFontLight = makeFont(s, s.fontSize * 0.6, false);
	const QFont numFont = makeFont(s, s.fontSize * 0.6, true);
	const QFont byFont = makeFont(s, s.fontSize * 0.42, false);
	const QFontMetricsF rfm(rowFont);
	const double rowH = rfm.height() * 1.28;
	const double pad = s.padding;
	const int shown = qMin(int(d.queue.size()), qMax(1, s.rows));
	const int extra = d.queueTotal - shown;

	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const double headerH = s.showLabel ? QFontMetricsF(labelFont).height() + s.fontSize * 0.42 : 0;
	double bodyH = shown > 0 ? shown * rowH : rowH;
	if (extra > 0)
		bodyH += QFontMetricsF(byFont).height() * 1.3;

	const QRectF card = placeCard(s, s.width, headerH + bodyH + pad * 2);
	drawPanel(p, s, card);
	const QRectF inner = card.adjusted(pad, pad, -pad, -pad);
	double y = inner.top();

	if (s.showLabel) {
		QString label = s.label.trimmed().isEmpty() ? d.lblQueue : s.label.trimmed();
		if (d.queueTotal > 0)
			label += QStringLiteral("  \u00b7  %1").arg(d.queueTotal);
		// Como pedir: a la vista siempre, tambien cuando ya hay canciones en la cola.
		const QString right = !d.requestsOpen ? d.lblClosed : (s.showHowTo ? d.lblHowTo : QString());
		y += drawHeader(p, s, inner, label, right);
	}

	if (shown == 0) {
		p.setFont(rowFontLight);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left(), y, inner.width(), rowH),
			   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(rowFontLight, d.requestsOpen ? d.lblEmptyQueue : d.lblClosed, inner.width()));
		finishPanel(p, s, card);
		return;
	}

	const double numW = s.showNumbers ? textWidth(numFont, QString::number(qMax(shown, 9))) + s.fontSize * 0.4 : 0;
	for (int i = 0; i < shown; i++) {
		const OverlayRow &row = d.queue[i];
		double x = inner.left();
		double right = inner.right();
		if (s.showNumbers) {
			p.setFont(numFont);
			p.setPen(s.accentColor);
			p.drawText(QRectF(x, y, numW, rowH), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				   QString::number(i + 1));
			x += numW;
		}
		if (s.showRequester && !row.requester.isEmpty()) {
			const double byW = qMin(textWidth(byFont, row.requester) + 2, (right - x) * 0.38);
			p.setFont(byFont);
			p.setPen(s.mutedColor);
			p.drawText(QRectF(right - byW, y, byW, rowH), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
				   elide(byFont, row.requester, byW));
			right -= byW + s.fontSize * 0.4;
		}
		const QString artist = s.showArtist && !row.artist.isEmpty() ? kDash + row.artist : QString();
		// el artista va en trazo fino para que el titulo destaque
		const QFontMetricsF fm(rowFont);
		const double titleW = fm.horizontalAdvance(row.title);
		const QRectF r(x, y, right - x, rowH);
		if (artist.isEmpty() || titleW >= r.width() - fm.averageCharWidth() * 3) {
			p.setFont(rowFont);
			p.setPen(s.textColor);
			p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(rowFont, row.title, r.width()));
		} else {
			p.setFont(rowFont);
			p.setPen(s.textColor);
			p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, row.title);
			QRectF rest = r;
			rest.setLeft(r.left() + titleW);
			p.setFont(rowFontLight);
			p.setPen(s.mutedColor);
			p.drawText(rest, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				   elide(rowFontLight, artist, rest.width()));
		}
		rowRule(p, s, inner.left(), inner.right(), y + rowH);
		y += rowH;
	}
	if (extra > 0) {
		p.setFont(byFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left() + numW, y, inner.width() - numW, QFontMetricsF(byFont).height() * 1.3),
			   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, d.lblMore.arg(extra));
	}
	finishPanel(p, s, card);
}

void drawSongList(QPainter &p, const OverlayStyle &s, const OverlayData &d)
{
	if (d.songs.isEmpty() && s.hideWhenEmpty)
		return;

	const int cols = qMax(1, s.columns);
	const int rows = qMax(1, s.rows);
	const QFont rowFont = makeFont(s, s.fontSize * 0.52, false);
	const QFont numFont = makeFont(s, s.fontSize * 0.52, true);
	const QFontMetricsF rfm(rowFont);
	const double rowH = rfm.height() * 1.3;
	const double pad = s.padding;

	const QFont labelFont = makeFont(s, s.fontSize * 0.36, true, s.fontSize * 0.07);
	const double headerH = s.showLabel ? QFontMetricsF(labelFont).height() + s.fontSize * 0.42 : 0;
	const int usedRows = d.pages > 1 ? rows : qMax(1, qMin(rows, int((d.songs.size() + cols - 1) / cols)));

	const QRectF card = placeCard(s, s.width, headerH + usedRows * rowH + pad * 2);
	drawPanel(p, s, card);
	const QRectF inner = card.adjusted(pad, pad, -pad, -pad);
	double top = inner.top();

	if (s.showLabel) {
		QString label = s.label.trimmed().isEmpty() ? d.lblSongList : s.label.trimmed();
		if (d.songsTotal > 0)
			label += QStringLiteral("  ·  %1").arg(d.songsTotal);
		QString right = !d.requestsOpen ? d.lblClosed : (s.showHowTo ? d.lblHowTo : QString());
		if (s.showWeb && !d.webUrl.isEmpty()) // donde ver la lista entera
			right += (right.isEmpty() ? QString() : QStringLiteral("      ")) + (d.lblWeb.contains(QLatin1String("%1")) ? d.lblWeb.arg(d.webUrl) : d.webUrl);
		if (d.pages > 1)
			right += QStringLiteral("    %1 / %2").arg(d.page + 1).arg(d.pages);
		top += drawHeader(p, s, inner, label, right);
	}

	if (d.songs.isEmpty()) {
		p.setFont(rowFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left(), top, inner.width(), rowH),
			   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, d.lblEmptyList);
		finishPanel(p, s, card);
		return;
	}

	int maxNumber = 9;
	for (const OverlayRow &row : d.songs)
		maxNumber = qMax(maxNumber, row.number);
	const double numW = s.showNumbers ? textWidth(numFont, QString::number(maxNumber)) + s.fontSize * 0.3 : 0;
	const double colGap = s.fontSize * 0.8;
	const double colW = (inner.width() - colGap * (cols - 1)) / cols;

	// se llena por columnas: se lee de arriba abajo, como una lista
	for (int i = 0; i < d.songs.size() && i < rows * cols; i++) {
		const OverlayRow &row = d.songs[i];
		const int c = i / rows, r = i % rows;
		double x = inner.left() + c * (colW + colGap);
		const double y = top + r * rowH;
		if (s.showNumbers) {
			p.setFont(numFont);
			p.setPen(s.accentColor);
			p.drawText(QRectF(x, y, numW - s.fontSize * 0.3, rowH),
				   Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, QString::number(row.number));
			x += numW;
		}
		const QString artist = s.showArtist && !row.artist.isEmpty() ? kDash + row.artist : QString();
		drawTwoTone(p, s, rowFont, QRectF(x, y, inner.left() + c * (colW + colGap) + colW - x, rowH), row.title,
			    artist);
		rowRule(p, s, inner.left() + c * (colW + colGap), inner.left() + c * (colW + colGap) + colW, y + rowH);
	}
	finishPanel(p, s, card);
}

// ----------------------------------------------------------------------- barra
// Una sola fuente, pensada para el pie de la pantalla: una franja de cristal de borde a
// borde con lo que suena (izquierda), la cola (centro) y la lista de canciones (derecha),
// separados por lineas finas. Cada parte se enciende y se apaga desde el panel de OBS;
// las que quedan encendidas se reparten el ancho.

const double kBarListScale = 0.52;  // letra de la lista respecto al titulo
const double kBarQueueScale = 0.58; // letra de la cola

OverlayStyle clamped(const OverlayStyle &style)
{
	OverlayStyle s = style;
	s.width = qBound(16, s.width, 4096);
	s.height = qBound(16, s.height, 4096);
	s.fontSize = qBound(10, s.fontSize, 400);
	return s;
}

// Orden en pantalla, de izquierda a derecha.
const int kBarOrder[3] = {OverlayStyle::BarNow, OverlayStyle::BarQueue, OverlayStyle::BarList};

// Tema de la tarjeta: colores, fondo, borde y detalles.
struct CardTheme {
	QColor bgTop, bgBottom; // fondo; en degradado lateral, de izquierda a derecha
	bool sideways = false;
	QColor text, muted, accent, accent2, dot, border;
	double radius = 0.32;  // esquinas de la tarjeta, por el tamano de letra
	double round = 0.24;   // esquinas de portada e iconos, por su tamano (0.5 = circulo)
	int glow = 0;          // brillo del borde en el color de acento
	bool chipTint = false; // etiquetas tenidas del acento
	bool sheen = false;    // reflejo de cristal arriba
	double topLine = 1.0;  // fuerza de la linea de acento de arriba
};

struct BarGeom {
	CardTheme theme;
	double f = 0;   // tamano del titulo; el resto de la letra sale de aqui
	double pad = 0; // margen interior de cada parte
	double headerH = 0, listRowH = 0, queueRowH = 0;
	QRectF panel;   // la franja entera
	QRectF rect[3]; // cada parte (por BarSection); vacio si esta apagada
	int listRows = 1, listCols = 1, queueRows = 1;
};

BarGeom barGeom(const OverlayStyle &s, const bool on[3])
{
	BarGeom g;
	// el tamano elegido manda, mientras quepa en el alto de la barra
	g.f = qMax(10.0, qMin(double(s.fontSize), s.height * 0.3));
	const double h = s.height;
	g.panel = QRectF(0, 0, s.width, h);
	g.pad = qMin(double(qMax(s.padding, 6)), h * 0.16);

	// "sonando" ocupa el porcentaje elegido; lista y cola se reparten el resto
	const double nowShare = qBound(20, s.barCenterPercent, 60) / 100.0;
	double weight[3] = {0, 0, 0};
	weight[OverlayStyle::BarNow] = on[OverlayStyle::BarNow] ? nowShare : 0;
	weight[OverlayStyle::BarList] = on[OverlayStyle::BarList] ? (1 - nowShare) / 2 : 0;
	weight[OverlayStyle::BarQueue] = on[OverlayStyle::BarQueue] ? (1 - nowShare) / 2 : 0;
	const double sum = weight[0] + weight[1] + weight[2];
	double x = 0;
	for (int idx : kBarOrder) {
		if (weight[idx] <= 0 || sum <= 0)
			continue;
		const double w = s.width * weight[idx] / sum;
		g.rect[idx] = QRectF(x, 0, w, h);
		x += w;
	}

	const QFont labelFont = makeFont(s, g.f * 0.36, true, g.f * 0.07);
	g.headerH = s.showLabel ? QFontMetricsF(labelFont).height() + g.f * 0.42 : 0;
	g.listRowH = QFontMetricsF(makeFont(s, g.f * kBarListScale, false)).height() * 1.2;
	g.queueRowH = QFontMetricsF(makeFont(s, g.f * kBarQueueScale, s.bold)).height() * 1.22;
	const double innerH = h - g.pad * 2 - g.headerH;
	g.listRows = qMax(1, int((innerH + 1.0) / g.listRowH));
	g.queueRows = qMax(1, int((innerH + 1.0) / g.queueRowH));
	// las columnas pedidas, salvo que quedaran demasiado estrechas para leerse con las
	// tres partes encendidas (asi la pagina no cambia de tamano al apagar una)
	const double listW = s.width * (1 - nowShare) / 2;
	g.listCols = qBound(1, s.columns, qMax(1, int((listW - g.pad * 2) / (g.f * kBarListScale * 12.5))));

	if (s.barStyle == OverlayStyle::BarCard) {
		// tarjeta: un pequeno margen alrededor (esquinas redondeadas y sombra) y las partes dentro
		const double m = qMax(3.0, g.f * 0.14);
		g.panel = QRectF(m, m, qMax(24.0, s.width - m * 2), qMax(8.0, h - m * 2));
		const double sx = g.panel.width() / qMax(1, s.width);
		for (QRectF &r : g.rect)
			if (!r.isEmpty())
				r = QRectF(g.panel.left() + r.left() * sx, g.panel.top(), r.width() * sx, g.panel.height());
		g.pad = qMin(double(qMax(s.padding, 8)), g.panel.height() * 0.15);
		const double labelH = QFontMetricsF(makeFont(s, g.f * 0.28, true)).height();
		const double footH = QFontMetricsF(makeFont(s, g.f * 0.26, true)).height();
		g.listRowH = QFontMetricsF(makeFont(s, g.f * 0.40, false)).height() * 1.12;
		const double listArea = g.panel.height() - g.pad * 1.5 - labelH * 1.9 - g.f * 0.14 - footH - g.f * 0.28 - qMax(2.0, g.f * 0.1);
		g.listRows = qMax(1, int((listArea + 1.0) / g.listRowH));
		g.listCols = qBound(1, s.columns, qMax(1, int((listW * sx - g.pad * 2) / (g.f * 0.40 * 12.5))));
		g.queueRows = 30; // la cinta pasa todos los pedidos
	}
	return g;
}

// La franja: cristal translucido con brillo arriba, un filo de luz en el borde superior y
// lineas finas entre las partes. (Una fuente de OBS no ve lo que tiene detras, asi que
// no puede desenfocarlo.)
void drawBarBackground(QPainter &p, const OverlayStyle &s, const BarGeom &g)
{
	const QRectF r = g.panel;
	if (s.backgroundOpacity > 0) {
		QColor tint = s.backgroundColor;
		tint.setAlphaF(qBound(0, s.backgroundOpacity, 100) / 100.0); // 100 = solido; por debajo empieza a transparentar
		p.fillRect(r, tint);
		QLinearGradient shine(r.topLeft(), r.bottomLeft());
		shine.setColorAt(0.0, QColor(255, 255, 255, 40));
		shine.setColorAt(0.4, QColor(255, 255, 255, 10));
		shine.setColorAt(1.0, QColor(255, 255, 255, 0));
		p.fillRect(r, shine);
		const double edge = qMax(1.0, g.f * 0.04);
		p.fillRect(QRectF(r.left(), r.top(), r.width(), edge), QColor(255, 255, 255, 110));
	}
	// separadores entre las partes encendidas
	QColor line = s.textColor;
	line.setAlpha(46);
	const double lw = qMax(1.0, g.f * 0.03);
	bool first = true;
	for (int idx : kBarOrder) {
		if (g.rect[idx].isEmpty())
			continue;
		if (!first)
			p.fillRect(QRectF(g.rect[idx].left() - lw / 2, r.top() + g.pad, lw, r.height() - g.pad * 2), line);
		first = false;
	}
}

void drawBarList(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g)
{
	const QRectF inner = g.rect[OverlayStyle::BarList].adjusted(g.pad, g.pad, -g.pad, -g.pad);
	double top = inner.top();
	if (s.showLabel) {
		// la direccion de la pagina web, para quien quiera ver la lista entera;
		// con ella el titulo va en corto para que quepan los dos
		QString right = (s.showWeb && !d.webUrl.isEmpty()) ? d.webUrl : QString();
		QString label = (right.isEmpty() || d.lblSongs.isEmpty()) ? d.lblSongList : d.lblSongs;
		if (d.songsTotal > 0)
			label += QStringLiteral("  ·  %1").arg(d.songsTotal);
		if (d.pages > 1)
			right += (right.isEmpty() ? QString() : QStringLiteral("    ")) + QStringLiteral("%1 / %2").arg(d.page + 1).arg(d.pages);
		top += drawHeader(p, s, inner, label, right);
	}
	const QFont rowFont = makeFont(s, g.f * kBarListScale, false);
	const QFont numFont = makeFont(s, g.f * kBarListScale, true);
	if (d.songs.isEmpty()) {
		p.setFont(rowFont);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left(), top, inner.width(), g.listRowH),
			   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(rowFont, d.lblEmptyList, inner.width()));
		return;
	}
	int maxNumber = 9;
	for (const OverlayRow &row : d.songs)
		maxNumber = qMax(maxNumber, row.number);
	const double numGap = g.f * 0.26;
	const double numW = s.showNumbers ? textWidth(numFont, QString::number(maxNumber)) + numGap : 0;
	const double colGap = g.f * 0.6;
	const double colW = (inner.width() - colGap * (g.listCols - 1)) / g.listCols;
	for (int i = 0; i < d.songs.size() && i < g.listRows * g.listCols; i++) {
		const OverlayRow &row = d.songs[i];
		const int c = i / g.listRows, r = i % g.listRows;
		const double left = inner.left() + c * (colW + colGap);
		const double y = top + r * g.listRowH;
		double x = left;
		if (s.showNumbers) {
			p.setFont(numFont);
			p.setPen(s.accentColor);
			p.drawText(QRectF(x, y, numW - numGap, g.listRowH), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
				   QString::number(row.number));
			x += numW;
		}
		const QString artist = s.showArtist && !row.artist.isEmpty() ? kDash + row.artist : QString();
		drawTwoTone(p, s, rowFont, QRectF(x, y, left + colW - x, g.listRowH), row.title, artist);
	}
}

// Capas de la barra: todo de una vez, solo lo quieto o solo lo que gira.
enum BarLayer { BarAll, BarStill, BarMoving };

// El disco esta siempre; gira mientras suena algo (quien llama congela el reloj t al parar,
// asi se queda quieto en la posicion en que estaba). El texto entra y sale con cada cancion
// con su propio fundido (textOpacity).
void drawBarNow(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g, double t, BarLayer layer,
		double textOpacity)
{
	const QRectF inner = g.rect[OverlayStyle::BarNow].adjusted(g.pad, g.pad, -g.pad, -g.pad);
	const double discR = qMin(inner.height() / 2, inner.width() * 0.2);
	const QPointF c(inner.left() + discR, inner.center().y());
	const double angle = s.animate ? t * 200.0 : 0.0;
	if (layer == BarMoving) {
		drawDisc(p, c, discR, s.accentColor, angle, DiscPrint);
		return;
	}
	drawDisc(p, c, discR, s.accentColor, angle, layer == BarAll ? DiscAll : DiscStill);
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(QColor(255, 255, 255, 46), qMax(1.0, g.f * 0.03))); // canto de luz: separa el disco del cristal
	p.drawEllipse(c, discR, discR);
	p.setPen(Qt::NoPen);

	const NowText n = nowText(s, d);
	if (!n.visible || textOpacity <= 0.001)
		return; // sin cancion (ni texto de espera): solo el disco
	p.setOpacity(p.opacity() * textOpacity);

	const double x = inner.left() + discR * 2 + g.f * 0.5;
	const double w = qMax(8.0, inner.right() - x);
	// un titulo largo encoge un poco antes de cortarse
	double titlePx = g.f;
	const double natural = textWidth(makeFont(s, titlePx, s.bold), n.title);
	if (natural > w)
		titlePx = qMax(g.f * 0.7, titlePx * w / natural);
	const QFont titleFont = makeFont(s, titlePx, s.bold);
	const QFont labelFont = makeFont(s, g.f * 0.36, true, g.f * 0.07);
	const QFont subFont = makeFont(s, g.f * 0.56, false);
	const QFontMetricsF tfm(titleFont), lfm(labelFont), sfm(subFont);
	const double gap = g.f * 0.1;
	const double textH = (n.label.isEmpty() ? 0 : lfm.height() + gap) + tfm.height() + (n.sub.isEmpty() ? 0 : sfm.height() + gap * 0.5);
	double y = inner.center().y() - textH / 2;
	if (!n.label.isEmpty()) {
		drawLine(p, labelFont, s.accentColor, QRectF(x, y, w, lfm.height()), n.label);
		y += lfm.height() + gap;
	}
	drawLine(p, titleFont, n.idle ? s.mutedColor : s.textColor, QRectF(x, y, w, tfm.height()), n.title);
	y += tfm.height() + gap * 0.5;
	if (!n.sub.isEmpty())
		drawLine(p, subFont, s.mutedColor, QRectF(x, y, w, sfm.height()), n.sub);
}

void drawBarQueue(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g)
{
	const QRectF inner = g.rect[OverlayStyle::BarQueue].adjusted(g.pad, g.pad, -g.pad, -g.pad);
	double y = inner.top();
	if (s.showLabel) {
		QString label = d.lblQueue;
		if (d.queueTotal > 0)
			label += QStringLiteral("  ·  %1").arg(d.queueTotal);
		const QString right = !d.requestsOpen ? d.lblClosed : (s.showHowTo ? d.lblHowTo : QString());
		y += drawHeader(p, s, inner, label, right);
	}
	const QFont rowFont = makeFont(s, g.f * kBarQueueScale, s.bold);
	const QFont rowFontLight = makeFont(s, g.f * kBarQueueScale, false);
	const QFont numFont = makeFont(s, g.f * kBarQueueScale, true);
	const QFont byFont = makeFont(s, g.f * 0.44, false);
	int shown = qMin(int(d.queue.size()), g.queueRows);
	// si no caben todos, la ultima fila avisa de cuantos mas hay
	if (d.queueTotal > shown && g.queueRows >= 2)
		shown = qMin(shown, g.queueRows - 1);
	const int extra = d.queueTotal - shown;
	if (shown == 0) {
		p.setFont(rowFontLight);
		p.setPen(s.mutedColor);
		// si los pedidos estan cerrados ya lo dice la cabecera
		p.drawText(QRectF(inner.left(), y, inner.width(), g.queueRowH), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(rowFontLight, (d.requestsOpen || s.showLabel) ? d.lblEmptyQueue : d.lblClosed, inner.width()));
		return;
	}
	const QFontMetricsF fm(rowFont);
	const double numW = s.showNumbers ? textWidth(numFont, QString::number(qMax(shown, 9))) + g.f * 0.36 : 0;
	for (int i = 0; i < shown; i++) {
		const OverlayRow &row = d.queue[i];
		double x = inner.left();
		double right = inner.right();
		if (s.showNumbers) {
			p.setFont(numFont);
			p.setPen(s.accentColor);
			p.drawText(QRectF(x, y, numW, g.queueRowH), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				   QString::number(i + 1));
			x += numW;
		}
		if (s.showRequester && !row.requester.isEmpty()) {
			const double byW = qMin(textWidth(byFont, row.requester) + 2, (right - x) * 0.36);
			p.setFont(byFont);
			p.setPen(s.mutedColor);
			p.drawText(QRectF(right - byW, y, byW, g.queueRowH), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
				   elide(byFont, row.requester, byW));
			right -= byW + g.f * 0.36;
		}
		const QString artist = s.showArtist && !row.artist.isEmpty() ? kDash + row.artist : QString();
		const double titleW = fm.horizontalAdvance(row.title);
		const QRectF r(x, y, right - x, g.queueRowH);
		p.setFont(rowFont);
		p.setPen(s.textColor);
		if (artist.isEmpty() || titleW >= r.width() - fm.averageCharWidth() * 3) {
			p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(rowFont, row.title, r.width()));
		} else {
			p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, row.title);
			QRectF rest = r;
			rest.setLeft(r.left() + titleW);
			p.setFont(rowFontLight);
			p.setPen(s.mutedColor);
			p.drawText(rest, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elide(rowFontLight, artist, rest.width()));
		}
		y += g.queueRowH;
	}
	if (extra > 0 && g.queueRows >= 2) {
		p.setFont(rowFontLight);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(inner.left() + numW, y, inner.width() - numW, g.queueRowH), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			   d.lblMore.arg(extra));
	}
}

// ------------------------------------------------------------- barra: tarjeta
// Tarjeta oscura redondeada con una linea de acento arriba. Sonando: portada con su disco,
// punto de "en vivo" y etiquetas (tiempo, artista, quien la pidio). Cola: icono, el
// siguiente pedido en grande y el resto pasando como una cinta. Lista: canciones de la
// pagina, barra de progreso de paginas y como pedir.

const double kCardLabel = 0.28, kCardTitle = 0.62, kCardSub = 0.34, kCardChip = 0.27, kCardRow = 0.40, kCardFoot = 0.26;

QColor withAlpha(QColor c, int alpha)
{
	c.setAlpha(alpha);
	return c;
}

// iconos pequenos dibujados a mano (no dependen de las fuentes instaladas)
void iconHeart(QPainter &p, const QRectF &r, const QColor &c)
{
	QPainterPath path;
	const double w = r.width(), h = r.height(), x = r.left(), y = r.top();
	path.moveTo(x + w * 0.5, y + h * 0.92);
	path.cubicTo(x - w * 0.15, y + h * 0.45, x + w * 0.15, y - h * 0.05, x + w * 0.5, y + h * 0.28);
	path.cubicTo(x + w * 0.85, y - h * 0.05, x + w * 1.15, y + h * 0.45, x + w * 0.5, y + h * 0.92);
	p.fillPath(path, c);
}

void iconNote(QPainter &p, const QRectF &r, const QColor &c)
{
	p.save();
	p.setPen(Qt::NoPen);
	p.setBrush(c);
	const double w = r.width(), h = r.height();
	p.drawEllipse(QPointF(r.left() + w * 0.32, r.top() + h * 0.78), w * 0.2, h * 0.15);
	p.fillRect(QRectF(r.left() + w * 0.46, r.top() + h * 0.12, w * 0.09, h * 0.66), c);
	QPainterPath flag;
	flag.moveTo(r.left() + w * 0.5, r.top() + h * 0.12);
	flag.cubicTo(r.left() + w * 0.75, r.top() + h * 0.2, r.left() + w * 0.9, r.top() + h * 0.3, r.left() + w * 0.8, r.top() + h * 0.5);
	flag.cubicTo(r.left() + w * 0.78, r.top() + h * 0.36, r.left() + w * 0.66, r.top() + h * 0.3, r.left() + w * 0.55, r.top() + h * 0.3);
	p.drawPath(flag);
	p.restore();
}

void iconLines(QPainter &p, const QRectF &r, const QColor &c)
{
	const double w = r.width(), h = r.height(), t = qMax(1.0, h * 0.11);
	for (int i = 0; i < 3; i++) {
		const double y = r.top() + h * (0.22 + i * 0.28);
		p.fillRect(QRectF(r.left() + w * 0.12, y - t / 2, t, t), c);
		p.fillRect(QRectF(r.left() + w * 0.32, y - t / 2, w * 0.56, t), c);
	}
}

void iconStar(QPainter &p, const QRectF &r, const QColor &c)
{
	QPolygonF star;
	for (int i = 0; i < 10; i++) {
		const double a = -M_PI / 2 + i * M_PI / 5, rr = (i % 2 ? 0.22 : 0.48) * r.width();
		star << QPointF(r.center().x() + rr * std::cos(a), r.center().y() + rr * std::sin(a));
	}
	p.save();
	p.setPen(QPen(c, qMax(1.0, r.width() * 0.08), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	p.setBrush(Qt::NoBrush);
	p.drawPolygon(star);
	p.restore();
}

// Etiqueta en una pildora: [icono texto]. Devuelve el ancho usado.
double chip(QPainter &p, const OverlayStyle &s, double x, double y, double h, double maxW, const QString &text,
	    void (*icon)(QPainter &, const QRectF &, const QColor &), const QColor &tint = QColor())
{
	if (maxW < h * 2)
		return 0;
	const QFont font = makeFont(s, h * 0.5, true);
	const double padX = h * 0.38, iconW = icon ? h * 0.48 : 0, gap = icon ? h * 0.22 : 0;
	const double w = qMin(maxW, padX * 2 + iconW + gap + textWidth(font, text) + 2);
	const QRectF r(x, y, w, h);
	p.save();
	p.setPen(QPen(tint.isValid() ? withAlpha(tint, 90) : withAlpha(s.textColor, 34), 1));
	p.setBrush(tint.isValid() ? withAlpha(tint, 34) : withAlpha(s.textColor, 14));
	p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), h / 2, h / 2);
	if (icon)
		icon(p, QRectF(x + padX, y + (h - iconW) / 2, iconW, iconW), tint.isValid() ? tint : s.mutedColor);
	drawLine(p, font, withAlpha(s.textColor, 215), QRectF(x + padX + iconW + gap, y, w - padX * 2 - iconW - gap, h), text);
	p.restore();
	return w;
}

// Cabecera de cada parte: punto o nada, texto en mayusculas y, a la derecha, un dato.
void cardLabel(QPainter &p, const OverlayStyle &s, const BarGeom &g, const QRectF &r, const QString &text, const QColor &color,
	       bool dot, const QColor &dotColor, const QString &right = QString())
{
	const QFont font = makeFont(s, g.f * kCardLabel, true, g.f * 0.07);
	double x = r.left();
	if (dot) {
		const double d = r.height() * 0.42;
		p.setPen(Qt::NoPen);
		p.setBrush(dotColor);
		p.drawEllipse(QPointF(x + d / 2, r.center().y()), d / 2, d / 2);
		x += d + g.f * 0.16;
	}
	double rightW = 0;
	if (!right.isEmpty()) {
		const QFont rf = makeFont(s, g.f * kCardLabel, false);
		rightW = qMin(textWidth(rf, right) + 2, r.width() * 0.55);
		p.setFont(rf);
		p.setPen(s.mutedColor);
		p.drawText(QRectF(r.right() - rightW, r.top(), rightW, r.height()), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
			   elide(rf, right, rightW));
		rightW += g.f * 0.3;
	}
	drawLine(p, font, color, QRectF(x, r.top(), r.right() - rightW - x, r.height()), text.toUpper());
}

// Los temas listos. El 0 usa los colores elegidos en la fuente.
CardTheme cardTheme(const OverlayStyle &s)
{
	CardTheme t;
	const auto rgb = [](int r, int g, int b, int a = 255) { return QColor(r, g, b, a); };
	switch (qBound(0, s.cardTheme, kCardThemes)) {
	case 1: // Medianoche: grafito, acento coral que pasa a ambar
		t.bgTop = rgb(27, 28, 35);
		t.bgBottom = rgb(16, 17, 22);
		t.text = Qt::white;
		t.muted = rgb(255, 255, 255, 150);
		t.accent = rgb(244, 114, 82);
		t.accent2 = rgb(251, 191, 36);
		t.dot = rgb(239, 68, 68);
		t.border = rgb(255, 255, 255, 24);
		break;
	case 2: // Nieve: blanca, acento violeta, sombra marcada
		t.bgTop = Qt::white;
		t.bgBottom = rgb(243, 244, 248);
		t.text = rgb(18, 20, 28);
		t.muted = rgb(18, 20, 28, 145);
		t.accent = rgb(139, 92, 246);
		t.accent2 = rgb(99, 102, 241);
		t.dot = rgb(244, 63, 94);
		t.border = rgb(0, 0, 0, 22);
		break;
	case 3: // Neon: negra, borde verde lima que brilla, esquinas rectas
		t.bgTop = rgb(11, 14, 11);
		t.bgBottom = rgb(5, 7, 5);
		t.text = Qt::white;
		t.muted = rgb(214, 255, 214, 140);
		t.accent = rgb(163, 230, 53);
		t.accent2 = rgb(34, 197, 94);
		t.dot = rgb(163, 230, 53);
		t.border = rgb(163, 230, 53, 150);
		t.radius = 0.2;
		t.round = 0.1;
		t.glow = 3;
		t.topLine = 1.5;
		t.chipTint = true;
		break;
	case 4: // Aurora: degradado de violeta a azul de lado a lado
		t.bgTop = rgb(36, 22, 70);
		t.bgBottom = rgb(13, 31, 66);
		t.sideways = true;
		t.text = Qt::white;
		t.muted = rgb(224, 228, 255, 165);
		t.accent = rgb(167, 139, 250);
		t.accent2 = rgb(96, 165, 250);
		t.dot = rgb(244, 114, 182);
		t.border = rgb(167, 139, 250, 70);
		t.radius = 0.38;
		break;
	case 5: // Cristal: azul noche translucido con reflejo, acento celeste
		t.bgTop = rgb(23, 37, 66, 190);
		t.bgBottom = rgb(12, 20, 40, 165);
		t.text = Qt::white;
		t.muted = rgb(226, 240, 255, 175);
		t.accent = rgb(56, 189, 248);
		t.accent2 = rgb(34, 211, 238);
		t.dot = rgb(56, 189, 248);
		t.border = rgb(255, 255, 255, 70);
		t.radius = 0.42;
		t.sheen = true;
		break;
	case 6: // Menta: verde profundo, esquinas de pildora, iconos redondos, etiquetas tenidas
		t.bgTop = rgb(17, 42, 36);
		t.bgBottom = rgb(9, 24, 21);
		t.text = rgb(236, 253, 245);
		t.muted = rgb(236, 253, 245, 150);
		t.accent = rgb(45, 212, 191);
		t.accent2 = rgb(52, 211, 153);
		t.dot = rgb(52, 211, 153);
		t.border = rgb(45, 212, 191, 60);
		t.radius = 0.75;
		t.round = 0.5;
		t.chipTint = true;
		break;
	default: { // con los colores de la fuente
		QColor bg = s.backgroundColor;
		bg.setAlphaF(qBound(0, s.backgroundOpacity, 100) / 100.0);
		t.bgTop = isLight(s.backgroundColor) ? bg : mix(bg, Qt::white, 0.06);
		t.bgTop.setAlpha(bg.alpha());
		t.bgBottom = isLight(s.backgroundColor) ? mix(bg, Qt::black, 0.04) : bg;
		t.bgBottom.setAlpha(bg.alpha());
		t.text = s.textColor;
		t.muted = s.mutedColor;
		t.accent = s.accentColor;
		t.accent2 = mix(s.accentColor, Qt::white, 0.25);
		t.dot = QColor(239, 68, 68);
		t.border = withAlpha(s.textColor, 26);
		break;
	}
	}
	return t;
}

void drawCardBackground(QPainter &p, const OverlayStyle &s, const BarGeom &g)
{
	Q_UNUSED(s);
	const CardTheme &t = g.theme;
	const QRectF r = g.panel;
	const double radius = qMin(g.f * t.radius, r.height() / 2);
	QPainterPath path;
	path.addRoundedRect(r, radius, radius);
	// sombra suave alrededor (mas marcada en los temas claros)
	const int shadowAlpha = isLight(t.bgTop) ? 24 : 16;
	for (int i = 4; i >= 1; i--) {
		QPainterPath shadow;
		const double e = g.f * 0.04 * i;
		shadow.addRoundedRect(r.adjusted(-e, -e * 0.5, e, e * 1.4), radius + e, radius + e);
		p.fillPath(shadow, QColor(0, 0, 0, shadowAlpha));
	}
	// brillo del borde (neon)
	for (int i = t.glow; i >= 1; i--) {
		QPainterPath halo;
		const double e = g.f * 0.05 * i;
		halo.addRoundedRect(r.adjusted(-e, -e, e, e), radius + e, radius + e);
		p.fillPath(halo.subtracted(path), withAlpha(t.accent, 18));
	}
	QLinearGradient grad = t.sideways ? QLinearGradient(r.topLeft(), r.topRight()) : QLinearGradient(r.topLeft(), r.bottomLeft());
	grad.setColorAt(0, t.bgTop);
	grad.setColorAt(1, t.bgBottom);
	if (t.bgTop.alpha() > 0 || t.bgBottom.alpha() > 0)
		p.fillPath(path, grad);
	p.save();
	p.setClipPath(path);
	if (t.sheen) { // reflejo de cristal en la mitad de arriba
		QLinearGradient sheen(r.topLeft(), QPointF(r.left(), r.center().y()));
		sheen.setColorAt(0, QColor(255, 255, 255, 34));
		sheen.setColorAt(1, QColor(255, 255, 255, 0));
		p.fillRect(QRectF(r.left(), r.top(), r.width(), r.height() / 2), sheen);
	}
	// linea de acento que brilla en el borde de arriba
	QLinearGradient line(r.topLeft(), r.topRight());
	line.setColorAt(0, withAlpha(t.accent, 0));
	line.setColorAt(0.3, withAlpha(t.accent, 230));
	line.setColorAt(0.7, withAlpha(t.accent2, 170));
	line.setColorAt(1, withAlpha(t.accent2, 0));
	p.fillRect(QRectF(r.left(), r.top(), r.width(), qMax(1.5, g.f * 0.045 * t.topLine)), line);
	QLinearGradient glow(r.topLeft(), QPointF(r.left(), r.top() + g.f * 0.6));
	glow.setColorAt(0, withAlpha(t.accent, int(26 * t.topLine)));
	glow.setColorAt(1, withAlpha(t.accent, 0));
	p.fillRect(QRectF(r.left(), r.top(), r.width(), g.f * 0.6), glow);
	p.restore();
	p.save();
	p.setPen(QPen(t.border, t.glow ? 1.5 : 1));
	p.setBrush(Qt::NoBrush);
	p.drawPath(path);
	p.restore();
	// separadores entre las partes encendidas
	bool first = true;
	for (int idx : kBarOrder) {
		if (g.rect[idx].isEmpty())
			continue;
		if (!first)
			p.fillRect(QRectF(g.rect[idx].left(), r.top() + g.pad * 1.1, 1, r.height() - g.pad * 2.2), withAlpha(t.text, 28));
		first = false;
	}
}

// Sonando: portada con su disco asomando, punto, titulo y etiquetas.
void drawCardNow(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g, double t, BarLayer layer,
		 double textOpacity)
{
	const QRectF inner = g.rect[OverlayStyle::BarNow].adjusted(g.pad, g.pad, -g.pad * 0.6, -g.pad);
	const double tile = qMin(inner.height() * 0.84, inner.width() * 0.28);
	const QRectF art(inner.left(), inner.center().y() - tile / 2, tile, tile);
	const double discR = tile * 0.44;
	const QPointF discC(art.right() - tile * 0.02, art.center().y());
	const bool any = d.hasPlaying || d.lastSong;
	if (layer == BarMoving) {
		if (any) { // lo impreso del disco gira por detras de la portada
			QPainterPath outside;
			outside.addRect(g.rect[OverlayStyle::BarNow]);
			QPainterPath artPath;
			artPath.addRoundedRect(art, tile * g.theme.round * 0.5, tile * g.theme.round * 0.5);
			p.save();
			p.setClipPath(outside.subtracted(artPath));
			drawDisc(p, discC, discR, coverLight(d.title, d.artist), s.animate ? t * 200.0 : 0.0, DiscPrint);
			p.restore();
		}
		return;
	}
	if (any) {
		drawDisc(p, discC, discR, coverLight(d.title, d.artist), s.animate ? t * 200.0 : 0.0, layer == BarAll ? DiscAll : DiscStill);
		QPainterPath artPath;
		artPath.addRoundedRect(art, tile * g.theme.round * 0.5, tile * g.theme.round * 0.5);
		p.save();
		p.setClipPath(artPath);
		p.drawImage(art, coverArt(d.title, d.artist, int(tile + 1)));
		p.restore();
		p.setPen(QPen(withAlpha(s.textColor, 40), 1));
		p.setBrush(Qt::NoBrush);
		p.drawPath(artPath);
	} else {
		QPainterPath empty;
		empty.addRoundedRect(art, tile * 0.12, tile * 0.12);
		p.fillPath(empty, withAlpha(s.textColor, 14));
		iconNote(p, art.adjusted(tile * 0.28, tile * 0.28, -tile * 0.28, -tile * 0.28), s.mutedColor);
	}

	const double x = (any ? discC.x() + discR : art.right()) + g.f * 0.45;
	const double w = qMax(8.0, inner.right() - x);
	const QFont titleFont = makeFont(s, g.f * kCardTitle, true);
	const QFontMetricsF tfm(titleFont);
	const double labelH = QFontMetricsF(makeFont(s, g.f * kCardLabel, true)).height();
	const double chipH = g.f * kCardChip * 2.0;
	const double gap = g.f * 0.14;
	const double blockH = labelH + gap + tfm.height() + gap + chipH;
	double y = inner.center().y() - blockH / 2;
	p.save();
	p.setOpacity(p.opacity() * textOpacity);
	const QString label = d.lastSong ? d.lblLastSong : d.lblNowPlaying;
	cardLabel(p, s, g, QRectF(x, y, w, labelH), label, s.mutedColor, true, d.hasPlaying ? g.theme.dot : withAlpha(s.textColor, 90));
	y += labelH + gap;
	drawLine(p, titleFont, any ? s.textColor : s.mutedColor, QRectF(x, y, w, tfm.height()), any ? d.title : d.lblNothing);
	y += tfm.height() + gap;
	double cx = x;
	const double right = x + w;
	const QColor tint = g.theme.chipTint ? g.theme.accent : QColor();
	if (any && s.showArtist && !d.artist.isEmpty())
		cx += chip(p, s, cx, y, chipH, right - cx, d.artist, iconNote, tint) + g.f * 0.14;
	if (any && s.showRequester && !d.requesters.isEmpty())
		chip(p, s, cx, y, chipH, right - cx, d.requesters.join(QStringLiteral(", ")), iconHeart, tint);
	p.restore();
}

// Cola: icono, el siguiente pedido en grande y el resto en una cinta que pasa.
QRectF cardTickerRect(const BarGeom &g)
{
	const QRectF inner = g.rect[OverlayStyle::BarQueue].adjusted(g.pad, g.pad * 0.8, -g.pad, -g.pad * 0.7);
	const double h = g.f * 0.29 * 1.5;
	return QRectF(inner.left(), inner.bottom() - h, inner.width(), h);
}

QString cardTickerText(const OverlayData &d)
{
	QStringList parts;
	for (int i = 1; i < d.queue.size(); i++) {
		const OverlayRow &r = d.queue[i];
		parts << QStringLiteral("%1  %2").arg(i + 1).arg(r.artist.isEmpty() ? r.title : r.title + kDash + r.artist);
	}
	if (d.queueTotal > d.queue.size() && !d.queue.isEmpty())
		parts << d.lblMore.arg(d.queueTotal - int(d.queue.size()));
	if (parts.isEmpty())
		return d.lblTicker;
	return parts.join(QStringLiteral("   •   ")) + QStringLiteral("   •   ") + d.lblTicker;
}

// la cinta: el texto se repite sin fin, siempre a la misma velocidad
void drawCardTicker(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g, const QRectF &ticker)
{
	const QFont font = makeFont(s, g.f * 0.29, false);
	const QString text = cardTickerText(d) + QStringLiteral("   •   ");
	const double w = textWidth(font, text);
	if (w <= 1)
		return;
	const double offset = s.animate ? std::fmod(d.clock * g.f * 1.4, w) : 0.0;
	p.save();
	p.setClipRect(ticker);
	p.setFont(font);
	p.setPen(s.mutedColor);
	for (double x = ticker.left() - offset; x < ticker.right(); x += w)
		p.drawText(QRectF(x, ticker.top(), w + 2, ticker.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
	p.restore();
}

void drawCardQueue(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g, BarLayer layer)
{
	const QRectF ticker = cardTickerRect(g);
	if (layer == BarMoving) {
		drawCardTicker(p, s, d, g, ticker);
		return;
	}
	const QRectF inner = g.rect[OverlayStyle::BarQueue].adjusted(g.pad, g.pad * 0.8, -g.pad, -g.pad * 0.7);
	// marca de agua y rayitas de acento, como en una pantalla de directo
	p.save();
	const double wm = inner.height() * 0.95;
	iconNote(p, QRectF(inner.right() - wm * 0.95, inner.top() - wm * 0.08, wm, wm), withAlpha(s.textColor, 12));
	const double dashW = g.f * 0.28, dashH = qMax(1.5, g.f * 0.05);
	for (int i = 0; i < 3; i++)
		p.fillRect(QRectF(inner.right() - dashW * (3.6 - i * 1.25), g.panel.top() + g.f * 0.16, dashW, dashH),
			   i == 0 ? s.accentColor : withAlpha(s.textColor, 40));
	p.restore();

	const double area = inner.height() - ticker.height() - g.f * 0.12;
	const double tile = qMin(area * 0.78, g.f * 1.25);
	const QRectF icon(inner.left(), inner.top() + (area - tile) / 2, tile, tile);
	QPainterPath tilePath;
	tilePath.addRoundedRect(icon, tile * g.theme.round, tile * g.theme.round);
	QLinearGradient tileGrad(icon.topLeft(), icon.bottomRight());
	tileGrad.setColorAt(0, g.theme.accent);
	tileGrad.setColorAt(1, g.theme.accent2);
	p.fillPath(tilePath, tileGrad);
	iconLines(p, icon.adjusted(tile * 0.24, tile * 0.24, -tile * 0.24, -tile * 0.24), QColor(255, 255, 255, 235));

	const double x = icon.right() + g.f * 0.4;
	const double w = qMax(8.0, inner.right() - x);
	const QFont titleFont = makeFont(s, g.f * kCardTitle, true);
	const QFont subFont = makeFont(s, g.f * kCardSub, false);
	const QFontMetricsF tfm(titleFont), sfm(subFont);
	const double labelH = QFontMetricsF(makeFont(s, g.f * kCardLabel, true)).height();
	const double blockH = labelH + tfm.height() + sfm.height();
	double y = inner.top() + (area - blockH) / 2;
	QString label = d.lblQueue;
	if (d.queueTotal > 0)
		label += QStringLiteral("  ·  %1").arg(d.queueTotal);
	if (!d.requestsOpen)
		label += QStringLiteral("  ·  ") + d.lblClosed;
	cardLabel(p, s, g, QRectF(x, y, w, labelH), label, s.accentColor, false, QColor());
	y += labelH;
	if (d.queue.isEmpty()) {
		drawLine(p, titleFont, s.textColor, QRectF(x, y, w, tfm.height()), d.requestsOpen ? d.lblEmptyQueue : d.lblClosed);
		y += tfm.height();
		drawLine(p, subFont, s.mutedColor, QRectF(x, y, w, sfm.height()), d.lblHowTo);
	} else {
		const OverlayRow &next = d.queue.first();
		drawLine(p, titleFont, s.textColor, QRectF(x, y, w, tfm.height()), next.title);
		y += tfm.height();
		QString sub = s.showArtist ? next.artist : QString();
		if (s.showRequester && !next.requester.isEmpty())
			sub = sub.isEmpty() ? d.lblRequestedBy.arg(next.requester) : sub + kDot + d.lblRequestedBy.arg(next.requester);
		drawLine(p, subFont, s.mutedColor, QRectF(x, y, w, sfm.height()), sub);
	}
	p.fillRect(QRectF(inner.left(), ticker.top() - g.f * 0.08, inner.width(), 1), withAlpha(s.textColor, 18));
	if (layer == BarAll)
		drawCardTicker(p, s, d, g, ticker);
}

// Lista: icono, canciones de la pagina, barra de progreso de paginas y como pedir.
void drawCardList(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g)
{
	const QRectF inner = g.rect[OverlayStyle::BarList].adjusted(g.pad, g.pad * 0.8, -g.pad, -g.pad * 0.7);
	const double labelH = QFontMetricsF(makeFont(s, g.f * kCardLabel, true)).height();
	const double tile = labelH * 1.9;
	const QRectF icon(inner.left(), inner.top(), tile, tile);
	QPainterPath tilePath;
	tilePath.addRoundedRect(icon, tile * g.theme.round, tile * g.theme.round);
	p.fillPath(tilePath, withAlpha(s.accentColor, 40));
	p.save();
	p.setPen(QPen(withAlpha(s.accentColor, 150), 1));
	p.setBrush(Qt::NoBrush);
	p.drawPath(tilePath);
	p.restore();
	iconStar(p, icon.adjusted(tile * 0.18, tile * 0.18, -tile * 0.18, -tile * 0.18), s.accentColor);

	const double hx = icon.right() + g.f * 0.25;
	QString label = d.lblSongList;
	if (d.songsTotal > 0)
		label += QStringLiteral("  ·  %1").arg(d.songsTotal);
	const QString pageText = d.pages > 1 ? QStringLiteral("%1 / %2").arg(d.page + 1).arg(d.pages) : QString();
	cardLabel(p, s, g, QRectF(hx, icon.center().y() - labelH / 2, inner.right() - hx, labelH), label, s.mutedColor, false, QColor(),
		  pageText);

	// canciones
	const QFont rowFont = makeFont(s, g.f * kCardRow, false);
	const QFont numFont = makeFont(s, g.f * kCardRow, true);
	const double top = icon.bottom() + g.f * 0.14;
	if (d.songs.isEmpty()) {
		drawLine(p, rowFont, s.mutedColor, QRectF(inner.left(), top, inner.width(), g.listRowH), d.lblEmptyList);
	} else {
		int maxNumber = 9;
		for (const OverlayRow &row : d.songs)
			maxNumber = qMax(maxNumber, row.number);
		const double numGap = g.f * 0.22;
		const double numW = s.showNumbers ? textWidth(numFont, QString::number(maxNumber)) + numGap : 0;
		const double colGap = g.f * 0.5;
		const double colW = (inner.width() - colGap * (g.listCols - 1)) / g.listCols;
		for (int i = 0; i < d.songs.size() && i < g.listRows * g.listCols; i++) {
			const OverlayRow &row = d.songs[i];
			const int c = i / g.listRows, r = i % g.listRows;
			const double left = inner.left() + c * (colW + colGap);
			const double y = top + r * g.listRowH;
			double x = left;
			if (s.showNumbers) {
				p.setFont(numFont);
				p.setPen(s.accentColor);
				p.drawText(QRectF(x, y, numW - numGap, g.listRowH), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
					   QString::number(row.number));
				x += numW;
			}
			drawTwoTone(p, s, rowFont, QRectF(x, y, left + colW - x, g.listRowH), row.title,
				    s.showArtist && !row.artist.isEmpty() ? kDash + row.artist : QString());
		}
	}

	// progreso de paginas y pie
	const QFont footFont = makeFont(s, g.f * kCardFoot * 1.1, true);
	const double footH = QFontMetricsF(footFont).height();
	const double barH = qMax(2.0, g.f * 0.1);
	const double barY = inner.bottom() - footH - g.f * 0.14 - barH;
	const QRectF track(inner.left(), barY, inner.width(), barH);
	p.save();
	p.setPen(Qt::NoPen);
	p.setBrush(withAlpha(s.textColor, 22));
	p.drawRoundedRect(track, barH / 2, barH / 2);
	const double progress = d.pages > 0 ? double(d.page + 1) / d.pages : 1.0;
	QLinearGradient fill(track.topLeft(), track.topRight());
	fill.setColorAt(0, g.theme.accent);
	fill.setColorAt(1, g.theme.accent2);
	p.setBrush(fill);
	p.drawRoundedRect(QRectF(track.left(), track.top(), qMax(barH, track.width() * progress), barH), barH / 2, barH / 2);
	// rayitas: una por pagina (hasta 5), la actual encendida
	const int dots = qMin(qMax(1, d.pages), 5);
	const double dw = g.f * 0.26;
	for (int i = 0; i < dots; i++) {
		const bool on = d.pages <= 5 ? i == d.page : i == qMin(4, d.page * 5 / qMax(1, d.pages));
		p.fillRect(QRectF(inner.left() + i * dw * 1.35, inner.bottom() - footH / 2 - barH / 2, dw, barH),
			   on ? s.accentColor : withAlpha(s.textColor, 40));
	}
	p.restore();
	const QString foot = (s.showWeb && !d.webUrl.isEmpty()) ? d.webUrl : d.lblHowTo;
	p.setFont(footFont);
	p.setPen(s.mutedColor);
	const double fx = inner.left() + dots * dw * 1.35 + g.f * 0.3;
	p.drawText(QRectF(fx, inner.bottom() - footH, inner.right() - fx, footH), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
		   elide(footFont, foot, inner.right() - fx));
}

void drawBar(QPainter &p, const OverlayStyle &style, const OverlayData &d, double opacity, double seconds, BarLayer layer = BarAll)
{
	BarGeom g = barGeom(style, d.sectionOn);
	if (g.rect[0].isEmpty() && g.rect[1].isEmpty() && g.rect[2].isEmpty())
		return; // todo apagado: no se dibuja ni la franja
	OverlayStyle s = style; // las cabeceras y demas piezas comunes se miden con la letra de la barra
	s.fontSize = int(g.f + 0.5);
	if (s.barStyle == OverlayStyle::BarCard) {
		g.theme = cardTheme(style);
		s.textColor = g.theme.text; // las piezas comunes toman los colores del tema
		s.mutedColor = g.theme.muted;
		s.accentColor = g.theme.accent;
		s.backgroundColor = g.theme.bgTop;
		const double base = qBound(0.0, opacity, 1.0);
		if (layer != BarMoving) {
			p.setOpacity(base);
			drawCardBackground(p, s, g);
		}
		const auto show = [&](int section) {
			if (g.rect[section].isEmpty())
				return false;
			p.setOpacity(base * qBound(0.0, d.section[section], 1.0));
			return p.opacity() > 0.001;
		};
		if (!g.rect[OverlayStyle::BarNow].isEmpty()) {
			p.setOpacity(base);
			drawCardNow(p, s, d, g, seconds, layer, qBound(0.0, d.section[OverlayStyle::BarNow], 1.0));
		}
		if (show(OverlayStyle::BarQueue))
			drawCardQueue(p, s, d, g, layer);
		if (layer != BarMoving && show(OverlayStyle::BarList))
			drawCardList(p, s, d, g);
		return;
	}
	if (layer != BarMoving) {
		p.setOpacity(qBound(0.0, opacity, 1.0));
		drawBarBackground(p, s, g);
	}
	const auto visible = [&](int section) {
		if (g.rect[section].isEmpty())
			return false;
		const double o = qBound(0.0, opacity, 1.0) * qBound(0.0, d.section[section], 1.0);
		p.setOpacity(o);
		return o > 0.001;
	};
	if (!g.rect[OverlayStyle::BarNow].isEmpty()) {
		p.setOpacity(qBound(0.0, opacity, 1.0)); // el disco no se desvanece al cambiar de cancion
		drawBarNow(p, s, d, g, seconds, layer, qBound(0.0, d.section[OverlayStyle::BarNow], 1.0));
	}
	if (layer != BarMoving && visible(OverlayStyle::BarList))
		drawBarList(p, s, d, g);
	if (layer != BarMoving && visible(OverlayStyle::BarQueue))
		drawBarQueue(p, s, d, g);
}

// Todo lo que decide como se ve la capa quieta de la barra (el estilo lo vigila quien llama).
QString barKey(const OverlayStyle &s, const OverlayData &d, double opacity)
{
	const QChar sep(0x1f);
	QString k = QString::number(s.width) + sep + QString::number(s.height) + sep + QString::number(int(opacity * 1000)) + sep;
	for (int i = 0; i < 3; i++)
		k += QString::number(int(d.section[i] * 1000)) + (d.sectionOn[i] ? QLatin1Char('+') : QLatin1Char('-')) + sep;
	k += QString::number(d.hasPlaying) + QString::number(d.lastSong) + sep + d.title + sep + d.artist + sep + d.requesters.join(QLatin1Char(',')) + sep +
	     QString::number(d.queueTotal) + sep + QString::number(d.songsTotal) + sep + QString::number(d.page) + sep +
	     QString::number(d.pages) + sep + QString::number(d.requestsOpen) + sep + d.lblNowPlaying + sep + d.lblQueue + sep +
	     d.lblSongList + sep + d.lblRequestedBy + sep + d.lblHowTo + sep + d.lblClosed + sep + d.lblEmptyQueue + sep +
	     d.lblEmptyList + sep + d.lblWeb + sep + d.lblSongs + sep + d.lblLastSong + sep + d.lblMore + sep + d.lblTicker + sep + d.webUrl + QChar(0x1e);
	for (const OverlayRow &r : d.queue)
		k += QString::number(r.number) + sep + r.title + sep + r.artist + sep + r.requester + QChar(0x1e);
	k += QChar(0x1d);
	for (const OverlayRow &r : d.songs)
		k += QString::number(r.number) + sep + r.title + sep + r.artist + QChar(0x1e);
	return k;
}

} // namespace

QImage coverArt(const QString &title, const QString &artist, int size)
{
	// se dibujan pocas veces (al cambiar de cancion): se guardan las ultimas
	static QHash<QString, QImage> cache;
	size = qBound(8, size, 1024);
	const quint32 h = songHash(title, artist);
	const QString key = QString::number(h) + QLatin1Char('/') + QString::number(size);
	auto it = cache.constFind(key);
	if (it != cache.constEnd())
		return *it;
	if (cache.size() > 40)
		cache.clear();
	const QImage img = renderCover(h, size);
	cache.insert(key, img);
	return img;
}

QColor coverDark(const QString &title, const QString &artist)
{
	return QColor::fromHsv(coverHue(songHash(title, artist)), 170, 70);
}

QColor coverLight(const QString &title, const QString &artist)
{
	return QColor::fromHsv((coverHue(songHash(title, artist)) + 20) % 360, 120, 255);
}

BarLayout barLayout(const OverlayStyle &style)
{
	const bool all[3] = {true, true, true};
	const BarGeom g = barGeom(clamped(style), all);
	BarLayout b;
	b.listRows = g.listRows;
	b.listColumns = g.listCols;
	b.queueRows = g.queueRows;
	return b;
}

QImage renderOverlay(const OverlayStyle &style, const OverlayData &data, double opacity, double seconds, OverlayCache *cache)
{
	const OverlayStyle s = clamped(style);

	// Barra con cache: los paneles y el texto se dibujan una vez; en cada fotograma solo
	// se copia esa capa y se pinta encima lo que gira.
	if (cache && s.mode == OverlayStyle::Bar && opacity > 0.001) {
		const QString key = barKey(s, data, opacity);
		if (cache->key != key || cache->base.size() != QSize(s.width, s.height)) {
			QImage base(s.width, s.height, QImage::Format_RGBA8888_Premultiplied);
			base.fill(Qt::transparent);
			QPainter bp(&base);
			bp.setRenderHint(QPainter::Antialiasing, true);
			bp.setRenderHint(QPainter::TextAntialiasing, true);
			drawBar(bp, s, data, opacity, seconds, BarStill);
			bp.end();
			cache->base = base;
			cache->key = key;
		}
		QImage out = cache->base; // comparte los pixeles hasta que se pinta encima
		QPainter op(&out);
		op.setRenderHint(QPainter::Antialiasing, true);
		drawBar(op, s, data, opacity, seconds, BarMoving);
		op.end();
		return out;
	}

	QImage img(s.width, s.height, QImage::Format_RGBA8888_Premultiplied);
	img.fill(Qt::transparent);
	if (opacity <= 0.001)
		return img;

	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setRenderHint(QPainter::TextAntialiasing, true);
	p.setOpacity(qBound(0.0, opacity, 1.0));
	switch (s.mode) {
	case OverlayStyle::NowPlaying:
		switch (s.skin) {
		case OverlayStyle::Minimal: drawNowPlaying(p, s, data); break;
		case OverlayStyle::Cassette: drawCassette(p, s, data, s.animate ? seconds : 0.0); break;
		case OverlayStyle::Led: drawLed(p, s, data, seconds); break;
		case OverlayStyle::Vinyl: drawVinyl(p, s, data, seconds); break;
		case OverlayStyle::Turntable: drawTurntable(p, s, data, seconds); break;
		}
		break;
	case OverlayStyle::Queue: drawQueue(p, skinned(s), data); break;
	case OverlayStyle::SongList: drawSongList(p, skinned(s), data); break;
	case OverlayStyle::Bar: drawBar(p, s, data, opacity, seconds); break;
	}
	p.end();
	return img;
}

} // namespace acsl

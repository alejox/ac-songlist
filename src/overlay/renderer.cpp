#include "renderer.hpp"

#include <QFontDatabase>
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
	n.idle = !d.hasPlaying;
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
		if (s.showLabel)
			n.label = (s.label.trimmed().isEmpty() ? d.lblNowPlaying : s.label.trimmed()).toUpper();
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
// Una sola fuente, pensada para el pie de la pantalla: tres paneles de cristal con la
// lista de canciones (izquierda), lo que suena (centro) y la cola (derecha). Cada panel
// se enciende y se apaga desde el panel de OBS.

const double kBarListScale = 0.44;  // letra de la lista respecto al titulo
const double kBarQueueScale = 0.5;  // letra de la cola

OverlayStyle clamped(const OverlayStyle &style)
{
	OverlayStyle s = style;
	s.width = qBound(16, s.width, 4096);
	s.height = qBound(16, s.height, 4096);
	s.fontSize = qBound(10, s.fontSize, 400);
	return s;
}

struct BarGeom {
	double f = 0;   // tamano del titulo; el resto de la letra sale de aqui
	double pad = 0; // margen interior de cada panel
	double headerH = 0, listRowH = 0, queueRowH = 0;
	QRectF left, center, right;
	int listRows = 1, listCols = 1, queueRows = 1;
};

BarGeom barGeom(const OverlayStyle &s)
{
	BarGeom g;
	g.f = qMax(10.0, qMin(double(s.fontSize), s.height * 0.22));
	const double m = qMax(3.0, g.f * 0.16); // aire alrededor para la sombra
	const double gap = g.f * 0.36;
	const double h = qMax(8.0, s.height - m * 2);
	const double total = qMax(24.0, s.width - m * 2 - gap * 2);
	const double centerW = total * qBound(20, s.barCenterPercent, 60) / 100.0;
	const double sideW = (total - centerW) / 2;
	g.left = QRectF(m, m, sideW, h);
	g.center = QRectF(m + sideW + gap, m, centerW, h);
	g.right = QRectF(m + sideW + gap + centerW + gap, m, sideW, h);
	g.pad = qMin(double(qMax(s.padding, 6)), h * 0.16);

	const QFont labelFont = makeFont(s, g.f * 0.36, true, g.f * 0.07);
	g.headerH = s.showLabel ? QFontMetricsF(labelFont).height() + g.f * 0.42 : 0;
	g.listRowH = QFontMetricsF(makeFont(s, g.f * kBarListScale, false)).height() * 1.2;
	g.queueRowH = QFontMetricsF(makeFont(s, g.f * kBarQueueScale, s.bold)).height() * 1.22;
	const double innerH = h - g.pad * 2 - g.headerH;
	g.listRows = qMax(1, int((innerH + 1.0) / g.listRowH));
	g.queueRows = qMax(1, int((innerH + 1.0) / g.queueRowH));
	// las columnas pedidas, salvo que quedaran demasiado estrechas para leerse
	g.listCols = qBound(1, s.columns, qMax(1, int((sideW - g.pad * 2) / (g.f * 5.5))));
	return g;
}

// Panel translucido con brillo arriba y un borde de luz, como un cristal esmerilado.
// (Una fuente de OBS no ve lo que tiene detras, asi que no puede desenfocarlo.)
void drawGlass(QPainter &p, const OverlayStyle &s, const QRectF &rect, double f)
{
	if (s.backgroundOpacity <= 0)
		return;
	const double r = qMin(double(s.radius), qMin(rect.width(), rect.height()) / 2.0);
	QPainterPath path;
	path.addRoundedRect(rect, r, r);

	// sombra suave, solo por fuera para no oscurecer el cristal
	p.save();
	QPainterPath outside;
	outside.addRect(rect.adjusted(-f, -f, f, f));
	p.setClipPath(outside.subtracted(path));
	const double spread = qMax(2.0, f * 0.14);
	for (int i = 4; i >= 1; i--) {
		const double e = spread * i / 4.0;
		QPainterPath shadow;
		shadow.addRoundedRect(rect.adjusted(-e, -e * 0.5, e, e), r + e, r + e);
		p.fillPath(shadow, QColor(0, 0, 0, 14));
	}
	p.restore();

	QColor tint = s.backgroundColor;
	tint.setAlphaF(qBound(0, s.backgroundOpacity, 100) / 100.0 * 0.62);
	p.fillPath(path, tint);

	QLinearGradient shine(rect.topLeft(), rect.bottomLeft());
	shine.setColorAt(0.0, QColor(255, 255, 255, 44));
	shine.setColorAt(0.38, QColor(255, 255, 255, 12));
	shine.setColorAt(1.0, QColor(255, 255, 255, 2));
	p.fillPath(path, shine);

	QLinearGradient edge(rect.topLeft(), rect.bottomLeft());
	edge.setColorAt(0.0, QColor(255, 255, 255, 150));
	edge.setColorAt(0.3, QColor(255, 255, 255, 56));
	edge.setColorAt(1.0, QColor(255, 255, 255, 30));
	const double w = qMax(1.0, f * 0.035);
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(QBrush(edge), w));
	p.drawRoundedRect(rect.adjusted(w / 2, w / 2, -w / 2, -w / 2), qMax(0.0, r - w / 2), qMax(0.0, r - w / 2));
	p.setPen(Qt::NoPen);
}

void drawBarList(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g)
{
	drawGlass(p, s, g.left, g.f);
	const QRectF inner = g.left.adjusted(g.pad, g.pad, -g.pad, -g.pad);
	double top = inner.top();
	if (s.showLabel) {
		QString label = d.lblSongList;
		if (d.songsTotal > 0)
			label += QStringLiteral("  ·  %1").arg(d.songsTotal);
		const QString right = d.pages > 1 ? QStringLiteral("%1 / %2").arg(d.page + 1).arg(d.pages) : QString();
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

void drawBarNow(QPainter &p, const OverlayStyle &s, const OverlayData &d, const BarGeom &g, double t, BarLayer layer)
{
	const NowText n = nowText(s, d);
	if (!n.visible)
		return;
	const QRectF inner = g.center.adjusted(g.pad, g.pad, -g.pad, -g.pad);
	// el disco gira solo mientras suena algo
	const double discR = qMin(inner.height() / 2, inner.width() * 0.2);
	const QPointF c(inner.left() + discR, inner.center().y());
	const double angle = (s.animate && !n.idle) ? t * 200.0 : 0.0;
	if (layer == BarMoving) {
		drawDisc(p, c, discR, s.accentColor, angle, DiscPrint);
		return;
	}
	drawGlass(p, s, g.center, g.f);
	drawDisc(p, c, discR, s.accentColor, angle, layer == BarAll ? DiscAll : DiscStill);
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(QColor(255, 255, 255, 46), qMax(1.0, g.f * 0.03))); // canto de luz: separa el disco del cristal
	p.drawEllipse(c, discR, discR);
	p.setPen(Qt::NoPen);

	const double x = inner.left() + discR * 2 + g.f * 0.5;
	const double w = qMax(8.0, inner.right() - x);
	// un titulo largo encoge un poco antes de cortarse
	double titlePx = g.f;
	const double natural = textWidth(makeFont(s, titlePx, s.bold), n.title);
	if (natural > w)
		titlePx = qMax(g.f * 0.7, titlePx * w / natural);
	const QFont titleFont = makeFont(s, titlePx, s.bold);
	const QFont labelFont = makeFont(s, g.f * 0.34, true, g.f * 0.07);
	const QFont subFont = makeFont(s, g.f * 0.52, false);
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
	drawGlass(p, s, g.right, g.f);
	const QRectF inner = g.right.adjusted(g.pad, g.pad, -g.pad, -g.pad);
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
	const QFont byFont = makeFont(s, g.f * 0.38, false);
	const int shown = qMin(int(d.queue.size()), g.queueRows);
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
}

void drawBar(QPainter &p, const OverlayStyle &style, const OverlayData &d, double opacity, double seconds, BarLayer layer = BarAll)
{
	const BarGeom g = barGeom(style);
	OverlayStyle s = style; // las cabeceras y demas piezas comunes se miden con la letra de la barra
	s.fontSize = int(g.f + 0.5);
	const auto visible = [&](int section) {
		const double o = qBound(0.0, opacity, 1.0) * qBound(0.0, d.section[section], 1.0);
		p.setOpacity(o);
		return o > 0.001;
	};
	if (layer != BarMoving && visible(OverlayStyle::BarList))
		drawBarList(p, s, d, g);
	if (visible(OverlayStyle::BarNow))
		drawBarNow(p, s, d, g, seconds, layer);
	if (layer != BarMoving && visible(OverlayStyle::BarQueue))
		drawBarQueue(p, s, d, g);
}

// Todo lo que decide como se ve la capa quieta de la barra (el estilo lo vigila quien llama).
QString barKey(const OverlayStyle &s, const OverlayData &d, double opacity)
{
	const QChar sep(0x1f);
	QString k = QString::number(s.width) + sep + QString::number(s.height) + sep + QString::number(int(opacity * 1000)) + sep;
	for (double o : d.section)
		k += QString::number(int(o * 1000)) + sep;
	k += QString::number(d.hasPlaying) + sep + d.title + sep + d.artist + sep + d.requesters.join(QLatin1Char(',')) + sep +
	     QString::number(d.queueTotal) + sep + QString::number(d.songsTotal) + sep + QString::number(d.page) + sep +
	     QString::number(d.pages) + sep + QString::number(d.requestsOpen) + sep + d.lblNowPlaying + sep + d.lblQueue + sep +
	     d.lblSongList + sep + d.lblRequestedBy + sep + d.lblHowTo + sep + d.lblClosed + sep + d.lblEmptyQueue + sep +
	     d.lblEmptyList + QChar(0x1e);
	for (const OverlayRow &r : d.queue)
		k += QString::number(r.number) + sep + r.title + sep + r.artist + sep + r.requester + QChar(0x1e);
	k += QChar(0x1d);
	for (const OverlayRow &r : d.songs)
		k += QString::number(r.number) + sep + r.title + sep + r.artist + QChar(0x1e);
	return k;
}

} // namespace

BarLayout barLayout(const OverlayStyle &style)
{
	const BarGeom g = barGeom(clamped(style));
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

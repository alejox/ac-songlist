#include "overlay-source.hpp"
#include "../plugin.hpp"
#include "../plugin-support.hpp"

#include <obs-module.h>

namespace acsl {

namespace {

struct Defaults {
	int width, height, fontSize;
};
// tamano inicial comodo para cada modo
const Defaults kDefaults[] = {{900, 180, 40}, {640, 460, 36}, {1500, 560, 36}, {1920, 200, 40}};
const int kModes = 4;

struct OverlaySource {
	obs_source_t *source = nullptr;
	std::shared_ptr<SourceShared> shared = std::make_shared<SourceShared>();
	gs_texture_t *texture = nullptr;
	uint32_t texW = 0, texH = 0;
	std::atomic<uint32_t> width{900}, height{180};
};

QColor colorFromObs(long long value, int alpha = 255)
{
	// OBS guarda los colores como 0xAABBGGRR
	return QColor(int(value & 0xff), int((value >> 8) & 0xff), int((value >> 16) & 0xff), alpha);
}

#ifdef _WIN32
const char *kDefaultFont = "Segoe UI";
#elif defined(__APPLE__)
const char *kDefaultFont = "Helvetica Neue";
#else
const char *kDefaultFont = "Sans Serif";
#endif

void readSettings(OverlaySource *ctx, obs_data_t *settings)
{
	OverlayStyle s;
	s.mode = OverlayStyle::Mode(qBound(0, int(obs_data_get_int(settings, "mode")), kModes - 1));
	s.skin = OverlayStyle::Skin(qBound(0, int(obs_data_get_int(settings, "skin")), int(OverlayStyle::NeonCassette)));
	s.animate = obs_data_get_bool(settings, "animate");
	s.showHowTo = obs_data_get_bool(settings, "show_howto");
	s.showWeb = obs_data_get_bool(settings, "show_web");
	s.width = qBound(64, int(obs_data_get_int(settings, "width")), 4096);
	s.height = qBound(32, int(obs_data_get_int(settings, "height")), 4096);

	obs_data_t *font = obs_data_get_obj(settings, "font");
	if (font) {
		s.fontFamily = QString::fromUtf8(obs_data_get_string(font, "face"));
		const int size = int(obs_data_get_int(font, "size"));
		if (size > 0)
			s.fontSize = size;
		const long long flags = obs_data_get_int(font, "flags");
		s.bold = (flags & OBS_FONT_BOLD) != 0;
		s.italic = (flags & OBS_FONT_ITALIC) != 0;
		obs_data_release(font);
	}

	s.textColor = colorFromObs(obs_data_get_int(settings, "text_color"));
	s.mutedColor = s.textColor;
	s.mutedColor.setAlpha(168);
	if (obs_data_has_user_value(settings, "sub_color")) { // si no se eligio: el de los titulos, mas tenue
		const long long sub = obs_data_get_int(settings, "sub_color");
		s.mutedColor = colorFromObs(sub, int((sub >> 24) & 0xff));
	}
	s.ownTextColors = obs_data_get_bool(settings, "own_text_colors");
	{
		const long long lc = obs_data_get_int(settings, "label_color");
		s.labelColor = colorFromObs(lc, int((lc >> 24) & 0xff));
	}
	if (obs_data_has_user_value(settings, "howto_color")) {
		const long long hc = obs_data_get_int(settings, "howto_color");
		s.howtoColor = colorFromObs(hc, int((hc >> 24) & 0xff));
	}
	s.format = OverlayStyle::Format(qBound(0, int(obs_data_get_int(settings, "format")), int(OverlayStyle::FormatVertical)));
	// letra de descripciones y etiquetas: solo si se eligio (si no, la de los titulos a su escala)
	if (obs_data_has_user_value(settings, "sub_font")) {
		if (obs_data_t *sf = obs_data_get_obj(settings, "sub_font")) {
			s.subFamily = QString::fromUtf8(obs_data_get_string(sf, "face"));
			const int size = int(obs_data_get_int(sf, "size"));
			if (size > 0 && s.fontSize > 0)
				s.subScale = qBound(0.3, size / (s.fontSize * 0.56), 4.0); // el tamano elegido es el del artista
			obs_data_release(sf);
		}
	}
	s.accentColor = colorFromObs(obs_data_get_int(settings, "accent_color"));
	s.backgroundColor = colorFromObs(obs_data_get_int(settings, "bg_color"));
	s.backgroundOpacity = int(obs_data_get_int(settings, "bg_opacity"));
	s.radius = int(obs_data_get_int(settings, "radius"));
	s.padding = int(obs_data_get_int(settings, "padding"));
	s.align = OverlayStyle::Align(qBound(0, int(obs_data_get_int(settings, "align")), 2));
	s.valign = OverlayStyle::VAlign(qBound(0, int(obs_data_get_int(settings, "valign")), 2));
	s.accentBar = obs_data_get_bool(settings, "accent_bar");
	s.showLabel = obs_data_get_bool(settings, "show_label");
	s.label = QString::fromUtf8(obs_data_get_string(settings, "label"));
	s.showArtist = obs_data_get_bool(settings, "show_artist");
	s.showRequester = obs_data_get_bool(settings, "show_requester");
	s.showNumbers = obs_data_get_bool(settings, "show_numbers");
	s.hideWhenEmpty = obs_data_get_bool(settings, "hide_empty");
	s.idleText = QString::fromUtf8(obs_data_get_string(settings, "idle_text"));
	s.rows = qBound(1, int(obs_data_get_int(settings, "rows")), 40);
	s.columns = qBound(1, int(obs_data_get_int(settings, "columns")), 6);
	s.barCenterPercent = qBound(20, int(obs_data_get_int(settings, "bar_center")), 60);
	s.barStyle = OverlayStyle::BarStyle(qBound(0, int(obs_data_get_int(settings, "bar_style")), 1));
	s.cardTheme = qBound(0, int(obs_data_get_int(settings, "card_theme")), kCardThemes);
	s.cardOpacity = qBound(0, int(obs_data_get_int(settings, "card_opacity")), 100);
	s.queueLook = OverlayStyle::QueueLook(qBound(0, int(obs_data_get_int(settings, "queue_look")), int(OverlayStyle::QueueFan)));
	if (obs_data_t *howto = obs_data_get_obj(settings, "howto_font")) {
		s.howtoFamily = QString::fromUtf8(obs_data_get_string(howto, "face"));
		s.howtoSize = qBound(0, int(obs_data_get_int(howto, "size")), 200);
		s.howtoBold = (obs_data_get_int(howto, "flags") & OBS_FONT_BOLD) != 0;
		obs_data_release(howto);
	}

	ctx->width = uint32_t(s.width);
	ctx->height = uint32_t(s.height);
	{
		std::lock_guard<std::mutex> lock(ctx->shared->mutex);
		ctx->shared->style = s;
		ctx->shared->listOnDemand = obs_data_get_bool(settings, "list_on_demand");
		ctx->shared->pageSeconds = qBound(2, int(obs_data_get_int(settings, "page_seconds")), 120);
		ctx->shared->styleVersion++;
	}
	Plugin::notifySourcesChanged();
}

const char *sourceName(void *)
{
	return obs_module_text("Source.Name");
}

void *sourceCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *ctx = new OverlaySource;
	ctx->source = source;
	readSettings(ctx, settings);
	Plugin::addSource(ctx->shared);
	return ctx;
}

void sourceDestroy(void *data)
{
	auto *ctx = static_cast<OverlaySource *>(data);
	ctx->shared->alive = false;
	Plugin::notifySourcesChanged();
	if (ctx->texture) {
		obs_enter_graphics();
		gs_texture_destroy(ctx->texture);
		obs_leave_graphics();
	}
	delete ctx;
}

void sourceUpdate(void *data, obs_data_t *settings)
{
	readSettings(static_cast<OverlaySource *>(data), settings);
}

uint32_t sourceWidth(void *data)
{
	return static_cast<OverlaySource *>(data)->width;
}

uint32_t sourceHeight(void *data)
{
	return static_cast<OverlaySource *>(data)->height;
}

void sourceShow(void *data)
{
	static_cast<OverlaySource *>(data)->shared->showing = true;
	Plugin::notifySourcesChanged();
}

void sourceHide(void *data)
{
	static_cast<OverlaySource *>(data)->shared->showing = false;
}

// Hilo grafico: sube la imagen nueva (si la hay) y la dibuja.
void sourceRender(void *data, gs_effect_t *effect)
{
	auto *ctx = static_cast<OverlaySource *>(data);

	QImage image;
	{
		std::lock_guard<std::mutex> lock(ctx->shared->mutex);
		if (ctx->shared->imageDirty) {
			image = std::move(ctx->shared->image);
			ctx->shared->image = QImage();
			ctx->shared->imageDirty = false;
		}
	}
	if (!image.isNull()) {
		const uint32_t w = uint32_t(image.width()), h = uint32_t(image.height());
		if (ctx->texture && (w != ctx->texW || h != ctx->texH)) {
			gs_texture_destroy(ctx->texture);
			ctx->texture = nullptr;
		}
		if (!ctx->texture) {
			const uint8_t *bits = image.constBits();
			ctx->texture = gs_texture_create(w, h, GS_RGBA, 1, &bits, GS_DYNAMIC);
			ctx->texW = w;
			ctx->texH = h;
		} else {
			gs_texture_set_image(ctx->texture, image.constBits(), uint32_t(image.bytesPerLine()), false);
		}
	}
	if (!ctx->texture)
		return;

	// La imagen viene con alfa premultiplicado.
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture_srgb(param, ctx->texture);
	gs_draw_sprite(ctx->texture, 0, ctx->texW, ctx->texH);
	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(previous);
}

void setVisible(obs_properties_t *props, const char *name, bool visible)
{
	if (obs_property_t *p = obs_properties_get(props, name))
		obs_property_set_visible(p, visible);
}

bool modeChanged(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const int mode = qBound(0, int(obs_data_get_int(settings, "mode")), kModes - 1);
	const bool np = mode == OverlayStyle::NowPlaying;
	const bool queue = mode == OverlayStyle::Queue;
	const bool list = mode == OverlayStyle::SongList;
	const bool bar = mode == OverlayStyle::Bar; // los tres a la vez, siempre en cristal
	// Los skins retro traen su paleta: solo usan el color de acento.
	const bool minimal = bar || obs_data_get_int(settings, "skin") == OverlayStyle::Minimal;
	// Tarjeta con un tema listo: los colores los pone el tema
	const bool card = bar && obs_data_get_int(settings, "bar_style") == OverlayStyle::BarCard;
	const bool themed = card && obs_data_get_int(settings, "card_theme") > 0;
	setVisible(props, "skin", !bar);
	// colores de letra: siempre a mano; sobre un tema o un skin retro, al marcar la casilla
	const bool own = obs_data_get_bool(settings, "own_text_colors");
	const bool palette = themed || !minimal;
	setVisible(props, "own_text_colors", palette);
	setVisible(props, "text_color", !palette || own);
	setVisible(props, "sub_color", !palette || own);
	setVisible(props, "label_color", !palette || own);
	setVisible(props, "howto_color", list || bar);
	setVisible(props, "format", np);
	setVisible(props, "bg_color", minimal && !themed);
	setVisible(props, "bg_opacity", minimal && !card);
	setVisible(props, "card_opacity", card);
	setVisible(props, "queue_look", card);
	setVisible(props, "howto_font", card);
	setVisible(props, "accent_color", !themed || own);
	setVisible(props, "card_theme", card);
	setVisible(props, "radius", minimal && !themed);
	setVisible(props, "padding", minimal || !np);
	setVisible(props, "animate", bar || (!minimal && np));
	setVisible(props, "show_howto", list || bar); // las instrucciones van con la lista
	setVisible(props, "show_web", list || bar);
	setVisible(props, "accent_bar", np && minimal);
	setVisible(props, "idle_text", np || bar);
	setVisible(props, "show_requester", np || queue || bar);
	setVisible(props, "show_numbers", queue || list || bar);
	setVisible(props, "hide_empty", queue);
	setVisible(props, "rows", queue || list); // en la barra las filas salen del alto
	setVisible(props, "columns", list || bar);
	setVisible(props, "list_on_demand", list || bar);
	setVisible(props, "page_seconds", list || bar);
	setVisible(props, "align", !bar);
	setVisible(props, "valign", !bar);
	setVisible(props, "label", !bar);
	setVisible(props, "bar_center", bar);
	setVisible(props, "bar_style", bar);
	setVisible(props, "bar_hint", bar);

	// Si el tamano sigue siendo el inicial de otro modo, usa el de este.
	const int w = int(obs_data_get_int(settings, "width")), h = int(obs_data_get_int(settings, "height"));
	for (int i = 0; i < kModes; i++) {
		if (i != mode && w == kDefaults[i].width && h == kDefaults[i].height) {
			obs_data_set_int(settings, "width", kDefaults[mode].width);
			obs_data_set_int(settings, "height", kDefaults[mode].height);
			break;
		}
	}
	return true;
}

// Al pasar "sonando ahora" a vertical con el tamano por defecto, la fuente se pone de pie.
bool formatChanged(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	const int format = int(obs_data_get_int(settings, "format"));
	const long long w = obs_data_get_int(settings, "width"), h = obs_data_get_int(settings, "height");
	if (obs_data_get_int(settings, "mode") == OverlayStyle::NowPlaying) {
		if (format == OverlayStyle::FormatVertical && w == kDefaults[0].width && h == kDefaults[0].height) {
			obs_data_set_int(settings, "width", 400);
			obs_data_set_int(settings, "height", 640);
		} else if (format == OverlayStyle::FormatHorizontal && w == 400 && h == 640) {
			obs_data_set_int(settings, "width", kDefaults[0].width);
			obs_data_set_int(settings, "height", kDefaults[0].height);
		}
	}
	return modeChanged(props, p, settings);
}

obs_properties_t *sourceProperties(void *)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *mode = obs_properties_add_list(props, "mode", obs_module_text("Source.Mode"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, obs_module_text("Source.Mode.NowPlaying"), OverlayStyle::NowPlaying);
	obs_property_list_add_int(mode, obs_module_text("Source.Mode.Queue"), OverlayStyle::Queue);
	obs_property_list_add_int(mode, obs_module_text("Source.Mode.SongList"), OverlayStyle::SongList);
	obs_property_list_add_int(mode, obs_module_text("Source.Mode.Bar"), OverlayStyle::Bar);
	obs_property_set_modified_callback(mode, modeChanged);
	obs_properties_add_text(props, "bar_hint", obs_module_text("Source.BarHint"), OBS_TEXT_INFO);
	obs_property_t *barStyle = obs_properties_add_list(props, "bar_style", obs_module_text("Source.BarStyle"), OBS_COMBO_TYPE_LIST,
							   OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(barStyle, obs_module_text("Source.BarStyle.Glass"), OverlayStyle::BarGlass);
	obs_property_list_add_int(barStyle, obs_module_text("Source.BarStyle.Card"), OverlayStyle::BarCard);
	obs_property_set_modified_callback(barStyle, modeChanged);
	obs_property_t *theme = obs_properties_add_list(props, "card_theme", obs_module_text("Source.CardTheme"), OBS_COMBO_TYPE_LIST,
							OBS_COMBO_FORMAT_INT);
	for (int i = 1; i <= kCardThemes; i++)
		obs_property_list_add_int(theme, obs_module_text(QStringLiteral("Source.CardTheme.%1").arg(i).toUtf8().constData()), i);
	obs_property_list_add_int(theme, obs_module_text("Source.CardTheme.0"), 0);
	obs_property_set_modified_callback(theme, modeChanged);
	obs_properties_add_int_slider(props, "card_opacity", obs_module_text("Source.CardOpacity"), 0, 100, 1);
	obs_property_t *queueLook = obs_properties_add_list(props, "queue_look", obs_module_text("Source.QueueLook"), OBS_COMBO_TYPE_LIST,
							    OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(queueLook, obs_module_text("Source.QueueLook.Ticker"), OverlayStyle::QueueTicker);
	obs_property_list_add_int(queueLook, obs_module_text("Source.QueueLook.Covers"), OverlayStyle::QueueCovers);
	obs_property_list_add_int(queueLook, obs_module_text("Source.QueueLook.Vinyl"), OverlayStyle::QueueVinyl);
	obs_property_list_add_int(queueLook, obs_module_text("Source.QueueLook.Fan"), OverlayStyle::QueueFan);
	obs_properties_add_font(props, "howto_font", obs_module_text("Source.HowtoFont"));

	obs_property_t *skin = obs_properties_add_list(props, "skin", obs_module_text("Source.Skin"), OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(skin, obs_module_text("Source.Skin.Minimal"), OverlayStyle::Minimal);
	obs_property_list_add_int(skin, obs_module_text("Source.Skin.Cassette"), OverlayStyle::Cassette);
	obs_property_list_add_int(skin, obs_module_text("Source.Skin.Led"), OverlayStyle::Led);
	obs_property_list_add_int(skin, obs_module_text("Source.Skin.Vinyl"), OverlayStyle::Vinyl);
	obs_property_list_add_int(skin, obs_module_text("Source.Skin.Turntable"), OverlayStyle::Turntable);
	obs_property_list_add_int(skin, obs_module_text("Source.Skin.NeonCassette"), OverlayStyle::NeonCassette);
	obs_property_set_modified_callback(skin, modeChanged);
	obs_property_t *format = obs_properties_add_list(props, "format", obs_module_text("Source.Format"), OBS_COMBO_TYPE_LIST,
							 OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(format, obs_module_text("Source.Format.Auto"), OverlayStyle::FormatAuto);
	obs_property_list_add_int(format, obs_module_text("Source.Format.Horizontal"), OverlayStyle::FormatHorizontal);
	obs_property_list_add_int(format, obs_module_text("Source.Format.Vertical"), OverlayStyle::FormatVertical);
	obs_property_set_modified_callback(format, formatChanged);
	obs_properties_add_bool(props, "animate", obs_module_text("Source.Animate"));

	obs_properties_add_int(props, "width", obs_module_text("Source.Width"), 64, 4096, 10);
	obs_properties_add_int(props, "height", obs_module_text("Source.Height"), 32, 4096, 10);
	obs_properties_add_int_slider(props, "bar_center", obs_module_text("Source.BarCenter"), 20, 60, 1);

	obs_properties_add_font(props, "font", obs_module_text("Source.Font"));
	obs_properties_add_font(props, "sub_font", obs_module_text("Source.SubFont"));
	obs_property_t *own = obs_properties_add_bool(props, "own_text_colors", obs_module_text("Source.OwnTextColors"));
	obs_property_set_modified_callback(own, modeChanged);
	obs_properties_add_color(props, "text_color", obs_module_text("Source.TextColor"));
	obs_properties_add_color_alpha(props, "sub_color", obs_module_text("Source.SubColor"));
	obs_properties_add_color_alpha(props, "label_color", obs_module_text("Source.LabelColor"));
	obs_properties_add_color_alpha(props, "howto_color", obs_module_text("Source.HowtoColor"));
	obs_properties_add_color(props, "accent_color", obs_module_text("Source.AccentColor"));
	obs_properties_add_color(props, "bg_color", obs_module_text("Source.BgColor"));
	obs_properties_add_int_slider(props, "bg_opacity", obs_module_text("Source.BgOpacity"), 0, 100, 1);
	obs_properties_add_int_slider(props, "radius", obs_module_text("Source.Radius"), 0, 60, 1);
	obs_properties_add_int_slider(props, "padding", obs_module_text("Source.Padding"), 0, 80, 1);

	obs_property_t *align = obs_properties_add_list(props, "align", obs_module_text("Source.Align"),
							OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(align, obs_module_text("Source.Align.Left"), OverlayStyle::Left);
	obs_property_list_add_int(align, obs_module_text("Source.Align.Center"), OverlayStyle::Center);
	obs_property_list_add_int(align, obs_module_text("Source.Align.Right"), OverlayStyle::Right);
	obs_property_t *valign = obs_properties_add_list(props, "valign", obs_module_text("Source.VAlign"),
							 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(valign, obs_module_text("Source.VAlign.Top"), OverlayStyle::Top);
	obs_property_list_add_int(valign, obs_module_text("Source.VAlign.Middle"), OverlayStyle::Middle);
	obs_property_list_add_int(valign, obs_module_text("Source.VAlign.Bottom"), OverlayStyle::Bottom);

	obs_properties_add_bool(props, "accent_bar", obs_module_text("Source.AccentBar"));
	obs_properties_add_bool(props, "show_label", obs_module_text("Source.ShowLabel"));
	obs_properties_add_text(props, "label", obs_module_text("Source.Label"), OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "show_artist", obs_module_text("Source.ShowArtist"));
	obs_properties_add_bool(props, "show_requester", obs_module_text("Source.ShowRequester"));
	obs_properties_add_bool(props, "show_numbers", obs_module_text("Source.ShowNumbers"));
	obs_properties_add_bool(props, "show_howto", obs_module_text("Source.ShowHowTo"));
	obs_properties_add_bool(props, "show_web", obs_module_text("Source.ShowWeb"));
	obs_properties_add_text(props, "idle_text", obs_module_text("Source.IdleText"), OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "hide_empty", obs_module_text("Source.HideEmpty"));
	obs_properties_add_int(props, "rows", obs_module_text("Source.Rows"), 1, 40, 1);
	obs_properties_add_int(props, "columns", obs_module_text("Source.Columns"), 1, 6, 1);
	obs_properties_add_bool(props, "list_on_demand", obs_module_text("Source.ListOnDemand"));
	obs_properties_add_int(props, "page_seconds", obs_module_text("Source.PageSeconds"), 2, 120, 1);
	return props;
}

void sourceDefaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "mode", OverlayStyle::NowPlaying);
	obs_data_set_default_int(settings, "skin", OverlayStyle::Minimal);
	obs_data_set_default_bool(settings, "animate", true);
	obs_data_set_default_bool(settings, "show_howto", true);
	obs_data_set_default_bool(settings, "show_web", true);
	obs_data_set_default_int(settings, "width", kDefaults[0].width);
	obs_data_set_default_int(settings, "height", kDefaults[0].height);

	obs_data_t *font = obs_data_create();
	obs_data_set_default_string(font, "face", kDefaultFont);
	obs_data_set_default_int(font, "size", kDefaults[0].fontSize);
	obs_data_set_default_int(font, "flags", OBS_FONT_BOLD);
	obs_data_set_default_obj(settings, "font", font);
	obs_data_release(font);

	obs_data_set_default_int(settings, "text_color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "sub_color", 0xA8FFFFFF);
	obs_data_set_default_int(settings, "label_color", 0xFFF8BD38); // #38BDF8, independiente del acento
	obs_data_set_default_int(settings, "howto_color", 0xA8FFFFFF);
	obs_data_set_default_int(settings, "format", OverlayStyle::FormatAuto);
	obs_data_t *subFont = obs_data_create();
	obs_data_set_default_string(subFont, "face", kDefaultFont);
	obs_data_set_default_int(subFont, "size", 22);
	obs_data_set_default_obj(settings, "sub_font", subFont);
	obs_data_release(subFont);
	obs_data_set_default_bool(settings, "own_text_colors", false);
	obs_data_set_default_int(settings, "accent_color", 0xFFF8BD38); // #38BDF8
	obs_data_set_default_int(settings, "bg_color", 0xFF141010);     // #101014
	obs_data_set_default_int(settings, "bg_opacity", 85);
	obs_data_set_default_int(settings, "radius", 14);
	obs_data_set_default_int(settings, "padding", 22);
	obs_data_set_default_int(settings, "align", OverlayStyle::Left);
	obs_data_set_default_int(settings, "valign", OverlayStyle::Top);
	obs_data_set_default_bool(settings, "accent_bar", true);
	obs_data_set_default_bool(settings, "show_label", true);
	obs_data_set_default_string(settings, "label", "");
	obs_data_set_default_bool(settings, "show_artist", true);
	obs_data_set_default_bool(settings, "show_requester", true);
	obs_data_set_default_bool(settings, "show_numbers", true);
	obs_data_set_default_string(settings, "idle_text", "");
	obs_data_set_default_bool(settings, "hide_empty", false);
	obs_data_set_default_int(settings, "rows", 6);
	obs_data_set_default_int(settings, "columns", 3);
	obs_data_set_default_bool(settings, "list_on_demand", false);
	obs_data_set_default_int(settings, "page_seconds", 8);
	obs_data_set_default_int(settings, "bar_center", 34);
	obs_data_set_default_int(settings, "bar_style", OverlayStyle::BarGlass);
	obs_data_set_default_int(settings, "card_theme", 1);
	obs_data_set_default_int(settings, "card_opacity", 100);
	obs_data_set_default_int(settings, "queue_look", OverlayStyle::QueueTicker);
	obs_data_t *howto = obs_data_create();
	obs_data_set_default_string(howto, "face", kDefaultFont);
	obs_data_set_default_int(howto, "size", 15);
	obs_data_set_default_int(howto, "flags", OBS_FONT_BOLD);
	obs_data_set_default_obj(settings, "howto_font", howto);
	obs_data_release(howto);
}

} // namespace

void registerOverlaySource()
{
	obs_source_info info = {};
	info.id = "ac_songlist_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
	info.icon_type = OBS_ICON_TYPE_TEXT;
	info.get_name = sourceName;
	info.create = sourceCreate;
	info.destroy = sourceDestroy;
	info.update = sourceUpdate;
	info.get_defaults = sourceDefaults;
	info.get_properties = sourceProperties;
	info.get_width = sourceWidth;
	info.get_height = sourceHeight;
	info.video_render = sourceRender;
	info.show = sourceShow;
	info.hide = sourceHide;
	obs_register_source(&info);
}

} // namespace acsl

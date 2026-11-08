#include "fan_control.h"
#include "powerui.h"

#include <string>

LV_IMG_DECLARE(fan_on);

using namespace powerui;

FanControl::FanControl(lv_obj_t *parent, const char *name, int y, int height, lv_event_cb_t cb, void *user_data,
                       const lv_img_dsc_t *frame, const lv_img_dsc_t *blades)
  : cont(card(parent, 12, y, 712, height))
{
  // Compact cards (short height): everything on one row, as in the fans with temperature targets layout.
  const bool compact = height < 100;
  const int top = compact ? (height - 44) / 2 : 16;
  icon_tile = plain(cont);
  lv_obj_set_size(icon_tile, px(44), px(44));
  lv_obj_set_pos(icon_tile, px(20), px(top));
  lv_obj_set_style_radius(icon_tile, px(10), 0);
  lv_obj_set_style_bg_color(icon_tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(icon_tile, LV_OPA_COVER, 0);
  icon_img = icon(icon_tile, &fan_on, 28, lv_color_hex(COLOR_MUTED));
  lv_obj_center(icon_img);

  // Icona grande a colori: telaio fermo e pale sopra, che girano quando la ventola va.
  int text_x = 76;
  if (frame != NULL && blades != NULL) {
    lv_obj_add_flag(icon_tile, LV_OBJ_FLAG_HIDDEN);
    const int icon_y = (height - 64) / 2;
    for (lv_obj_t **img : {&frame_img, &blades_img}) {
      *img = lv_img_create(cont);
      lv_img_set_src(*img, img == &frame_img ? frame : blades);
      lv_obj_set_pos(*img, px(14), px(icon_y));
      lv_obj_clear_flag(*img, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_img_recolor(*img, lv_color_hex(COLOR_MUTED), 0);
      lv_obj_set_style_img_recolor_opa(*img, LV_OPA_80, 0);
    }
    lv_img_set_pivot(blades_img, 32, 32);
    lv_img_set_antialias(blades_img, true);
    text_x = 92;
  }

  name_label = label(cont, name, &lv_font_montserrat_18, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(name_label, px(text_x), px(compact ? top - 2 : 16));
  // Acceso / spento grande: verde quando la ventola gira.
  state_label = label(cont, "Off", &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(state_label, px(text_x), px(compact ? top + 24 : 42));

  value_label = label(cont, "0%", &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
  if (compact) {
    lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, -px(212), px(top + 2));
  } else {
    lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, -px(20), px(16));
  }

  const int controls_y = height - 52;
  slider = lv_slider_create(cont);
  lv_slider_set_range(slider, 0, 100);
  // Barra piu' a destra: il pallino a 0 non tocca il nome della ventola.
  lv_obj_set_size(slider, px(compact ? 150 : 444), px(14));
  lv_obj_set_pos(slider, px(compact ? 276 : 22), px(compact ? height / 2 - 7 : controls_y + 15));
  lv_obj_set_style_bg_color(slider, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(COLOR_FG), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, px(6), LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_add_event_cb(slider, &FanControl::_handle_value_changed, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(slider, cb, LV_EVENT_RELEASED, user_data);

  auto make_button = [&](const char *text, int x, bool primary) {
    lv_obj_t *button = lv_btn_create(cont);
    lv_obj_set_size(button, px(compact ? 92 : 96), px(44));
    lv_obj_set_pos(button, px(x), px(compact ? top : controls_y));
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, px(10), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    const lv_color_t accent = lv_color_hex(COLOR_ACCENT);
    lv_obj_set_style_bg_color(button, primary ? accent : lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, primary ? LV_OPA_20 : LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, primary ? accent : lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(button, primary ? LV_OPA_50 : LV_OPA_10, LV_PART_MAIN);
    lv_obj_t *text_label = label(button, text, &lv_font_montserrat_16, primary ? accent : lv_color_hex(COLOR_FG));
    lv_obj_center(text_label);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    return button;
  };
  off_btn = make_button("Off", compact ? 508 : 502, false);
  max_btn = make_button("Max", compact ? 608 : 606, true);
}

FanControl::~FanControl() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void FanControl::update_value(int value) {
  lv_slider_set_value(slider, value, LV_ANIM_OFF);
  lv_label_set_text(value_label, (std::to_string(value) + "%").c_str());
  lv_label_set_text(state_label, value > 0 ? "Running" : "Off");
  lv_obj_set_style_text_color(state_label, lv_color_hex(value > 0 ? COLOR_ACCENT : COLOR_MUTED), 0);
  icon_set_color(icon_img, lv_color_hex(value > 0 ? COLOR_ACCENT : COLOR_MUTED));

  // Accesa: colori pieni. Spenta: grigia e ferma.
  for (lv_obj_t *img : {frame_img, blades_img}) {
    if (img != NULL) {
      lv_obj_set_style_img_recolor_opa(img, value > 0 ? LV_OPA_TRANSP : LV_OPA_80, 0);
    }
  }
  speed = value;
}

void FanControl::spin(uint32_t dt) {
  if (blades_img == NULL || speed <= 0) {
    return;
  }
  // Al 100% un giro e mezzo al secondo (5400 decimi di grado); piu' piano se la ventola e' piu' bassa.
  const int step = static_cast<int>(5400LL * speed / 100 * dt / 1000);
  angle = (angle + (step > 0 ? step : 1)) % 3600;
  lv_img_set_angle(blades_img, angle);
}

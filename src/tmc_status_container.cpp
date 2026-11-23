#include "tmc_status_container.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <cstdlib>

namespace {

// Nome del motore in italiano a partire dal nome Klipper (stepper_x -> Motore X).
const char *motor_title(const std::string &stepper) {
  if (stepper == "stepper_x") return "Motore X";
  if (stepper == "stepper_y") return "Motore Y";
  if (stepper == "stepper_z") return "Motore Z";
  if (stepper == "extruder") return "Estrusore";
  return stepper.c_str();
}

// Etichetta che va a capo dentro la larghezza del contenitore.
lv_obj_t *wrap_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
  lv_obj_t *l = powerui::label(parent, text, font, lv_color_hex(color));
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  return l;
}

// Colonna trasparente a tutta larghezza (sezione della card).
lv_obj_t *column(lv_obj_t *parent, int gap) {
  lv_obj_t *c = powerui::plain(parent);
  lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, powerui::px(gap), 0);
  return c;
}

// Riquadro di un valore: titolo (18), valore grande (24) e spiegazione (16). Restituisce il riquadro;
// l'etichetta del valore va in *value.
lv_obj_t *tile(lv_obj_t *parent, const char *title, const char *help, lv_obj_t **value) {
  using namespace powerui;
  lv_obj_t *t = plain(parent);
  lv_obj_set_size(t, LV_PCT(32), LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(t, px(130), 0);
  lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(t, px(12), 0);
  lv_obj_set_style_pad_row(t, px(6), 0);
  lv_obj_set_style_bg_color(t, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(t, px(10), 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_border_color(t, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(t, LV_OPA_10, 0);

  wrap_label(t, title, &lv_font_montserrat_18, COLOR_MUTED);
  *value = wrap_label(t, "--", &lv_font_montserrat_24, COLOR_FG);
  if (help != NULL) {
    wrap_label(t, help, &lv_font_montserrat_16, COLOR_MUTED);
  }
  return t;
}

// Barra orizzontale alta 14 px (sola lettura) con il colore indicato.
lv_obj_t *value_bar(lv_obj_t *parent, uint32_t color, int max) {
  using namespace powerui;
  lv_obj_t *b = lv_bar_create(parent);
  lv_obj_set_size(b, LV_PCT(100), px(14));
  lv_bar_set_range(b, 0, max);
  lv_bar_set_value(b, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  return b;
}

// Pulsante quadrato 52x52 con un simbolo (meno o piu').
lv_obj_t *step_button(lv_obj_t *parent, const char *symbol) {
  using namespace powerui;
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, px(52), px(52));
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(btn, px(10), 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(btn, LV_OPA_10, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_PRESSED), LV_STATE_PRESSED);
  lv_obj_t *l = label(btn, symbol, &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_center(l);
  return btn;
}

// Valore di una sezione della configurazione (es. run_current di "tmc2209 stepper_x"),
// letto senza creare voci vuote nello stato. Stringa vuota se manca.
std::string config_value(const std::string &section, const std::string &key) {
  const json *n = &State::get_instance()->get_data();
  for (const std::string &k : {std::string("printer_state"), std::string("configfile"), std::string("config"),
			       section, key}) {
    if (!n->is_object() || !n->contains(k)) {
      return "";
    }
    n = &n->at(k);
  }
  return n->is_string() ? n->get<std::string>() : "";
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Campo regolabile

TmcFieldControl::TmcFieldControl(lv_obj_t *parent,
				 const char *title,
				 const char *help,
				 int min,
				 int max,
				 std::function<void(int)> update_cb)
  : slider(NULL)
  , value_label(NULL)
  , cb(update_cb)
{
  using namespace powerui;
  // Riga: a sinistra nome e spiegazione, a destra  [-]  cursore  valore  [+]
  lv_obj_t *row = plain(parent);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, px(16), 0);

  lv_obj_t *text = plain(row);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(text, px(2), 0);
  wrap_label(text, title, &lv_font_montserrat_18, COLOR_FG);
  wrap_label(text, help, &lv_font_montserrat_16, COLOR_MUTED);

  lv_obj_t *ctrl = plain(row);
  lv_obj_set_size(ctrl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(ctrl, px(18), 0);

  lv_obj_t *minus = step_button(ctrl, LV_SYMBOL_MINUS);
  lv_obj_add_event_cb(minus, [](lv_event_t *e) {
    ((TmcFieldControl*)e->user_data)->button_event(lv_event_get_code(e), -1);
  }, LV_EVENT_ALL, this);

  slider = lv_slider_create(ctrl);
  lv_obj_set_size(slider, px(140), px(14));
  style_slider(slider);
  lv_obj_set_style_pad_all(slider, px(8), LV_PART_KNOB);  // pallino grande (30 px)
  lv_slider_set_range(slider, min, max);
  lv_slider_set_value(slider, min, LV_ANIM_OFF);
  current = min;
  lv_obj_add_event_cb(slider, [](lv_event_t *e) {
    TmcFieldControl *f = (TmcFieldControl*)e->user_data;
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
      f->show_value();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
      f->send_value();
    }
  }, LV_EVENT_ALL, this);

  value_label = label(ctrl, "", &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
  lv_obj_set_width(value_label, px(40));
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *plus = step_button(ctrl, LV_SYMBOL_PLUS);
  lv_obj_add_event_cb(plus, [](lv_event_t *e) {
    ((TmcFieldControl*)e->user_data)->button_event(lv_event_get_code(e), 1);
  }, LV_EVENT_ALL, this);

  show_value();
}

void TmcFieldControl::button_event(lv_event_code_t code, int delta) {
  if (code == LV_EVENT_SHORT_CLICKED) {
    step(delta);
    send_value();
  } else if (code == LV_EVENT_LONG_PRESSED_REPEAT) {
    step(delta);
    repeating = true;
  } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && repeating) {
    repeating = false;
    send_value();
  }
}

void TmcFieldControl::step(int delta) {
  lv_slider_set_value(slider, lv_slider_get_value(slider) + delta, LV_ANIM_OFF);
  show_value();
}

void TmcFieldControl::show_value() {
  lv_label_set_text(value_label, std::to_string(lv_slider_get_value(slider)).c_str());
}

void TmcFieldControl::send_value() {
  int v = lv_slider_get_value(slider);
  if (v != current) {
    current = v;
    if (cb) {
      cb(v);
    }
  }
}

void TmcFieldControl::update_value(int v) {
  // Mentre l'utente sta toccando il cursore o tiene premuto un pulsante non lo si sposta.
  if (lv_obj_has_state(slider, LV_STATE_PRESSED) || repeating) {
    return;
  }
  current = v;
  if (lv_slider_get_value(slider) != v) {
    lv_slider_set_value(slider, v, LV_ANIM_OFF);
    show_value();
  }
}

// ---------------------------------------------------------------------------------------------
// Card di un motore

TmcStatusContainer::TmcStatusContainer(KWebSocketClient &c,
				       lv_obj_t *parent,
				       const std::string &stepper_name)
  : ws(c)
  , driver(stepper_name)
  , driver_type(stepper_name.substr(0, stepper_name.find(' ')))
  , cont(powerui::card(parent, 0, 0, 712, 100))
  , chart(NULL)
{
  using namespace powerui;
  const std::string short_name = stepper_name.substr(stepper_name.find(' ') + 1);

  lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(cont, px(16), 0);
  lv_obj_set_style_pad_row(cont, px(18), 0);

  // Intestazione: nome del motore, driver e stato complessivo.
  lv_obj_t *header = plain(cont);
  lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(header, px(12), 0);
  lv_obj_t *names = column(header, 2);
  lv_obj_set_flex_grow(names, 1);
  wrap_label(names, motor_title(short_name), &lv_font_montserrat_24, COLOR_FG);
  lv_obj_t *sub = label(names, "", &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
  lv_label_set_text_fmt(sub, "Driver %s, %s", driver_type.c_str(), short_name.c_str());
  status_badge = badge(header, "Nessun problema", lv_color_hex(COLOR_ACCENT));
  lv_obj_set_height(status_badge, px(34));
  lv_obj_set_style_text_font(lv_obj_get_child(status_badge, 1), &lv_font_montserrat_16, 0);

  // Riquadri dei valori: tre per riga.
  lv_obj_t *tiles = plain(cont);
  lv_obj_set_size(tiles, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(tiles, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(tiles, px(12), 0);
  lv_obj_set_style_pad_column(tiles, px(10), 0);

  // SG_RESULT: 0..510 sui TMC2208/2209, 0..1023 sugli altri.
  const bool is_2209 = driver_type == "tmc2209" || driver_type == "tmc2208";
  lv_obj_t *t = tile(tiles, "Corrente attuale", NULL, &irms_label);
  irms_bar = value_bar(t, COLOR_MAT_RED, 2000);
  wrap_label(t, "assorbita adesso dal motore", &lv_font_montserrat_16, COLOR_MUTED);

  t = tile(tiles, "Carico motore", NULL, &sg_label);
  sg_bar = value_bar(t, COLOR_MAT_ORANGE, is_2209 ? 510 : 1023);
  wrap_label(t, "più è basso, più il motore fatica", &lv_font_montserrat_16, COLOR_MUTED);

  tile(tiles, "Temperatura", "soglia raggiunta dal chip del driver", &temp_label);
  if (!is_2209) {
    lv_label_set_text(temp_label, "Non disponibile");
  }

  tile(tiles, "Corrente di marcia", "quando il motore si muove", &run_label);
  tile(tiles, "Corrente di attesa", "quando il motore è fermo", &hold_label);
  t = tile(tiles, "Stato", NULL, &state_label);
  mode_label = wrap_label(t, "", &lv_font_montserrat_16, COLOR_MUTED);
  config_loaded = load_config_currents();

  // Errori del driver: una riga per ogni problema segnalato, nascosta finché non compare.
  lv_obj_t *errors = column(cont, 6);
  wrap_label(errors, "Errori del driver", &lv_font_montserrat_20, COLOR_FG);
  no_error_label = wrap_label(errors, "Nessun errore segnalato", &lv_font_montserrat_18, COLOR_ACCENT);
  const std::vector<std::pair<std::string, const char *>> error_texts = {
    {"ot", "Surriscaldamento: driver spento per protezione"},
    {"otpw", "Preallarme temperatura del driver"},
    {"s2ga", "Cortocircuito verso massa (fase A)"},
    {"s2gb", "Cortocircuito verso massa (fase B)"},
    {"s2vsa", "Cortocircuito verso alimentazione (fase A)"},
    {"s2vsb", "Cortocircuito verso alimentazione (fase B)"},
  };
  for (const auto &et : error_texts) {
    lv_obj_t *l = wrap_label(errors, et.second, &lv_font_montserrat_18, COLOR_DESTRUCTIVE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    error_labels.push_back({et.first, l});
  }

  // Grafico nel tempo: carico, corrente e soglie CoolStep.
  lv_obj_t *chart_sec = column(cont, 8);
  wrap_label(chart_sec, "Andamento corrente e carico", &lv_font_montserrat_20, COLOR_FG);
  lv_obj_t *legend = plain(chart_sec);
  lv_obj_set_size(legend, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(legend, px(18), 0);
  lv_obj_set_style_pad_row(legend, px(4), 0);
  label(legend, "Carico", &lv_font_montserrat_16, lv_color_hex(COLOR_MAT_ORANGE));
  label(legend, "Corrente (mA)", &lv_font_montserrat_16, lv_color_hex(COLOR_MAT_RED));
  label(legend, "Soglia CoolStep minima", &lv_font_montserrat_16, lv_color_hex(COLOR_MAT_BLUE));
  label(legend, "Soglia CoolStep massima", &lv_font_montserrat_16, lv_color_hex(COLOR_MAT_GREEN));

  // Margine a sinistra per i numeri dell'asse, sopra e sotto per non tagliarli.
  lv_obj_t *chart_wrap = plain(chart_sec);
  lv_obj_set_size(chart_wrap, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_left(chart_wrap, px(60), 0);
  lv_obj_set_style_pad_top(chart_wrap, px(12), 0);
  lv_obj_set_style_pad_bottom(chart_wrap, px(12), 0);

  chart = lv_chart_create(chart_wrap);
  lv_obj_set_size(chart, LV_PCT(100), px(180));
  sg_series = lv_chart_add_series(chart, lv_color_hex(COLOR_MAT_ORANGE), LV_CHART_AXIS_PRIMARY_Y);
  irms_series = lv_chart_add_series(chart, lv_color_hex(COLOR_MAT_RED), LV_CHART_AXIS_PRIMARY_Y);
  semin_series = lv_chart_add_series(chart, lv_color_hex(COLOR_MAT_BLUE), LV_CHART_AXIS_PRIMARY_Y);
  semax_series = lv_chart_add_series(chart, lv_color_hex(COLOR_MAT_GREEN), LV_CHART_AXIS_PRIMARY_Y);

  lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(chart, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(chart, px(8), 0);
  lv_obj_set_style_border_width(chart, 1, 0);
  lv_obj_set_style_border_color(chart, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(chart, LV_OPA_10, 0);
  lv_obj_set_style_line_color(chart, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_text_color(chart, lv_color_hex(COLOR_MUTED), LV_PART_TICKS);
  lv_obj_set_style_text_font(chart, &lv_font_montserrat_16, LV_PART_TICKS);

  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1600);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 5, 1, true, 56);

  lv_chart_set_div_line_count(chart, 3, 8);
  lv_chart_set_point_count(chart, 5000);
  lv_chart_set_zoom_x(chart, 5000);
  lv_obj_scroll_to_x(chart, LV_COORD_MAX, LV_ANIM_OFF);

  // Regolazioni avanzate: cambiano subito il driver con SET_TMC_FIELD.
  lv_obj_t *adv = column(cont, 14);
  wrap_label(adv, "Regolazioni avanzate", &lv_font_montserrat_20, COLOR_FG);
  wrap_label(adv, "Cambiano subito il driver ma non vengono salvate: al riavvio tornano i valori "
	     "della configurazione. Toccale solo se sai cosa fai.", &lv_font_montserrat_16, COLOR_MUTED);

  struct FieldSpec {
    const char *field;
    const char *title;
    const char *help;
    int max;
  };
  const FieldSpec coolstep[] = {
    {"semin", "Soglia minima (semin)", "Sotto questo carico la corrente sale. 0 = CoolStep spento.", 15},
    {"semax", "Soglia massima (semax)", "Sopra questo margine la corrente scende.", 15},
    {"seup", "Passo di aumento (seup)", "Quanto sale la corrente a ogni passo.", 3},
    {"sedn", "Velocità di riduzione (sedn)", "Quanto in fretta la corrente scende.", 3},
  };
  const FieldSpec chopper[] = {
    {"toff", "Tempo di spegnimento (toff)", "Pausa del chopper. 0 = motore senza corrente!", 15},
    {"tbl", "Tempo di blanking (tbl)", "Ignora i disturbi all'inizio di ogni impulso.", 3},
    {"hstrt", "Isteresi iniziale (hstrt)", "Inizio dell'isteresi SpreadCycle.", 7},
    {"hend", "Isteresi finale (hend)", "Fine dell'isteresi SpreadCycle.", 15},
  };

  auto add_fields = [this, &adv, &short_name](const char *group, const FieldSpec *specs, size_t count) {
    wrap_label(adv, group, &lv_font_montserrat_18, COLOR_ACCENT);
    for (size_t i = 0; i < count; ++i) {
      const std::string field = specs[i].field;
      fields[field] = std::unique_ptr<TmcFieldControl>(new TmcFieldControl(
	adv, specs[i].title, specs[i].help, 0, specs[i].max,
	[this, short_name, field](int v) { update_tmc_value(short_name, field, v); }));
    }
  };
  add_fields("CoolStep: corrente automatica secondo il carico", coolstep, 4);
  add_fields("Chopper: come il driver regola la corrente", chopper, 4);
}

TmcStatusContainer::~TmcStatusContainer()
{
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void TmcStatusContainer::set_visible(bool visible) {
  if (visible) {
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  }
}

bool TmcStatusContainer::load_config_currents() {
  const std::string run = config_value(driver, "run_current");
  if (run.empty()) {
    lv_label_set_text(run_label, "--");
    lv_label_set_text(hold_label, "--");
    return false;
  }
  const double run_a = std::strtod(run.c_str(), NULL);
  lv_label_set_text(run_label, fmt::format("{:.2f} A", run_a).c_str());
  // Senza hold_current Klipper tiene la stessa corrente di marcia anche da fermo.
  const std::string hold = config_value(driver, "hold_current");
  const double hold_a = hold.empty() ? run_a : std::strtod(hold.c_str(), NULL);
  lv_label_set_text(hold_label, fmt::format("{:.2f} A", hold_a).c_str());
  // La barra della corrente attuale arriva poco oltre la corrente di marcia.
  if (run_a > 0.1) {
    lv_bar_set_range(irms_bar, 0, (int32_t)(run_a * 1000 * 1.2));
  }
  return true;
}

void TmcStatusContainer::update(json &stepper) {
  // spdlog::debug("tmc stepper {}", stepper.dump());
  if (!stepper.is_null()) {
    if (!config_loaded) {
      config_loaded = load_config_currents();
    }

    auto v = stepper["/i_rms"_json_pointer];
    if (!v.is_null()) {
      const double ma = v.template get<double>();
      lv_chart_set_next_value(chart, irms_series, ma);
      lv_label_set_text(irms_label, fmt::format("{:.2f} A", ma / 1000.0).c_str());
      lv_bar_set_value(irms_bar, (int32_t)ma, LV_ANIM_OFF);
    }

    // drv_status only lists the flags that are set: any of these means the driver reports a fault.
    bool fault = false;
    auto drv = stepper["/drv_status"_json_pointer];
    if (drv.is_object()) {
      for (const char *flag : {"ot", "otpw", "s2ga", "s2gb", "s2vsa", "s2vsb"}) {
	fault = fault || drv.contains(flag);
      }

      // Righe degli errori: si mostrano solo quelle segnalate.
      int mask = 0;
      for (size_t i = 0; i < error_labels.size(); ++i) {
	if (drv.contains(error_labels[i].first)) {
	  mask |= 1 << i;
	}
      }
      if (mask != error_mask) {
	error_mask = mask;
	for (size_t i = 0; i < error_labels.size(); ++i) {
	  if (mask & (1 << i)) {
	    lv_obj_clear_flag(error_labels[i].second, LV_OBJ_FLAG_HIDDEN);
	  } else {
	    lv_obj_add_flag(error_labels[i].second, LV_OBJ_FLAG_HIDDEN);
	  }
	}
	if (mask == 0) {
	  lv_obj_clear_flag(no_error_label, LV_OBJ_FLAG_HIDDEN);
	} else {
	  lv_obj_add_flag(no_error_label, LV_OBJ_FLAG_HIDDEN);
	}
      }

      // Temperatura: i TMC2208/2209 dicono solo quale soglia hanno superato (120, 143, 150, 157 gradi).
      if (driver_type == "tmc2209" || driver_type == "tmc2208") {
	int level = drv.contains("t157") ? 4 : drv.contains("t150") ? 3 : drv.contains("t143") ? 2
	  : drv.contains("t120") ? 1 : 0;
	if (level != temp_level) {
	  temp_level = level;
	  static const char *const texts[] = {"Sotto 120 °C", "Oltre 120 °C", "Oltre 143 °C", "Oltre 150 °C",
					      "Oltre 157 °C"};
	  lv_label_set_text(temp_label, texts[level]);
	  uint32_t color = level == 0 ? powerui::COLOR_FG : level == 1 ? powerui::COLOR_WARNING : powerui::COLOR_DESTRUCTIVE;
	  lv_obj_set_style_text_color(temp_label, lv_color_hex(color), 0);
	}
      }

      // Stato: fermo (stst) o in movimento; modo silenzioso (stealth) o SpreadCycle.
      int now_moving = drv.contains("stst") ? 0 : 1;
      if (now_moving != moving) {
	moving = now_moving;
	lv_label_set_text(state_label, moving ? "In movimento" : "Fermo");
      }
      int now_stealth = drv.contains("stealth") ? 1 : 0;
      if (now_stealth != stealth) {
	stealth = now_stealth;
	lv_label_set_text(mode_label, stealth ? "Modo silenzioso (StealthChop)" : "Modo SpreadCycle");
      }
    }
    if (fault != alert) {
      alert = fault;
      powerui::badge_set(status_badge, fault ? "Allarme" : "Nessun problema",
			 lv_color_hex(fault ? powerui::COLOR_DANGER : powerui::COLOR_ACCENT));
    }

    v = stepper["/semin"_json_pointer];
    int semin = 0;
    if (!v.is_null()) {
      semin = v.template get<int>();
      lv_chart_set_next_value(chart, semin_series, semin * 32);
    }

    v = stepper["/semax"_json_pointer];
    if (!v.is_null()) {
      auto semax = v.template get<int>();
      lv_chart_set_next_value(chart, semax_series, (semin + semax + 1) * 32);
    }

    // Valori attuali dei campi regolabili.
    for (auto &f : fields) {
      v = stepper[json::json_pointer("/" + f.first)];
      if (!v.is_null()) {
	f.second->update_value(v.template get<int>());
      }
    }

    v = stepper["/sg_result"_json_pointer];
    if (!v.is_null()) {
      const int sg = v.template get<int>();
      lv_chart_set_next_value(chart, sg_series, sg);
      lv_label_set_text(sg_label, std::to_string(sg).c_str());
      lv_bar_set_value(sg_bar, sg, LV_ANIM_OFF);
    }
  }
}

void TmcStatusContainer::update_tmc_value(const std::string &stepper_name,
					  const std::string &field_name,
					  int value) {
  ws.gcode_script(fmt::format("SET_TMC_FIELD field={} value={} stepper={}",
			      field_name, value, stepper_name));
}

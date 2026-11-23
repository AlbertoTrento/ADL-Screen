#include "tmc_tune_panel.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <utility>

LV_IMG_DECLARE(ui_icon_save);

static std::map<std::string, int> goal_idx_map = {
  {"auto", 0 },
  {"silent", 1 },
  {"performance", 2 },
  {"autoswitch", 3 }
};

// Valori scritti nella configurazione per ogni voce del menu "Obiettivo" (stesso ordine di goal_idx_map):
// il menu mostra i nomi in italiano, il comando usa sempre questi.
static const char *const GOAL_NAMES[] = {"auto", "silent", "performance", "autoswitch"};

static std::map<std::string, std::pair<int16_t, int16_t>> tmc_sg_range = {
  {"tmc2130", { -64, 63 } },
  {"tmc2209", { 0, 255 } },
  {"tmc2240", { -64, 63 } },
  {"tmc2660", { -64, 63 } },
  {"tmc5160", { -64, 63 } }
};

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

// Riga di un parametro: a sinistra nome (18) e spiegazione (16, grigia), a destra il comando.
// Restituisce la riga: il comando va aggiunto dopo, come figlio della riga.
lv_obj_t *param_row(lv_obj_t *parent, const char *title, const char *help) {
  using namespace powerui;
  lv_obj_t *row = plain(parent);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, px(16), 0);

  lv_obj_t *text = plain(row);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(text, px(4), 0);
  wrap_label(text, title, &lv_font_montserrat_18, COLOR_FG);
  wrap_label(text, help, &lv_font_montserrat_16, COLOR_MUTED);
  return row;
}

// Menu a tendina grande (testo 20, alto circa 56 px) sia chiuso sia aperto.
void style_big_select(lv_obj_t *dd, int width) {
  using namespace powerui;
  style_select(dd);
  lv_obj_set_width(dd, px(width));
  lv_obj_set_height(dd, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(dd, px(16), LV_PART_MAIN);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_t *list = lv_dropdown_get_list(dd);
  if (list != NULL) {
    lv_obj_set_style_text_font(list, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(list, px(22), LV_PART_MAIN);
  }
  // Elenchi lunghi (molti modelli di motore): scorrono dentro la tendina invece di uscire dallo schermo.
  lv_obj_add_event_cb(dd, [](lv_event_t *e) {
    lv_obj_t *l = lv_dropdown_get_list(lv_event_get_target(e));
    if (l != NULL) {
      lv_obj_set_style_max_height(l, powerui::px(300), LV_PART_MAIN);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
      lv_obj_set_style_text_line_space(l, powerui::px(22), LV_PART_MAIN);
    }
  }, LV_EVENT_READY, NULL);
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

}  // namespace

AutoTmcContainer::AutoTmcContainer(const std::list<std::string> &motors,
				   const std::string &stepper_name,
				   int motor_idx,
				   int goal_idx,
				   bool sg_configured,
				   int16_t sgthrs,
				   std::pair<int16_t, int16_t> sg_min_max,
				   lv_obj_t *parent)
  : cont(powerui::card(parent, 0, 0, 712, 100))
  , name(stepper_name)
  , motors_dd(NULL)
  , tuning_goal_dd(NULL)
  , threshold_slider(NULL)
  , threshold_value(NULL)
  , configured_motor_idx(motor_idx)
  , configured_goal_idx(goal_idx)
  , has_sg(sg_configured)
  , configured_sensorless_thrs(sgthrs)
  , sg_range(sg_min_max)
{
  using namespace powerui;

  // Card del motore: titolo, poi una riga per parametro. Altezza secondo il contenuto.
  lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(cont, px(16), 0);
  lv_obj_set_style_pad_row(cont, px(14), 0);

  lv_obj_t *header = plain(cont);
  lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(header, px(12), 0);
  label(header, motor_title(name), &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
  label(header, name.c_str(), &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));

  // Modello del motore: "Non configurato" (indice 0) toglie la regolazione automatica per questo motore.
  lv_obj_t *row = param_row(cont, "Modello del motore",
			    "Scegli il motore montato: serve a calcolare la regolazione del driver.");
  motors_dd = lv_dropdown_create(row);
  std::string options = "Non configurato";
  for (auto &s : motors) {
    const std::string motor = s.substr(s.find(' ') + 1);
    motor_names.push_back(motor);
    options += "\n" + motor;
  }
  lv_dropdown_set_options(motors_dd, options.c_str());
  style_big_select(motors_dd, 300);
  lv_dropdown_set_selected(motors_dd, motor_idx);

  row = param_row(cont, "Obiettivo",
		  "Silenzioso, prestazioni (più coppia) o automatico.");
  tuning_goal_dd = lv_dropdown_create(row);
  lv_dropdown_set_options(tuning_goal_dd,
			  "Automatico\n"
			  "Silenzioso\n"
			  "Prestazioni\n"
			  "Cambio automatico");
  style_big_select(tuning_goal_dd, 300);
  lv_dropdown_set_selected(tuning_goal_dd, goal_idx);

  if (has_sg) {
    // Sensibilita' dell'urto per l'homing senza finecorsa: cursore grande, numero e pulsanti -/+.
    row = param_row(cont, "Sensibilità urto (homing senza finecorsa)",
		    sg_range.first == 0
		    ? "Più alto = si ferma prima contro il bordo."
		    : "Più basso = si ferma prima contro il bordo.");
    lv_obj_t *ctrl = plain(row);
    lv_obj_set_size(ctrl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl, px(18), 0);

    lv_obj_t *minus = step_button(ctrl, LV_SYMBOL_MINUS);
    lv_obj_add_event_cb(minus, [](lv_event_t *e) {
      lv_event_code_t code = lv_event_get_code(e);
      if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
	((AutoTmcContainer*)e->user_data)->change_threshold(-1);
      }
    }, LV_EVENT_ALL, this);

    threshold_slider = lv_slider_create(ctrl);
    lv_obj_set_size(threshold_slider, px(150), px(14));
    style_slider(threshold_slider);
    lv_obj_set_style_pad_all(threshold_slider, px(8), LV_PART_KNOB);  // pallino grande (30 px)
    lv_slider_set_range(threshold_slider, sg_range.first, sg_range.second);
    lv_slider_set_value(threshold_slider, sgthrs, LV_ANIM_OFF);
    lv_obj_add_event_cb(threshold_slider, [](lv_event_t *e) {
      ((AutoTmcContainer*)e->user_data)->show_threshold();
    }, LV_EVENT_VALUE_CHANGED, this);

    threshold_value = label(ctrl, "", &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
    lv_obj_set_width(threshold_value, px(52));
    lv_obj_set_style_text_align(threshold_value, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *plus = step_button(ctrl, LV_SYMBOL_PLUS);
    lv_obj_add_event_cb(plus, [](lv_event_t *e) {
      lv_event_code_t code = lv_event_get_code(e);
      if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
	((AutoTmcContainer*)e->user_data)->change_threshold(1);
      }
    }, LV_EVENT_ALL, this);

    show_threshold();
  }
}

AutoTmcContainer::~AutoTmcContainer() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void AutoTmcContainer::change_threshold(int delta) {
  if (threshold_slider != NULL) {
    lv_slider_set_value(threshold_slider, lv_slider_get_value(threshold_slider) + delta, LV_ANIM_OFF);
    show_threshold();
  }
}

void AutoTmcContainer::show_threshold() {
  if (threshold_slider != NULL && threshold_value != NULL) {
    lv_label_set_text(threshold_value, std::to_string(lv_slider_get_value(threshold_slider)).c_str());
  }
}

std::string AutoTmcContainer::get_config_macro() {
  // Indice 0 = "Non configurato": si cancella la sezione, come prima.
  uint16_t midx = lv_dropdown_get_selected(motors_dd);
  if (midx == 0 || midx > motor_names.size()) {
    return fmt::format("_PS_DELETE_CONFIG SECTION=\"autotune_tmc {}\"", name);
  }
  const std::string &motor = motor_names[midx - 1];

  uint16_t gidx = lv_dropdown_get_selected(tuning_goal_dd);
  const char *goal = GOAL_NAMES[gidx < 4 ? gidx : 0];
  if (has_sg) {
    auto sg_value = lv_slider_get_value(threshold_slider);
    return fmt::format("_PS_SAVE_CONFIG SECTION=\"autotune_tmc {}\" KEY_VALUE=\"motor:{},tuning_goal:{},{}:{}\"",
		       name, motor, goal, sg_range.first == 0 ? "sg4_thrs" : "sgt", sg_value);
  } else {
    return fmt::format("_PS_SAVE_CONFIG SECTION=\"autotune_tmc {}\" KEY_VALUE=\"motor:{},tuning_goal:{}\"",
		       name, motor, goal);
  }
}

TmcTunePanel::TmcTunePanel(KWebSocketClient &c)
  : ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , controls_cont(lv_obj_create(cont))
  , btns_cont(powerui::plain(cont))
  , save_btn(NULL)
{
  using namespace powerui;
  motor_parser._delim = ":";
  lv_obj_move_background(cont);
  style_overlay_root(cont);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

  // Elenco scorrevole: spiegazione in alto, poi una card per motore (aggiunte da init()).
  lv_obj_set_width(controls_cont, LV_PCT(100));
  lv_obj_set_flex_grow(controls_cont, 1);
  lv_obj_set_flex_flow(controls_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(controls_cont, LV_DIR_VER);
  lv_obj_set_style_bg_opa(controls_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(controls_cont, 0, 0);
  lv_obj_set_style_radius(controls_cont, 0, 0);
  lv_obj_set_style_pad_all(controls_cont, px(12), 0);
  lv_obj_set_style_pad_row(controls_cont, px(12), 0);

  lv_obj_t *intro = card(controls_cont, 0, 0, 712, 100);
  lv_obj_set_size(intro, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(intro, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(intro, px(16), 0);
  lv_obj_set_style_pad_row(intro, px(6), 0);
  wrap_label(intro, "Regolazione automatica dei driver", &lv_font_montserrat_20, COLOR_FG);
  wrap_label(intro, "Per ogni motore scegli il modello e l'obiettivo. Klipper calcola da solo "
	     "i parametri del driver TMC quando si avvia.", &lv_font_montserrat_16, COLOR_MUTED);

  // Barra in basso: spiegazione e pulsante grande Salva e riavvia.
  lv_obj_set_size(btns_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(btns_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btns_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(btns_cont, px(12), 0);
  lv_obj_set_style_pad_right(btns_cont, px(12), 0);
  lv_obj_set_style_pad_bottom(btns_cont, px(12), 0);
  lv_obj_set_style_pad_column(btns_cont, px(16), 0);

  lv_obj_t *hint = label(btns_cont, "Salva le scelte nella configurazione e riavvia Klipper.",
			 &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_flex_grow(hint, 1);

  save_btn = action_button(btns_cont, &ui_icon_save, "Save/Restart", ActionKind::Primary, 0, 0, 250, 56,
			   [](lv_event_t *e) {
			     TmcTunePanel *panel = (TmcTunePanel*)e->user_data;
			     panel->save_config();
			   }, this);
  // Testo del pulsante a 20 (l'ultimo figlio del pulsante e' l'etichetta).
  lv_obj_set_style_text_font(lv_obj_get_child(save_btn, lv_obj_get_child_cnt(save_btn) - 1),
			     &lv_font_montserrat_20, 0);
}

TmcTunePanel::~TmcTunePanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void TmcTunePanel::init(json &j, fs::path &kp) {
  if (motor_parser.LoadFromFile(kp.string().c_str()) != 0) {
    spdlog::error("Failed to load motor_databse.cfg for TMC AUTOTUNE");
    return;
  }

  motor_index.clear();
  steppers.clear();

  // 0 reserved for no configuration
  auto motors = motor_parser.GetSections();
  int motor_idx = 1;
  for (auto &s : motors) {
    motor_index.insert({s.substr(s.find(' ') + 1), motor_idx++});
  }

  State *s = State::get_instance();
  auto v = s->get_data("/printer_state/configfile/config"_json_pointer);

  std::map<std::string, json> tuned_config;
  std::vector<std::string> configured_steppers;

  for (auto &el : v.items()) {
    if (el.key().rfind("autotune_tmc ", 0) == 0) {
      // existing auto tune tmc configs
      std::string stepper_name = el.key().substr(el.key().find(' ') + 1);
      tuned_config.insert({stepper_name, el.value()});

      spdlog::debug("found tuned stepper {}, {}", stepper_name, el.value().dump());
    }

    if (el.key().rfind("tmc", 0) == 0 && el.key().find(' ') != std::string::npos) {
      spdlog::debug("found configured tmc {}", el.key());
      configured_steppers.push_back(el.key());
    }
  }

  for (auto &s : configured_steppers) {
    std::string tmctype = s.substr(0, s.find(' '));
    std::string stepper_name = s.substr(s.find(' ') + 1);
    spdlog::debug("tmctype {}, stepper {}, s {}", tmctype, stepper_name, s);

    auto endstop_pin = v[json::json_pointer(fmt::format("/{}/endstop_pin", stepper_name))];
    auto sg_range_el = tmc_sg_range.find(tmctype);
    bool has_virtual_endstop = !endstop_pin.is_null()
      && endstop_pin.template get<std::string>().find("virtual_endstop") != std::string::npos
      && sg_range_el != tmc_sg_range.end();

    spdlog::debug("has_virtual_endstop {}, {}, {}", !endstop_pin.is_null(),
		  !endstop_pin.is_null() ? endstop_pin.template get<std::string>().find("virtual_endstop") != std::string::npos : false,
		  !endstop_pin.is_null() ? endstop_pin.template get<std::string>(): "null");

    auto sg_range = sg_range_el != tmc_sg_range.end()
      ? sg_range_el->second
      : std::make_pair<int16_t, int16_t>(0, 0);

    const auto &el = tuned_config.find(stepper_name);
    if (el != tuned_config.end()) {
      auto motor = el->second["/motor"_json_pointer];
      auto goal = el->second["/tuning_goal"_json_pointer];
      auto sgthrs = el->second["/sg4_thrs"_json_pointer];

      spdlog::debug("found tuned stepper {}, {}, {}",
		    motor.is_null() ? "NULL" : motor.template get<std::string>(),
		    goal.is_null() ? "auto" : goal.template get<std::string>(),
		    sgthrs.is_null() ? 10 : std::stoi(sgthrs.template get<std::string>())
		    );

      int midx = 0;
      if (!motor.is_null()) {
	const auto &el = motor_index.find(motor.template get<std::string>());
	if (el != motor_index.end()) {
	  midx = el->second;
	}
      }

      int goal_idx = 0;
      if (!goal.is_null()) {
	const auto &el = goal_idx_map.find(goal.template get<std::string>());
	if (el != goal_idx_map.end()) {
	  goal_idx = el->second;
	}
      }

      int16_t sg_value = !sgthrs.is_null()
	? std::stoi(sgthrs.template get<std::string>())
	: (sg_range.first == 0 ? 80 : 1); // sg4 default 80, sgt default 1

      steppers.push_back(std::make_shared<AutoTmcContainer>(motors, el->first, midx, goal_idx,
							    has_virtual_endstop, sg_value, sg_range,
							    controls_cont));
    } else {
      spdlog::debug("did not find tuned config for stepper {}", s);
      int16_t sg_value = sg_range.first == 0 ? 80 : 1; // sg4 default 80, sgt default 1
      steppers.push_back(std::make_shared<AutoTmcContainer>(motors, stepper_name, 0, 0,
							    has_virtual_endstop, sg_value, sg_range,
							    controls_cont));
    }
  }
}

void TmcTunePanel::foreground() {
  lv_obj_move_foreground(cont);
  powerui::overlay_open("Regolazione driver TMC", [this]() { lv_obj_move_background(cont); });
}

void TmcTunePanel::background() {
  lv_obj_move_background(cont);
}

void TmcTunePanel::save_config() {
  std::vector<std::string> save_configs;
  for (auto &s : steppers) {
    save_configs.push_back(s->get_config_macro());
  }

  ws.gcode_script(fmt::format("{}\nSAVE_CONFIG", fmt::join(save_configs, "\n")));
}

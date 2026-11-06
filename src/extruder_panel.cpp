#include "extruder_panel.h"
#include "powerui.h"
#include "state.h"
#include "config.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <limits>

LV_IMG_DECLARE(extrude_img);
LV_IMG_DECLARE(retract_img);
LV_IMG_DECLARE(unload_filament_img);
LV_IMG_DECLARE(load_filament_img);
LV_IMG_DECLARE(filament_img);
LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(ui_hand_img);

ExtruderPanel::ExtruderPanel(KWebSocketClient &websocket_client,
			     std::mutex &lock,
			     Numpad &numpad)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , title_bar(lv_obj_create(panel_cont))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , extruder_temp(ws, panel_cont, &extruder, 150,
	  "EXTRUDER", lv_color_hex(powerui::COLOR_MAT_RED), false, true, numpad, "extruder", NULL, NULL)
  , temp_selector(panel_cont, "EXTRUDER TEMPERATURE (C)",
		  {"180", "190", "200", "210", "220", "230", "240", ""}, 6, &ExtruderPanel::_handle_callback, this)
  , length_selector(panel_cont, "EXTRUDE LENGTH (MM)",
		    {"5", "10", "15", "20", "25", "30", "35", ""}, 1, &ExtruderPanel::_handle_callback, this)
  , speed_selector(panel_cont, "EXTRUDE SPEED (MM/S)",
		   {"1", "2", "5", "10", "25", "35", "50", ""}, 2, &ExtruderPanel::_handle_callback, this)
  , rightside_btns_cont(lv_obj_create(panel_cont))
  , leftside_btns_cont(lv_obj_create(panel_cont))
  , load_btn(leftside_btns_cont, &load_filament_img, "Carica filamento", &ExtruderPanel::_handle_callback, this)
  , unload_btn(leftside_btns_cont, &unload_filament_img, "Togli filamento", &ExtruderPanel::_handle_callback, this)
  , cooldown_btn(leftside_btns_cont, &extruder, "Spegni ugello", &ExtruderPanel::_handle_callback, this)
  , manual_change_btn(leftside_btns_cont, NULL, "Cambio in pausa", &ExtruderPanel::_handle_callback, this)
  , extrude_btn(rightside_btns_cont, &extrude_img, "Spingi", &ExtruderPanel::_handle_callback, this)
  , retract_btn(rightside_btns_cont, &retract_img, "Tira indietro", &ExtruderPanel::_handle_callback, this)
{
  lv_obj_move_background(panel_cont);
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);  
  lv_obj_set_size(panel_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(panel_cont, 0, 0);
  lv_obj_set_style_pad_row(panel_cont, 1, 0);

  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(title_bar, LV_PCT(100), 32);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_set_style_pad_all(title_bar, 0, 0);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(powerui::COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(title_bar, 0, 0);

  lv_label_set_text(title_label, "FILAMENTO");
  lv_obj_set_width(title_label, LV_PCT(100));
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(title_label, lv_color_hex(powerui::COLOR_WHITE), 0);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_set_width(time_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(time_label, lv_color_hex(powerui::COLOR_WHITE), 0);
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, 0);
  lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -10, 0);
  update_clock();
  clock_timer = lv_timer_create(&ExtruderPanel::_update_clock_cb, 1000, this);

  // PowerUI layout for the overlay area (736 x 440): left rail, three option cards, right rail.
  using namespace powerui;
  lv_obj_set_style_radius(panel_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(panel_cont, lv_color_hex(powerui::COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel_cont, 0, LV_PART_MAIN);

  // The main title bar replaces the panel's own one; the clock timer keeps running on the hidden label.
  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(leftside_btns_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(rightside_btns_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);

  auto put = [this](ButtonContainer &button, int x, int y, int w, int h) {
    lv_obj_set_parent(button.get_container(), panel_cont);
    button.set_fixed_size(px(w), px(h));
    lv_obj_set_pos(button.get_container(), px(x), px(y));
  };

  // Ogni pulsante dice cosa fa: nome sopra e spiegazione sotto. Solo ASCII: i font non hanno le lettere accentate.
  auto describe = [](ButtonContainer &button, const lv_font_t *font, const char *text) {
    lv_obj_add_flag(button.get_button(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(button.get_label(), font, 0);
    lv_obj_align(button.get_label(), LV_ALIGN_TOP_MID, 0, px(12));
    lv_obj_t *info = label(button.get_container(), text, &lv_font_montserrat_12, lv_color_hex(powerui::COLOR_MUTED));
    lv_obj_set_width(info, LV_PCT(92));
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -px(10));
  };

  // Pagina Filamento (736 x 440): temperatura, carica e togli, lunghezza, spingi, tira indietro e spegni ugello.
  put(load_btn, 12, 122, 350, 108);
  put(unload_btn, 374, 122, 350, 108);
  put(extrude_btn, 12, 342, 228, 86);
  put(retract_btn, 252, 342, 228, 86);
  put(cooldown_btn, 492, 342, 232, 86);
  put(manual_change_btn, 280, 18, 210, 36);

  style_button(load_btn.get_container(), load_btn.get_button(), ButtonKind::Soft);
  for (ButtonContainer *button : {&unload_btn, &extrude_btn, &retract_btn, &cooldown_btn, &manual_change_btn}) {
    style_button(button->get_container(), button->get_button(), ButtonKind::Outline);
  }

  describe(load_btn, &lv_font_montserrat_20, "Porta il filamento all'ugello, spurga e pulisce");
  describe(unload_btn, &lv_font_montserrat_20, "Scalda e tira indietro: poi lo sfili a mano");
  describe(extrude_btn, &lv_font_montserrat_16, "Spinge in avanti il filamento dei mm scelti");
  describe(retract_btn, &lv_font_montserrat_16, "Tira indietro il filamento dei mm scelti");
  describe(cooldown_btn, &lv_font_montserrat_16, "Spegne il riscaldamento dell'ugello");

  // Il cambio filamento serve solo con la stampa in pausa: fuori dalla pausa il pulsante non si vede.
  lv_obj_add_flag(manual_change_btn.get_button(), LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(manual_change_btn.get_label(), &lv_font_montserrat_14, 0);
  lv_obj_align(manual_change_btn.get_label(), LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(manual_change_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

  // La velocita' resta quella predefinita del selettore: nella pagina non serve sceglierla.
  lv_obj_add_flag(speed_selector.get_container(), LV_OBJ_FLAG_HIDDEN);

  // Riquadri con titolo sopra e selettore sotto.
  struct CardSpec { Selector *selector; const char *title; int y; int h; };
  const CardSpec specs[2] = {{&temp_selector, "Temperatura ugello (°C)", 12, 100},
                             {&length_selector, "Lunghezza per Spingi e Tira indietro (mm)", 240, 92}};
  lv_obj_t *temp_card = NULL;
  for (const CardSpec &spec : specs) {
    lv_obj_t *option_card = card(panel_cont, 12, spec.y, 712, spec.h);
    lv_obj_t *title = label(option_card, spec.title, &lv_font_montserrat_14, lv_color_hex(powerui::COLOR_MUTED));
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, px(15), px(10));

    lv_obj_t *selector_cont = spec.selector->get_container();
    lv_obj_set_parent(selector_cont, option_card);
    lv_obj_add_flag(spec.selector->get_label(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(selector_cont, px(682), px(44));
    lv_obj_align(selector_cont, LV_ALIGN_BOTTOM_MID, 0, -px(8));
    lv_obj_set_size(spec.selector->get_selector(), px(682), px(44));
    style_segmented(spec.selector->get_selector());
    if (spec.selector == &temp_selector) {
      temp_card = option_card;
    }
  }
  lv_obj_move_foreground(manual_change_btn.get_container());

  // Temperatura attuale dell'ugello accanto al titolo.
  lv_obj_set_parent(extruder_temp.get_sensor(), temp_card);
  lv_obj_clear_flag(extruder_temp.get_sensor(), LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(extruder_temp.get_sensor(), px(120), px(32));
  extruder_temp.set_current_only(px(70), px(5));
  extruder_temp.set_image_zoom(100);
  lv_obj_align(extruder_temp.get_sensor(), LV_ALIGN_TOP_RIGHT, -px(10), px(2));

  ws.register_notify_update(this);    
}

ExtruderPanel::~ExtruderPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
}

void ExtruderPanel::foreground() {
  auto &state = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
  if (state.is_string()) {
    update_pause_button(state.template get<std::string>());
  }
  lv_obj_move_foreground(panel_cont);
  powerui::overlay_open("Filamento", [this]() { lv_obj_move_background(panel_cont); });
}

void ExtruderPanel::update_pause_button(const std::string &state) {
  if (state == "paused") {
    lv_obj_clear_flag(manual_change_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(manual_change_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  }
}

void ExtruderPanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void ExtruderPanel::show_manual_filament_change() {
  // The window opens locally so it does not wait for Klipper, which may be busy with another macro.
  // Without an opener, the menu defined in powerscreen_cmd.cfg is requested instead.
  if (manual_change_opener) {
    manual_change_opener();
    return;
  }
  ws.gcode_script("_PS_MANUAL_CHANGE_PROMPT");
}

void ExtruderPanel::consume(json& j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto target_value = j["/params/0/extruder/target"_json_pointer];
  if (!target_value.is_null()) {
    int target = target_value.template get<int>();
    extruder_temp.update_target(target);
  }
  
  auto temp_value = j["/params/0/extruder/temperature"_json_pointer];
  if (!temp_value.is_null()) {   
    int value = temp_value.template get<int>();
    extruder_temp.update_value(value);
  }

  auto state_value = j["/params/0/print_stats/state"_json_pointer];
  if (state_value.is_string()) {
    update_pause_button(state_value.template get<std::string>());
  }
}

void ExtruderPanel::handle_callback(lv_event_t *e) {
  spdlog::trace("handling extruder panel callback");
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *selector = lv_event_get_target(e);
    uint32_t idx = lv_btnmatrix_get_selected_btn(selector);
    const char * v = lv_btnmatrix_get_btn_text(selector, idx);

    if (selector == temp_selector.get_selector()) {
      temp_selector.set_selected_idx(idx);
    }

    if (selector == length_selector.get_selector()) {
      length_selector.set_selected_idx(idx);
    }

    if (selector == speed_selector.get_selector()) {
      speed_selector.set_selected_idx(idx);
    }

    spdlog::trace("selector {} {} {}, {} {} {}", fmt::ptr(selector), idx, v,
		  fmt::ptr(temp_selector.get_selector()),
		  fmt::ptr(length_selector.get_selector()),
		  fmt::ptr(speed_selector.get_selector()));
    
  } else if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);

    // Durante una stampa questi comandi non devono partire.
    auto &state = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
    if (state.is_string() && state.template get<std::string>() == "printing") {
      return;
    }

    const char *temp = lv_btnmatrix_get_btn_text(temp_selector.get_selector(), temp_selector.get_selected_idx());
    const char *len = lv_btnmatrix_get_btn_text(length_selector.get_selector(), length_selector.get_selected_idx());
    const char *speed = lv_btnmatrix_get_btn_text(speed_selector.get_selector(), speed_selector.get_selected_idx());

    if (btn == extrude_btn.get_container()) {
      ws.gcode_script(fmt::format("M109 S{}\nM83\nG1 E{} F{}", temp, len, std::stoi(speed) * 60));
    }

    if (btn == retract_btn.get_container()) {
      ws.gcode_script(fmt::format("M109 S{}\nM83\nG1 E-{} F{}", temp, len, std::stoi(speed) * 60));
    }

    if (btn == unload_btn.get_container()) {
      // Niente taglio: scalda, tira indietro di 100 mm e spegne. Il filamento si sfila a mano.
      ws.gcode_script(fmt::format("M109 S{}\nM83\nG1 E-100 F600\nM400\nM104 S0", temp));
    }

    if (btn == manual_change_btn.get_container()) {
      show_manual_filament_change();
    }

    if (btn == load_btn.get_container()) {
      const std::string load_script = fmt::format("_CARICA_BOBINA_ESTERNA TEMP={}", temp);
      if (KUtils::is_homed()) {
        ws.gcode_script(load_script);
      } else {
        // Il caricamento va al vassoio: serve l'home.
        powerui::confirm_dialog("Serve l'home",
                                "La stampante non ha fatto l'home. Faccio l'home e poi carico il filamento?",
                                "Home e carica", powerui::ActionKind::Primary, [this, load_script]() {
          ws.gcode_script("G28\n" + load_script);
        });
      }
    }

    if (btn == cooldown_btn.get_container()) {
      ws.gcode_script("SET_HEATER_TEMPERATURE HEATER=extruder TARGET=0");
    }
  }
}

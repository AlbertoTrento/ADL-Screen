#include "tmc_status_panel.h"
#include "powerui.h"
#include "spdlog/spdlog.h"

#include <algorithm>

namespace {

// Ordine delle schede: X, Y, Z, Estrusore, poi gli altri motori.
int motor_rank(const std::string &key) {
  const std::string name = key.substr(key.find(' ') + 1);
  if (name == "stepper_x") return 0;
  if (name == "stepper_y") return 1;
  if (name == "stepper_z") return 2;
  if (name == "extruder") return 3;
  return 10;
}

// Testo della scheda in italiano ("tmc2209 stepper_x" -> "Motore X").
std::string motor_tab(const std::string &key) {
  const std::string name = key.substr(key.find(' ') + 1);
  if (name == "stepper_x") return "Motore X";
  if (name == "stepper_y") return "Motore Y";
  if (name == "stepper_z") return "Motore Z";
  if (name == "extruder") return "Estrusore";
  return name;
}

}  // namespace

TmcStatusPanel::TmcStatusPanel(KWebSocketClient &c,
			      std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , top(lv_obj_create(cont))
  , toggle(lv_switch_create(top))
  , tabs(lv_btnmatrix_create(cont))
  , list(lv_obj_create(cont))
  , empty_label(NULL)
{
  using namespace powerui;
  lv_obj_move_background(cont);
  style_overlay_root(cont);
  const int gap = (int)(12 * powerui::overlay_width_scale());
  lv_obj_set_style_pad_all(cont, gap, 0);
  lv_obj_set_style_pad_row(cont, px(10), 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

  // In alto: cos'e' la pagina e l'interruttore che accende la lettura dei driver.
  lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(top, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(top, px(14), 0);
  lv_obj_set_style_border_width(top, 1, 0);
  lv_obj_set_style_border_color(top, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(top, LV_OPA_10, 0);
  lv_obj_set_style_pad_hor(top, px(16), 0);
  lv_obj_set_style_pad_ver(top, px(10), 0);
  lv_obj_set_style_pad_column(top, px(16), 0);
  lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *text = plain(top);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(text, px(2), 0);
  lv_obj_t *l = label(text, "Lettura dei driver in tempo reale", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  l = label(text, "Sperimentale: accendila solo quando serve.", &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);

  // L'interruttore va dopo il testo, a destra.
  lv_obj_move_foreground(toggle);
  style_switch(toggle);
  lv_obj_set_size(toggle, px(64), px(36));
  lv_obj_clear_state(toggle, LV_STATE_CHECKED);

  lv_obj_add_event_cb(toggle, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
      TmcStatusPanel *p = (TmcStatusPanel*)e->user_data;
      if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
	p->ws.gcode_script("_PS_LOAD_MODULE SECTION=tmcstatus");
      } else {
	p->ws.gcode_script("_PS_UNLOAD_MODULE SECTION=tmcstatus");
	// Prima la pagina restava piena e sembrava ancora accesa: ora si svuota subito.
	p->clear_metrics();
      }
    }
  }, LV_EVENT_VALUE_CHANGED, this);

  // Schede dei motori: una sola card alla volta, niente lista infinita.
  lv_obj_set_size(tabs, LV_PCT(100), px(52));
  style_segmented(tabs);
  lv_obj_set_style_text_font(tabs, &lv_font_montserrat_18, LV_PART_ITEMS);
  lv_obj_add_flag(tabs, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(tabs, [](lv_event_t *e) {
    TmcStatusPanel *p = (TmcStatusPanel*)e->user_data;
    uint16_t id = lv_btnmatrix_get_selected_btn(p->tabs);
    if (id != LV_BTNMATRIX_BTN_NONE && id < p->order.size()) {
      p->selected = p->order[id];
      p->show_selected();
    }
  }, LV_EVENT_VALUE_CHANGED, this);

  // Area scorrevole con la card del motore scelto.
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_radius(list, 0, 0);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_row(list, px(12), 0);

  empty_label = label(list, "Accendi la lettura con l'interruttore qui sopra: dopo qualche secondo "
		      "compaiono i dati di ogni motore.", &lv_font_montserrat_18, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(empty_label, LV_PCT(100));
  lv_label_set_long_mode(empty_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_pad_top(empty_label, px(24), 0);
  lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);

  ws.register_notify_update(this);
}

TmcStatusPanel::~TmcStatusPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void TmcStatusPanel::foreground() {
  lv_obj_move_foreground(cont);
  powerui::overlay_open("Stato driver TMC", [this]() { lv_obj_move_background(cont); });
}

void TmcStatusPanel::background() {
  lv_obj_move_background(cont);
}

void TmcStatusPanel::init(json &j) {
  auto tmc_status = j["/result/status/tmcstatus"_json_pointer];
  if (!tmc_status.is_null()) {
    if (!tmc_status.empty()) {
      lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }
    update_metrics(tmc_status);
  }

}

void TmcStatusPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto tmc_status = j["/params/0/tmcstatus"_json_pointer];
  // Con l'interruttore spento i dati che arrivano ancora non si mostrano.
  if (!tmc_status.is_null() && lv_obj_has_state(toggle, LV_STATE_CHECKED)) {
    update_metrics(tmc_status);
  }

}

void TmcStatusPanel::clear_metrics() {
  metrics.clear();  // le card si cancellano da sole (distruttore di TmcStatusContainer)
  order.clear();
  selected.clear();
  lv_obj_add_flag(tabs, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);
}

void TmcStatusPanel::update_metrics(json &tmc_status) {
  bool added = false;
  for (auto &el : tmc_status.items()) {
    const auto &s = metrics.find(el.key());
    if (s != metrics.end()) {
      s->second->update(el.value());
    } else {
      spdlog::debug("tmc stepper created {}", el.key());
      auto tmc_status_cont = std::make_shared<TmcStatusContainer>(ws, list, el.key());
      tmc_status_cont->set_visible(false);
      metrics.insert({el.key(), tmc_status_cont});
      tmc_status_cont->update(el.value());
      added = true;
    }
  }
  if (added) {
    rebuild_tabs();
  }
}

void TmcStatusPanel::rebuild_tabs() {
  order.clear();
  for (auto &m : metrics) {
    order.push_back(m.first);
  }
  // `metrics` e' gia' in ordine alfabetico: a parita' di posto resta quello.
  std::stable_sort(order.begin(), order.end(), [](const std::string &a, const std::string &b) {
    return motor_rank(a) < motor_rank(b);
  });

  tab_names.clear();
  for (auto &k : order) {
    tab_names.push_back(motor_tab(k));
  }
  tab_map.clear();
  for (auto &n : tab_names) {
    tab_map.push_back(n.c_str());
  }
  tab_map.push_back("");
  lv_btnmatrix_set_map(tabs, tab_map.data());
  lv_btnmatrix_set_btn_ctrl_all(tabs, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(tabs, true);

  if (selected.empty() || metrics.find(selected) == metrics.end()) {
    selected = order.front();
  }
  auto it = std::find(order.begin(), order.end(), selected);
  lv_btnmatrix_set_btn_ctrl(tabs, (uint16_t)(it - order.begin()), LV_BTNMATRIX_CTRL_CHECKED);

  lv_obj_clear_flag(tabs, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
  show_selected();
}

void TmcStatusPanel::show_selected() {
  for (auto &m : metrics) {
    m.second->set_visible(m.first == selected);
  }
  lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);
}

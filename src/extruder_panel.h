#ifndef __EXTRUDER_PANEL_H__
#define __EXTRUDER_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include <functional>
#include "selector.h"
#include "button_container.h"
#include "sensor_container.h"
#include "numpad.h"
#include "lvgl/lvgl.h"

#include <ctime>

class ExtruderPanel : public NotifyConsumer {
 public:
  ExtruderPanel(KWebSocketClient &ws, std::mutex &l, Numpad &np);
  ~ExtruderPanel();

  void foreground();
  void consume(json &j);
  void handle_callback(lv_event_t *e);
  void update_clock();
  void set_manual_change_opener(std::function<void()> opener) { manual_change_opener = opener; }

  static void _handle_callback(lv_event_t *event) {
    ExtruderPanel *panel = (ExtruderPanel*)event->user_data;
    panel->handle_callback(event);
  };

  static void _update_clock_cb(lv_timer_t *timer) {
    ExtruderPanel *panel = static_cast<ExtruderPanel *>(timer->user_data);
    panel->update_clock();
  }

 private:
  std::function<void()> manual_change_opener;
  KWebSocketClient &ws;
  lv_obj_t *panel_cont;
  lv_obj_t *title_bar;
  lv_obj_t *title_label;
  lv_obj_t *time_label;
  lv_timer_t *clock_timer;
  SensorContainer extruder_temp;
  Selector temp_selector;
  Selector length_selector;
  Selector speed_selector;
  lv_obj_t *rightside_btns_cont;
  lv_obj_t *leftside_btns_cont;
  ButtonContainer load_btn;
  ButtonContainer unload_btn;
  ButtonContainer cooldown_btn;
  ButtonContainer manual_change_btn;
  ButtonContainer extrude_btn;
  ButtonContainer retract_btn;

  // Temperatura dell'ugello nella scheda in alto: gradi e stato (si scalda, in temperatura, si raffredda, spento).
  lv_obj_t *temp_value_label = NULL;
  lv_obj_t *temp_state_label = NULL;
  int nozzle_temp = -1;
  int nozzle_target = 0;
  int cooldown_heater_on = -1;  // -1: colore del tasto Spegni ugello non ancora deciso

  // Togli filamento: mentre la stampante lavora il tasto e' bloccato e dice "In corso...".
  bool unloading = false;
  lv_timer_t *unload_guard_timer = NULL;
  void set_unloading(bool busy);
  static void _unload_guard_cb(lv_timer_t *timer) {
    ExtruderPanel *panel = static_cast<ExtruderPanel *>(timer->user_data);
    panel->unload_guard_timer = NULL;
    panel->set_unloading(false);
  }

  void show_manual_filament_change();
  void update_pause_button(const std::string &state);
  void update_nozzle_status();
};

#endif // __EXTRUDER_PANEL_H__

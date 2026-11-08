#ifndef __FAN_CONTROL_H__
#define __FAN_CONTROL_H__

#include "lvgl/lvgl.h"

// PowerUI card for one fan: icon, name, state, percentage, slider and Off / Max buttons.
class FanControl {
 public:
  // frame e blades: icona a colori della ventola (telaio fermo e pale che girano). Se mancano, icona piccola.
  FanControl(lv_obj_t *parent, const char *name, int y, int height, lv_event_cb_t cb, void *user_data,
             const lv_img_dsc_t *frame = NULL, const lv_img_dsc_t *blades = NULL);
  ~FanControl();

  lv_obj_t *get_container() { return cont; }
  lv_obj_t *get_label() { return name_label; }
  lv_obj_t *get_slider() { return slider; }
  lv_obj_t *get_off() { return off_btn; }
  lv_obj_t *get_max() { return max_btn; }
  void update_value(int value);
  // Fa avanzare le pale: chiamata a ogni scatto del timer della pagina Ventole (dt in millisecondi).
  void spin(uint32_t dt);

  static void _handle_value_changed(lv_event_t *event) {
    static_cast<FanControl *>(event->user_data)->update_value(lv_slider_get_value(lv_event_get_target(event)));
  }

 private:
  lv_obj_t *cont;
  lv_obj_t *icon_tile;
  lv_obj_t *icon_img;
  lv_obj_t *name_label;
  lv_obj_t *state_label;
  lv_obj_t *value_label;
  lv_obj_t *slider;
  lv_obj_t *off_btn;
  lv_obj_t *max_btn;
  lv_obj_t *frame_img = NULL;
  lv_obj_t *blades_img = NULL;
  int speed = 0;      // percentuale attuale
  int angle = 0;      // decimi di grado
};

#endif // __FAN_CONTROL_H__

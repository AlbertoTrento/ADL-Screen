#ifndef __TMC_STATUS_CONTAINER_H__
#define __TMC_STATUS_CONTAINER_H__

#include "websocket_client.h"
#include "lvgl/lvgl.h"
#include "hv/json.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;

// Un campo del driver regolabile al volo: nome, spiegazione, cursore grande, pulsanti - e + e valore.
// `cb` riceve il nuovo valore quando l'utente lo cambia (rilascio del cursore o del pulsante).
// Gli oggetti LVGL appartengono alla card che li contiene e vengono cancellati con lei.
class TmcFieldControl {
 public:
  TmcFieldControl(lv_obj_t *parent,
		  const char *title,
		  const char *help,
		  int min,
		  int max,
		  std::function<void(int)> cb);

  void update_value(int v);

 private:
  // Eventi dei pulsanti - (delta -1) e + (delta +1).
  void button_event(lv_event_code_t code, int delta);
  void step(int delta);
  void show_value();
  // Manda il valore del cursore solo se e' diverso dall'ultimo noto.
  void send_value();

  lv_obj_t *slider;
  lv_obj_t *value_label;
  int current = 0;          // ultimo valore letto dal driver o mandato
  bool repeating = false;   // pulsante tenuto premuto: il valore si manda al rilascio
  std::function<void(int)> cb;
};

class TmcStatusContainer {
 public:
  TmcStatusContainer(KWebSocketClient &c,
		     lv_obj_t *parent,
		     const std::string &s);
  ~TmcStatusContainer();

  void update(json &d);

  void update_tmc_value(const std::string &stepper_name,
			const std::string &field_name,
			int value);

  // Mostra o nasconde la card (la pagina ne mostra una alla volta).
  void set_visible(bool visible);

 private:
  // Legge correnti di marcia e di attesa dalla configurazione; false se non e' ancora disponibile.
  bool load_config_currents();

  KWebSocketClient &ws;
  std::string driver;        // es. "tmc2209 stepper_x"
  std::string driver_type;   // es. "tmc2209"
  lv_obj_t *cont;
  lv_obj_t *chart;
  lv_chart_series_t *sg_series;
  lv_chart_series_t *irms_series;
  lv_chart_series_t *semin_series;
  lv_chart_series_t *semax_series;

  lv_obj_t *status_badge;
  lv_obj_t *irms_label;      // corrente attuale
  lv_obj_t *irms_bar;
  lv_obj_t *sg_label;        // carico (StallGuard)
  lv_obj_t *sg_bar;
  lv_obj_t *temp_label;      // soglia di temperatura raggiunta dal driver
  lv_obj_t *run_label;       // corrente di marcia (configurazione)
  lv_obj_t *hold_label;      // corrente di attesa (configurazione)
  lv_obj_t *state_label;     // fermo / in movimento
  lv_obj_t *mode_label;      // silenzioso (StealthChop) / SpreadCycle
  lv_obj_t *no_error_label;
  std::vector<std::pair<std::string, lv_obj_t *>> error_labels;  // flag di drv_status -> riga di errore

  bool alert = false;
  bool config_loaded = false;
  int temp_level = -1;       // ultima soglia mostrata (evita di riscrivere l'etichetta)
  int moving = -1;
  int stealth = -1;
  int error_mask = -1;

  // Campi regolabili (semin, semax, seup, sedn, toff, tbl, hstrt, hend), per nome del campo TMC.
  std::map<std::string, std::unique_ptr<TmcFieldControl>> fields;
};

#endif // __TMC_STATUS_CONTAINER_H__

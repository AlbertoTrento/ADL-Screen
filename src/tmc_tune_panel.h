#ifndef __TMC_TUNE_PANEL__
#define __TMC_TUNE_PANEL__

#include "websocket_client.h"
#include "button_container.h"
#include "lvgl/lvgl.h"
#include "hv/iniparser.h"
#include "hv/json.hpp"

#include <list>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

using json = nlohmann::json;

// Card di un motore nella pagina "Regolazione driver TMC": modello del motore, obiettivo e,
// se il motore fa l'homing senza finecorsa, la sensibilita' dell'urto.
class AutoTmcContainer {
 public:
  AutoTmcContainer(const std::list<std::string> &motors,
		   const std::string &stepper_name,
		   int motor_idx,
		   int goal_idx,
		   bool has_sg,
		   int16_t sg4_thrs,
		   std::pair<int16_t, int16_t> sg_min_max,
		   lv_obj_t *parent);

  ~AutoTmcContainer();
  std::string get_config_macro();

 private:
  // Sposta il cursore della sensibilita' di `delta` e aggiorna il numero accanto.
  void change_threshold(int delta);
  void show_threshold();

  lv_obj_t *cont;
  std::string name;
  std::vector<std::string> motor_names;  // modelli del database, nello stesso ordine del menu (dopo "Non configurato")
  lv_obj_t *motors_dd;
  lv_obj_t *tuning_goal_dd;
  lv_obj_t *threshold_slider;
  lv_obj_t *threshold_value;
  int configured_motor_idx;
  int configured_goal_idx;
  bool has_sg;
  int16_t configured_sensorless_thrs;
  std::pair<int16_t, int16_t> sg_range;
};

class TmcTunePanel {
 public:
  TmcTunePanel(KWebSocketClient &c);
  ~TmcTunePanel();

  void init(json &j, fs::path &kp);
  void foreground();
  void background();

  void save_config();

 private:
  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *controls_cont;   // elenco scorrevole delle card dei motori
  lv_obj_t *btns_cont;       // barra in basso con il pulsante Salva e riavvia
  lv_obj_t *save_btn;
  IniParser motor_parser;

  std::map<std::string, int> motor_index;
  std::vector<std::shared_ptr<AutoTmcContainer>> steppers;

};

#endif // __TMC_TUNE_PANEL__

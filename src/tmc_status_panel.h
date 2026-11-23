#ifndef __TMC_STATUS_PANEL_H__
#define __TMC_STATUS_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "tmc_status_container.h"
#include "button_container.h"
#include "lvgl/lvgl.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class TmcStatusPanel : public NotifyConsumer {
 public:
  TmcStatusPanel(KWebSocketClient &ws,
		 std::mutex &lv_lock);
  ~TmcStatusPanel();

  void foreground();
  void background();

  void init(json &);
  void consume(json &j);

 private:
  // Crea o aggiorna le card dei motori con i dati di tmcstatus.
  void update_metrics(json &tmc_status);
  // Ricostruisce le schede in alto (un pulsante per motore, X, Y, Z, Estrusore, poi gli altri).
  void rebuild_tabs();
  // Mostra solo la card del motore scelto.
  void show_selected();

  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *top;
  lv_obj_t *toggle;
  lv_obj_t *tabs;          // pulsanti per scegliere il motore
  lv_obj_t *list;          // area scorrevole con la card del motore scelto
  lv_obj_t *empty_label;   // spiegazione quando non ci sono ancora dati
  std::map<std::string, std::shared_ptr<TmcStatusContainer>> metrics;
  std::vector<std::string> order;       // chiavi di `metrics` nell'ordine delle schede
  std::vector<std::string> tab_names;   // testi delle schede
  std::vector<const char *> tab_map;    // mappa per lv_btnmatrix (deve restare viva)
  std::string selected;                 // chiave del motore mostrato
};

#endif // __TMC_STATUS_PANEL_H__

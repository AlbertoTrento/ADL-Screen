#ifndef __SCREENSHOT_H__
#define __SCREENSHOT_H__

// Foto dello schermo, per le immagini del README.
// Quando e' accesa (interruttore in Sistema, spento a ogni avvio) compare un'area invisibile sopra l'orologio
// in alto a destra: tenendola premuta un secondo lo schermo salva quello che mostra in
// /usr/data/printer_data/logs/adl_screenshots (al massimo 40 foto, poi si ferma).
namespace screenshot {

void set_enabled(bool on);
bool enabled();

}  // namespace screenshot

#endif  // __SCREENSHOT_H__

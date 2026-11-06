#ifndef __I18N_H__
#define __I18N_H__

#include <string>

// Lingue del pannello. I testi non vengono cambiati nel codice: al link le funzioni LVGL che scrivono i testi passano
// da qui (vedi --wrap nel Makefile) e il testo viene sostituito con quello della lingua scelta (i18n/testi.tsv).
namespace i18n {

// Da chiamare una volta all'avvio, prima di creare le schermate. La lingua cambia al riavvio del pannello.
void init(const std::string &code);

}  // namespace i18n

#endif  // __I18N_H__

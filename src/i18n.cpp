#include "i18n.h"
#include "lvgl/lvgl.h"

#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct Row {
  const char *key;
  const char *text[5];  // it, en, de, fr, es
};

const Row TABLE[] = {
#include "i18n_table.inc"
};

const char *const CODES[5] = {"it", "en", "de", "fr", "es"};

std::unordered_map<std::string, const char *> exact;
std::vector<std::pair<std::string, const char *>> prefixes;  // chiavi che finiscono con uno spazio
std::vector<std::pair<std::string, const char *>> suffixes;  // chiavi che iniziano con uno spazio o con ":"

const char *find_exact(const char *text) {
  if (text == NULL) {
    return NULL;
  }
  auto it = exact.find(text);
  return it == exact.end() ? text : it->second;
}

// Il testo tradotto: intero, oppure solo l'inizio o la fine per i testi composti ("Slot A salvato").
std::string translate(const char *text) {
  if (text[0] == '\0' || std::isdigit(static_cast<unsigned char>(text[0]))) {
    return text;
  }
  auto it = exact.find(text);
  if (it != exact.end()) {
    return it->second;
  }
  std::string out = text;
  for (const auto &p : prefixes) {
    if (out.compare(0, p.first.size(), p.first) == 0) {
      out = p.second + out.substr(p.first.size());
      break;
    }
  }
  for (const auto &s : suffixes) {
    if (out.size() > s.first.size() && out.compare(out.size() - s.first.size(), s.first.size(), s.first) == 0) {
      out = out.substr(0, out.size() - s.first.size()) + s.second;
      break;
    }
  }
  return out;
}

}  // namespace

namespace i18n {

void init(const std::string &code) {
  int lang = 0;
  for (int i = 0; i < 5; ++i) {
    if (code == CODES[i]) {
      lang = i;
    }
  }
  for (const Row &row : TABLE) {
    const std::string key = row.key;
    const char *text = row.text[lang];
    exact[key] = text;
    if (key.size() > 2 && key.back() == ' ') {
      prefixes.emplace_back(key, text);
    } else if (key.size() > 2 && (key[0] == ' ' || key[0] == ':') && key[1] != ' ') {
      suffixes.emplace_back(key, text);
    }
  }
}

}  // namespace i18n

// Le funzioni vere di LVGL, raggiungibili con --wrap.
extern "C" {

void __real_lv_label_set_text(lv_obj_t *obj, const char *text);
void __real_lv_dropdown_set_options(lv_obj_t *obj, const char *options);
void __real_lv_textarea_set_placeholder_text(lv_obj_t *obj, const char *text);
void __real_lv_btnmatrix_set_map(lv_obj_t *obj, const char *map[]);

void __wrap_lv_label_set_text(lv_obj_t *obj, const char *text) {
  if (text == NULL) {
    __real_lv_label_set_text(obj, text);
    return;
  }
  const std::string out = translate(text);
  __real_lv_label_set_text(obj, out.c_str());
}

void __wrap_lv_textarea_set_placeholder_text(lv_obj_t *obj, const char *text) {
  if (text == NULL) {
    __real_lv_textarea_set_placeholder_text(obj, text);
    return;
  }
  const std::string out = translate(text);
  __real_lv_textarea_set_placeholder_text(obj, out.c_str());
}

void __wrap_lv_dropdown_set_options(lv_obj_t *obj, const char *options) {
  if (options == NULL) {
    __real_lv_dropdown_set_options(obj, options);
    return;
  }
  std::string out;
  const char *start = options;
  while (true) {
    const char *end = std::strchr(start, '\n');
    const std::string item = end == NULL ? std::string(start) : std::string(start, end - start);
    out += find_exact(item.c_str());
    if (end == NULL) {
      break;
    }
    out += '\n';
    start = end + 1;
  }
  __real_lv_dropdown_set_options(obj, out.c_str());
}

void __wrap_lv_btnmatrix_set_map(lv_obj_t *obj, const char *map[]) {
  if (map == NULL) {
    __real_lv_btnmatrix_set_map(obj, map);
    return;
  }
  // LVGL tiene il puntatore alla mappa: la copia tradotta deve restare viva, una per ogni mappa.
  static std::map<const char **, std::vector<const char *>> copies;
  std::vector<const char *> &copy = copies[map];
  copy.clear();
  for (int i = 0;; ++i) {
    // Una mappa finisce con "" oppure con NULL.
    copy.push_back(find_exact(map[i]));
    if (map[i] == NULL || map[i][0] == '\0') {
      break;
    }
  }
  __real_lv_btnmatrix_set_map(obj, copy.data());
}

}  // extern "C"

#include "screenshot.h"
#include "powerui.h"
#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#if !defined(SIMULATOR)
#include <linux/fb.h>
#include <sys/ioctl.h>
#endif

namespace screenshot {
namespace {

const char *FOLDER = "/usr/data/printer_data/logs/adl_screenshots";
constexpr int MAX_PHOTOS = 40;

bool is_on = false;
lv_obj_t *hotspot = NULL;
lv_obj_t *toast = NULL;
lv_timer_t *toast_timer = NULL;

// Quante foto ci sono gia' nella cartella (file foto_NN.fb).
int count_photos() {
  int count = 0;
  DIR *dir = opendir(FOLDER);
  if (dir == NULL) {
    return 0;
  }
  while (struct dirent *entry = readdir(dir)) {
    const std::string name = entry->d_name;
    if (name.rfind("foto_", 0) == 0 && name.size() > 3 && name.compare(name.size() - 3, 3, ".fb") == 0) {
      ++count;
    }
  }
  closedir(dir);
  return count;
}

void show_toast(const std::string &text) {
  if (toast == NULL) {
    toast = powerui::label(lv_layer_sys(), "", &lv_font_montserrat_20, lv_color_hex(powerui::COLOR_WHITE));
    lv_obj_set_style_bg_color(toast, lv_color_hex(powerui::COLOR_CARD), 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(toast, 2, 0);
    lv_obj_set_style_border_color(toast, lv_color_hex(powerui::COLOR_ACCENT), 0);
    lv_obj_set_style_radius(toast, powerui::px(12), 0);
    lv_obj_set_style_pad_hor(toast, powerui::px(20), 0);
    lv_obj_set_style_pad_ver(toast, powerui::px(12), 0);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_label_set_text(toast, text.c_str());
  lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -powerui::px(20));
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
  if (toast_timer != NULL) {
    lv_timer_del(toast_timer);
  }
  toast_timer = lv_timer_create([](lv_timer_t *) {
    if (toast != NULL) {
      lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_del(toast_timer);
    toast_timer = NULL;
  }, 1800, NULL);
}

// Copia quello che c'e' sullo schermo (framebuffer) in un file. In testa una riga con le misure:
// "ADLFB larghezza altezza bit_per_pixel byte_per_riga\n", poi i pixel cosi' come sono.
bool save_framebuffer(const std::string &path) {
#if defined(SIMULATOR)
  (void)path;
  return false;
#else
  int fb = open("/dev/fb0", O_RDONLY);
  if (fb < 0) {
    return false;
  }
  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;
  if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) != 0 || ioctl(fb, FBIOGET_FSCREENINFO, &finfo) != 0) {
    close(fb);
    return false;
  }
  const size_t size = static_cast<size_t>(finfo.line_length) * vinfo.yres;
  std::vector<char> pixels(size);
  const off_t start = static_cast<off_t>(vinfo.yoffset) * finfo.line_length;
  size_t done = 0;
  if (lseek(fb, start, SEEK_SET) == start) {
    while (done < size) {
      const ssize_t n = read(fb, pixels.data() + done, size - done);
      if (n <= 0) {
        break;
      }
      done += static_cast<size_t>(n);
    }
  }
  close(fb);
  if (done != size) {
    return false;
  }
  FILE *out = fopen(path.c_str(), "wb");
  if (out == NULL) {
    return false;
  }
  fprintf(out, "ADLFB %u %u %u %u\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length);
  const bool ok = fwrite(pixels.data(), 1, size, out) == size;
  fclose(out);
  return ok;
#endif
}

void take() {
  mkdir("/usr/data/printer_data/logs", 0755);
  mkdir(FOLDER, 0755);
  const int count = count_photos();
  if (count >= MAX_PHOTOS) {
    show_toast("Memoria foto piena (40)");
    return;
  }
  char name[64];
  // Primo numero libero: le foto non si sovrascrivono.
  int number = count + 1;
  std::string path;
  for (;; ++number) {
    snprintf(name, sizeof(name), "%s/foto_%02d.fb", FOLDER, number);
    path = name;
    if (access(path.c_str(), F_OK) != 0) {
      break;
    }
  }
  if (save_framebuffer(path)) {
    spdlog::info("foto dello schermo salvata: {}", path);
    show_toast("Foto " + std::to_string(number) + " salvata");
  } else {
    spdlog::warn("foto dello schermo non riuscita: {}", path);
    show_toast("Foto non riuscita");
  }
}

}  // namespace

void set_enabled(bool on) {
  is_on = on;
  if (on && hotspot == NULL) {
    // Area invisibile sopra l'orologio, sul livello piu' alto: funziona su tutte le pagine e le finestre.
    hotspot = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(hotspot);
    lv_obj_set_size(hotspot, powerui::px(90), powerui::px(44));
    lv_obj_align(hotspot, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(hotspot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hotspot, [](lv_event_t *) { take(); }, LV_EVENT_LONG_PRESSED, NULL);
  } else if (!on && hotspot != NULL) {
    lv_obj_del(hotspot);
    hotspot = NULL;
  }
}

bool enabled() {
  return is_on;
}

}  // namespace screenshot

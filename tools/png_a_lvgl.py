"""Converte un PNG con trasparenza in un'immagine LVGL (TRUE_COLOR_ALPHA, BGRA a 32 bit).

Uso: python tools/png_a_lvgl.py file.png nome_variabile lato_pixel cartella_uscita "commento"
Serve Pillow. Il file .c finisce in cartella_uscita/nome_variabile.c ed e' compilato dal Makefile
(assets/powerui/*.c).
"""
import os
import sys

from PIL import Image


def converti(png, nome, lato, uscita, commento):
    im = Image.open(png).convert('RGBA').resize((lato, lato), Image.LANCZOS)
    dati = []
    for r, g, b, a in im.getdata():
        dati += [b, g, r, a]
    righe = []
    for i in range(0, len(dati), 24):
        righe.append('  ' + ', '.join('0x%02x' % v for v in dati[i:i + 24]))
    testo = f'''/* {commento} */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#if LV_COLOR_DEPTH != 32
#error "Le immagini sono generate per LV_COLOR_DEPTH 32"
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {nome}_map[] = {{
{(','+chr(10)).join(righe)}
}};

const lv_img_dsc_t {nome} = {{
  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = {lato},
  .header.h = {lato},
  .data_size = {lato * lato} * LV_IMG_PX_SIZE_ALPHA_BYTE,
  .data = {nome}_map,
}};
'''
    with open(os.path.join(uscita, nome + '.c'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(testo)


if __name__ == '__main__':
    converti(sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5])

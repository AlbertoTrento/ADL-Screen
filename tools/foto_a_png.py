"""Trasforma le foto dello schermo (file foto_NN.fb salvati da ADL Screen) in PNG.

Uso: python tools/foto_a_png.py cartella_con_i_file_fb cartella_uscita [gradi_rotazione]
Ogni file comincia con la riga "ADLFB larghezza altezza bit_per_pixel byte_per_riga" seguita dai pixel
del framebuffer cosi' come sono (32 bit BGRA oppure 16 bit RGB565). Serve Pillow.
"""
import os
import sys

from PIL import Image


def converti(percorso, uscita, rotazione=0):
    with open(percorso, 'rb') as f:
        dati = f.read()
    fine_riga = dati.index(b'\n')
    _, w, h, bpp, riga = dati[:fine_riga].decode('ascii').split()
    w, h, bpp, riga = int(w), int(h), int(bpp), int(riga)
    pixel = dati[fine_riga + 1:]
    if bpp == 32:
        img = Image.frombuffer('RGBA', (w, h), pixel, 'raw', 'BGRA', riga, 1).convert('RGB')
    elif bpp == 16:
        img = Image.frombuffer('RGB', (w, h), pixel, 'raw', 'BGR;16', riga, 1)
    else:
        raise ValueError(f'{percorso}: {bpp} bit per pixel non gestiti')
    if rotazione:
        img = img.rotate(rotazione, expand=True)
    img.save(uscita)
    return img.size


if __name__ == '__main__':
    sorgente, destinazione = sys.argv[1], sys.argv[2]
    rot = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    os.makedirs(destinazione, exist_ok=True)
    for nome in sorted(os.listdir(sorgente)):
        if nome.endswith('.fb'):
            dim = converti(os.path.join(sorgente, nome), os.path.join(destinazione, nome[:-3] + '.png'), rot)
            print(nome, '->', dim)

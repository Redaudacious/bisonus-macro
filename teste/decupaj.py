#!/usr/bin/env python3
"""decupaj.py <intrare.png> <iesire.png> <x y lățime înălțime>… — taie poza pe cutia care cuprinde ferestrele date
și pune o culoare plată peste tot ce nu e fereastră: imaginea de fundal, Sober, panoul Plasma, orice altă fereastră
dintre ele. Colțurile rotunjite ale ferestrelor intră în culoarea plată. Ferestrele se dau cu locul lor de la KWin,
cu tot cu ramă."""
import sys
from PIL import Image, ImageDraw

CULOARE = (48, 48, 48)
RAZA = 12  # peste raza colțurilor lui Breeze: un colț mai rotund lasă un pic de fereastră deoparte, unul mai strâmt lasă fundalul să treacă


def decupaj(intrare, iesire, v):
    r = [(v[i], v[i + 1], v[i] + v[i + 2], v[i + 1] + v[i + 3]) for i in range(0, len(v), 4)]
    cutie = (min(a[0] for a in r), min(a[1] for a in r), max(a[2] for a in r), max(a[3] for a in r))
    poza = Image.open(intrare).convert("RGB").crop(cutie)
    masca = Image.new("L", poza.size, 0)
    d = ImageDraw.Draw(masca)
    for x0, y0, x1, y1 in r:
        d.rounded_rectangle((x0 - cutie[0], y0 - cutie[1], x1 - cutie[0] - 1, y1 - cutie[1] - 1), RAZA, fill=255)
    Image.composite(poza, Image.new("RGB", poza.size, CULOARE), masca).save(iesire)


if __name__ == "__main__":
    a = sys.argv[3:]
    if len(a) == 0 or len(a) % 4:
        sys.exit("folosește: decupaj.py intrare.png iesire.png x y lățime înălțime [x y lățime înălțime]…")
    decupaj(sys.argv[1], sys.argv[2], [round(float(x)) for x in a])

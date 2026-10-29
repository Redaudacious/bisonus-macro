#!/bin/bash
# proba-decupaj.sh — `decupaj.py` pune culoarea plată peste tot ce nu e fereastră și lasă ferestrele neatinse.
# Nu cere desktopul: poza e făcută aici, cu zgomot, ca niciun pixel de fundal să nu semene cu culoarea plată.
set -u
cd "$(dirname "$0")/.."
D=$(mktemp -d)
trap 'rm -rf "$D"' EXIT
python3 -I - "$D" <<'PY' || { echo "✗ decupaj.py"; exit 1; }
import os, random, subprocess, sys
from PIL import Image
d = sys.argv[1]
random.seed(1)
poza = Image.new("RGB", (300, 200))
poza.putdata([(random.randrange(60, 256), random.randrange(60, 256), random.randrange(60, 256)) for _ in range(300 * 200)])
poza.save(d + "/in.png")
# două ferestre cu un gol între ele, cu valori zecimale ca la KWin
subprocess.run([sys.executable, "-I", "teste/decupaj.py", d + "/in.png", d + "/out.png", "20", "30.4", "100", "80", "200", "60", "80", "120"], check=True)
iesire = Image.open(d + "/out.png")
assert iesire.size == (260, 150), iesire.size  # cutia: x 20..280, y 30..180
def inauntru(x, y):  # x, y în poza întreagă
    return (20 <= x < 120 and 30 <= y < 110) or (200 <= x < 280 and 60 <= y < 180)
plat = ferest = 0
for y in range(150):
    for x in range(260):
        p, o = poza.getpixel((x + 20, y + 30)), iesire.getpixel((x, y))
        if not inauntru(x + 20, y + 30):
            assert o == (48, 48, 48), ("fundal rămas", x + 20, y + 30, o)
            plat += 1
        elif o == p:
            ferest += 1
assert ferest > 100 * 80 * 0.9 + 80 * 120 * 0.9, ferest  # mijlocul ferestrelor rămâne neatins
print("✓ decupaj.py: %d pixeli plați în afara ferestrelor, %d ai ferestrelor păstrați" % (plat, ferest))
PY

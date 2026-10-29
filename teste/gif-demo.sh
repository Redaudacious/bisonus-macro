#!/bin/bash
# gif-demo.sh <1|2|3> — cele trei GIF-uri din README și din docs/README.md, făcute din poze ale ecranului:
#   1 → docs/media/1-panel.gif: clic pe rândul „Snow 2”, iar fereastra de probă primește „4”, „/Snow 2” și Enter. CERE EIS.
#   2 → docs/media/2-editor.gif: fereastra codului primește un `while` în `block`, arată greșeala, apoi greșeala dispare.
#       Nu cere EIS: aplicația pornește după cele 120 s ale ferestrei de acord a lui KDE, iar scriptul mută fereastra de
#       acord din cadru. Nimeni n-o acceptă.
#   3 → docs/media/3-stop.gif: F7 pornește un macro fără capăt, apoi „■ Stop” îl oprește. CERE EIS.
# Scenele 1 și 3 n-au fost rulate: tokenul de acord n-a putut fi dat, iar tastatura EIS nu pornește fără el.
# Pozele vin de la `spectacle -b -n -f`, cam una la 0,5 s. `teste/decupaj.py` le taie pe locul ferestrelor și
# pune culoare plată peste tot ce nu e fereastră, iar `ffmpeg` le lipește cu
# `palettegen` și `paletteuse`. Aplicația pornește sub componenta de probă `gif-demo`, golită la ieșire, ca
# `kglobalshortcutsrc` al omului să rămână neatins. Cere sesiunea KDE pe Wayland, ecranul aprins și ydotoold.
# Omul nu atinge mouse-ul și tastatura cam 3 minute (2 minute de așteptare pentru scena 2).
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
N=${1:-}
case $N in
    1) NUME=1-panel ;;
    2) NUME=2-editor ;;
    3) NUME=3-stop ;;
    *) echo "✗ folosește: gif-demo.sh 1|2|3"; exit 1 ;;
esac
D=build/gif-demo
OUT=docs/media
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
KG="busctl --user call org.kde.kglobalaccel"
REGULI="$HOME/.config/kwinrulesrc"
APP=""
TINTA=""
FILM=""

liber_sau_iesi "filmării"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: scriptul face clicurile și scrie cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ scriptul cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }

goleste() {
    local nume=$1 cale=/component/${1//-/_}
    busctl --user --json=short call org.kde.kglobalaccel "$cale" org.kde.kglobalaccel.Component shortcutNames 2>/dev/null \
        | python3 -I -c 'import sys, json; [print(a) for a in json.load(sys.stdin)["data"][0]]' 2>/dev/null \
        | while IFS= read -r a; do $KG /kglobalaccel org.kde.KGlobalAccel unregister ss "$nume" "$a" >/dev/null 2>&1; done
    $KG "$cale" org.kde.kglobalaccel.Component cleanUp >/dev/null 2>&1
    true
}

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro" "$D/cadre" "$D/taiate" "$OUT"
SETARI="$PWD/$D/config/bisonus-macrorc"
case $N in
    1)
        cat > "$SETARI" <<'EOT'
[General]
versiuneVazuta=99.0.0
latimePanou=420
deasupra=true
ferestre=fereastra-tinta
tinta=
transparent=false
EOT
        cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": {},
  "arbore": [ { "categorie": "Rift", "id": "rift", "copii": [
    { "macro": "Snow 2", "id": "rift-snow-2", "script": "keydelay(150)\nkey(\"4\")\nwait(2000)\nblock {\n    key(\"/\")\n    text(\"Snow 2\")\n    key(\"Return\")\n}\n" } ] } ] }
EOT
        ;;
    2)
        cat > "$SETARI" <<'EOT'
[General]
versiuneVazuta=99.0.0
deasupra=true
ferestre=
transparent=false
EOT
        # Fără al doilea macro, categoria ar lipsi din panou, ca în capturi-ajutor.sh
        cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{"arbore":[{"categorie":"Demo","id":"demo","activ":true,"copii":[{"macro":"Demo","id":"demo-1","activ":true,"script":"key(\"4\")\n"},{"macro":"Bun","id":"demo-2","activ":true,"script":"key(\"4\")\n"}]}],"sabloane":{}}
EOT
        ;;
    3)
        cat > "$SETARI" <<'EOT'
[General]
versiuneVazuta=99.0.0
latimePanou=420
deasupra=true
ferestre=fereastra-tinta
tinta=
transparent=false
tastaOprire=F9

[Nod][bucla]
tasta=F7
EOT
        cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": {},
  "arbore": [ { "categorie": "Demo", "id": "demo", "copii": [
    { "macro": "Loop", "id": "bucla", "script": "loop {\n    key(\"a\")\n    wait(150)\n}\n" } ] } ] }
EOT
        ;;
esac
[ "$N" != 2 ] && [ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$SETARI"

DEV=""
for s in $(qdbus6 org.kde.KWin /org/kde/KWin/InputDevice org.kde.KWin.InputDeviceManager.devicesSysNames); do
    [ "$(busctl --user get-property org.kde.KWin "/org/kde/KWin/InputDevice/$s" org.kde.KWin.InputDevice name)" = 's "ydotoold virtual device"' ] && DEV=$s
done
[ -n "$DEV" ] || { echo "✗ KWin nu vede dispozitivul ydotoold"; exit 1; }
CALE=/org/kde/KWin/InputDevice/$DEV
proprietate() { busctl --user get-property org.kde.KWin "$CALE" org.kde.KWin.InputDevice "$1" | cut -d' ' -f2; }
seteaza() { busctl --user set-property org.kde.KWin "$CALE" org.kde.KWin.InputDevice "$1" b "$2"; }
PLAT=$(proprietate pointerAccelerationProfileFlat)
ADAPTIV=$(proprietate pointerAccelerationProfileAdaptive)
LOC=$(kreadconfig6 --file "$REGULI" --group bisonus-macro --key position)
curata() {
    rm -f "$D/filmeaza"
    [ "$PLAT" = true ] && seteaza pointerAccelerationProfileFlat true
    [ "$ADAPTIV" = true ] && seteaza pointerAccelerationProfileAdaptive true
    [ -n "$FILM" ] && kill "$FILM" 2>/dev/null
    [ -n "$APP" ] && kill "$APP" 2>/dev/null
    [ -n "$TINTA" ] && kill "$TINTA" 2>/dev/null
    wait 2>/dev/null
    scripting unloadScript bisonus-macro >/dev/null 2>&1
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    goleste gif-demo
    sleep 0.5
    if [ -n "$LOC" ]; then
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position "$LOC"
    else
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position --delete
    fi
    qdbus6 org.kde.KWin /KWin reconfigure
    true
}
trap curata EXIT
goleste gif-demo
seteaza pointerAccelerationProfileFlat true
kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position 40,40
qdbus6 org.kde.KWin /KWin reconfigure
kscreen-doctor --dpms on >/dev/null 2>&1; sleep 1.5

build/fereastra-tinta -1 fereastra-tinta 600000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
grep -qx activ "$D/tinta.log" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }
BISONUS_COMPONENTA=gif-demo LANGUAGE=en XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
if [ "$N" = 2 ]; then
    # Fără EIS, panoul apare după cele 120 s ale ferestrei de acord. Un mic pas al mouse-ului ține ecranul treaz, iar
    # fereastra de acord (Remote Control) se mută în colțul din dreapta jos, în afara cadrului.
    sleep 8
    js 'for (const w of workspace.windowList()) if (w.caption === "Remote Control") w.frameGeometry = {x: workspace.virtualScreenSize.width - w.width, y: workspace.virtualScreenSize.height - w.height - 60, width: w.width, height: w.height};'
    for _ in $(seq $(((ASTEPTARE + 30) / 5))); do
        grep -q '^urmărire: gata' "$D/aplicatia.log" && break
        ydotool mousemove -x 2 -y 0; ydotool mousemove -x -2 -y 0
        sleep 5
    done
    grep -q '^urmărire: gata' "$D/aplicatia.log" || { echo "✗ panoul n-a apărut: $D/aplicatia.log"; exit 1; }
    sleep 2
else
    [ -s "$ACORD" ] || { echo "✗ lipsește $ACORD: scenele 1 și 3 cer tastatura EIS, deci un acord dat o dată de om"; exit 1; }
    for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
    grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
fi
sleep 1.5

du() { ydotool mousemove --absolute -x 0 -y 0; sleep 0.5; ydotool mousemove -x "$1" -y "$2"; sleep 0.5; }
read -r PX PY <<< "$(panou_colt)"
read -r _ _ CW _ <<< "$(panou_cheie "$GEOM_CONTINUT")"
[ -n "${PY:-}" ] && [ -n "${CW:-}" ] || { echo "✗ KWin nu dă locul panoului"; exit 1; }

# filma: în fundal, o poză a ecranului după alta cât există $D/filmeaza
filma() {
    local i=0
    while [ -e "$D/filmeaza" ]; do
        i=$((i + 1))
        spectacle -b -n -f -o "$PWD/$D/cadre/cadru-$(printf %03d $i).png" >/dev/null 2>&1
    done
}
incepe_filmul() { touch "$D/filmeaza"; filma & FILM=$!; }
opreste_filmul() { rm -f "$D/filmeaza"; wait "$FILM" 2>/dev/null; FILM=""; }

case $N in
1)
    # Rândul lui Snow 2 stă sub bara de 38 px și titlul de 24 px, iar centrul lui e la 20 px de sus. Apoi cursorul pleacă
    # de pe panou, în fereastra de probă, ca macro-ul să pornească.
    du $((PX + 100)) $((PY + 38 + 24 + 20))
    incepe_filmul
    sleep 1.5
    ydotool click 0xC0
    sleep 1
    ydotool mousemove -x 600 -y 100
    sleep 7
    opreste_filmul
    ;;
2)
    du $((PX + CW - 16)) $((PY + 38 + 24 + 20))
    ydotool click 0xC0
    sleep 2
    EDITOR=$(uuid bisonus-macro 'Demo — code — BisonusMacro')
    # uuid tipărește ultimul uuid cerut, chiar dacă fereastra nu există: se verifică titlul ei
    [[ "$(info "$EDITOR" caption)" == *"— code —"* ]] || { echo "✗ fereastra codului n-a apărut"; exit 1; }
    # Mesajul lung al greșelii lărgește fereastra la mijlocul scenei, iar tăietura e fixă: fereastra primește dinainte
    # mărimea largă
    js 'for (const w of workspace.windowList()) if (w.resourceClass === "bisonus-macro" && w.caption === "Demo — code — BisonusMacro") w.frameGeometry = {x: w.frameGeometry.x, y: w.frameGeometry.y, width: 940, height: 380};'
    sleep 1.5
    # KWin dă mărimile cu zecimale: se rotunjesc
    EX=$(printf %.0f "$(info "$EDITOR" x)"); EY=$(printf %.0f "$(info "$EDITOR" y)")
    EW=$(printf %.0f "$(info "$EDITOR" width)"); EH=$(printf %.0f "$(info "$EDITOR" height)")
    echo "$EX $EY $EW $EH" > "$D/loc.txt"
    # clic în text, apoi Ctrl+End: cursorul stă pe rândul gol de după `key("4")`
    du $((EX + EW / 2)) $((EY + EH / 3))
    ydotool click 0xC0
    ydotool key 29:1 107:1 107:0 29:0
    ydotool mousemove --absolute -x $((EX + EW + 40)) -y $((EY + EH + 40)) # cursorul iese din cadru
    incepe_filmul
    sleep 1.5
    ydotool type -d 90 $'block {\n    while 1 {\n        key("a")\n    }\n}\n'
    sleep 3
    # 7 rânduri, cursorul pe al 7-lea. Două rânduri sus, la începutul rândului `    }`: se selectează și se șterge
    ydotool key 103:1 103:0 103:1 103:0
    sleep 0.5
    ydotool key 102:1 102:0
    ydotool key 42:1 108:1 108:0 42:0
    sleep 1
    ydotool key 111:1 111:0
    sleep 1.5
    # acum `}` e pe rândul 5. Două rânduri sus, la rândul `    while 1 {`
    ydotool key 103:1 103:0 103:1 103:0
    sleep 0.5
    ydotool key 102:1 102:0
    ydotool key 42:1 108:1 108:0 42:0
    sleep 1
    ydotool key 111:1 111:0
    sleep 3
    opreste_filmul
    ;;
3)
    du $((PX + 400)) $((PY + 14))
    ydotool mousemove -x 0 -y 0
    incepe_filmul
    sleep 1.5
    ydotool key 65:1 65:0 # F7
    sleep 4
    ydotool mousemove -x 0 -y 0
    du $((PX + 400)) $((PY + 14))
    ydotool click 0xC0
    sleep 3
    opreste_filmul
    ;;
esac

# Locul tăieturii: fereastra codului la scena 2, panoul și fereastra de probă la celelalte
if [ "$N" = 2 ]; then
    read -r X0 Y0 W0 H0 < "$D/loc.txt"
    TAIE="$X0 $Y0 $W0 $H0"
else
    read -r FX FY FW FH <<< "$(panou_cheie "$GEOM_RAMA")"
    read -r TX TY TW TH <<< "$(intreaba tinta "$PANOU_JS const t = workspace.windowList().find(w => w.resourceClass === \"fereastra-tinta\"); raport(\"tinta \" + (t ? [t.frameGeometry.x, t.frameGeometry.y, t.frameGeometry.width, t.frameGeometry.height].map(Math.round).join(\" \") : \"0 0 0 0\"));")"
    TAIE="$FX $FY $FW $FH $TX $TY $TW $TH"
fi
# Fiecare poză se taie pe cutia ferestrelor, iar tot ce nu e fereastră (fundalul, Sober, panoul Plasma) devine culoare plată
i=0
for f in "$D"/cadre/cadru-*.png; do
    i=$((i + 1))
    python3 -I teste/decupaj.py "$f" "$D/taiate/cadru-$(printf %03d $i).png" $TAIE || { echo "✗ decupaj.py a picat pe $f"; exit 1; }
done
ls "$D/taiate" | grep -q . || { echo "✗ nicio poză tăiată"; exit 1; }
ffmpeg -y -loglevel error -framerate 2.4 -i "$D/taiate/cadru-%03d.png" \
    -vf "fps=2.4,scale=800:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse" "$OUT/$NUME.gif" \
    || { echo "✗ ffmpeg n-a făcut $OUT/$NUME.gif"; exit 1; }
echo "✓ $OUT/$NUME.gif, $(ls "$D/taiate" | wc -l) cadre, $(stat -c%s "$OUT/$NUME.gif") octeți"

#!/bin/bash
# capturi-ajutor.sh [tot|noi] — capturile paginilor de help, din aplicația de pe ramură, cu arborele adus de pachet, în
# engleză: date/help/capturi/panou.png, fereastra-tastei.png și fereastra-principala.png, apoi pagina-goala.png (fereastra
# principală cu arborele gol) și editorul.png (fereastra codului unui macro cu o greșeală). `noi` face numai ultimele două
# și nu cere tastatura EIS: aplicația pornește după cele 120 s ale ferestrei de acord a lui KDE, cu macro-urile gri, iar
# scriptul mută din calea capturilor fereastra de acord rămasă pe ecran. `tot` cere tastatura EIS. Categoriile Rift sunt strânse,
# afară de Durana, ca lista să încapă fără bară de derulare. O schimbare a panoului sau a ferestrei principale își reface
# așa capturile. Cere sesiunea KDE pe Wayland, ydotoold și mouse-ul neatins cam 25 de secunde. Înaintea fiecărei capturi,
# cursorul pleacă la marginea din dreapta a ecranului, ca semnul de pornire al KDE să nu intre în ea. Scriptul încarcă
# scripturi în KWin-ul omului și le descarcă la ieșire. Panoul din capturi are fundalul opac, iar ce nu e fereastră devine culoare
# plată: nimic din desktopul omului nu rămâne în ele.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/capturi
OUT=date/help/capturi
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
MODO=${1:-tot}
[ "$MODO" = tot ] || [ "$MODO" = noi ] || { echo "✗ folosește: capturi-ajutor.sh [tot|noi]"; exit 1; }
KG="busctl --user call org.kde.kglobalaccel"
REGULI="$HOME/.config/kwinrulesrc"
APP=""
TINTA=""

liber_sau_iesi "capturilor"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: scriptul face clicul pe ⚙ cu ydotool"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }

# Aplicația ține tastele sub componenta de probă `capturi-ajutor`, ca kglobalshortcutsrc al omului să rămână neatins
goleste() {
    local nume=$1 cale=/component/${1//-/_}
    busctl --user --json=short call org.kde.kglobalaccel "$cale" org.kde.kglobalaccel.Component shortcutNames 2>/dev/null \
        | python3 -c 'import sys, json; [print(a) for a in json.load(sys.stdin)["data"][0]]' 2>/dev/null \
        | while IFS= read -r a; do $KG /kglobalaccel org.kde.KGlobalAccel unregister ss "$nume" "$a" >/dev/null 2>&1; done
    $KG "$cale" org.kde.kglobalaccel.Component cleanUp >/dev/null 2>&1
    true
}

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro" "$OUT"
python3 -I -c 'import json, sys; json.dump(json.load(open(sys.argv[1], encoding="utf-8"))["macros"], open(sys.argv[2], "w", encoding="utf-8"), ensure_ascii=False, indent=2)' date/templates/bloodlines.json "$D/config/bisonus-macro/macros.json"
# Arborele editorului: Demo are în bloc un `while`, care nu e voie, și stă lângă un macro bun. Fără macro bun, categoria
# ar lipsi din panou: o categorie fără macro care lucrează nu se arată. `noi` pornește cu el, `tot` îl pune mai târziu.
ARBORE_DEMO='{"arbore":[{"categorie":"Demo","id":"demo","activ":true,"copii":[{"macro":"Demo","id":"demo-1","activ":true,"script":"key(\"4\")\nwait(2000)\nblock {\n    while 1 {\n    }\n}\n"},{"macro":"Bun","id":"demo-2","activ":true,"script":"key(\"4\")\n"}]}],"sabloane":{}}'
[ "$MODO" = noi ] && printf '%s\n' "$ARBORE_DEMO" > "$D/config/bisonus-macro/macros.json"
cat > "$D/config/bisonus-macrorc" <<'EOT'
[General]
versiuneVazuta=99.0.0
deasupra=true
ferestre=
transparent=false

[Nod][rift-sorythia]
strans=true

[Nod][rift-expanse]
strans=true

[Nod][rift-training]
strans=true

[Nod][rift-snow]
strans=true

[Nod][rift-rain]
strans=true

[Nod][rift-mello]
strans=true

[Nod][rift-rose]
strans=true

[Nod][rift-chakra]
strans=true
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$D/config/bisonus-macrorc"

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
    [ "$PLAT" = true ] && seteaza pointerAccelerationProfileFlat true
    [ "$ADAPTIV" = true ] && seteaza pointerAccelerationProfileAdaptive true
    [ -n "$APP" ] && kill "$APP" 2>/dev/null
    [ -n "$TINTA" ] && kill "$TINTA" 2>/dev/null
    wait 2>/dev/null
    scripting unloadScript bisonus-macro >/dev/null 2>&1
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    goleste capturi-ajutor
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
seteaza pointerAccelerationProfileFlat true
kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position 40,40
qdbus6 org.kde.KWin /KWin reconfigure
sleep 1

TITLU_SETARI='Macro settings — BisonusMacro'
# panou_uuid: uuid-ul panoului, în oricare titlu
panou_uuid() {
    local u
    u=$(uuid bisonus-macro "$TITLU_DOI")
    [ -n "$u" ] || u=$(uuid bisonus-macro BisonusMacro)
    echo "$u"
}

# Ținta trăiește mai mult decât cele 120 s de așteptare, altfel numele win.bisonus.proba pleacă înaintea întrebărilor
build/fereastra-tinta -1 fereastra-tinta 600000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
BISONUS_COMPONENTA=capturi-ajutor LANGUAGE=en XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
if [ "$MODO" = tot ]; then
    for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
    grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS: $D/aplicatia.log"; exit 1; }
else
    # Fără EIS, aplicația arată panoul după cele 120 s ale ferestrei de acord. Cât așteaptă, un mic pas al mouse-ului ține
    # ecranul treaz. Fereastra de acord (Remote Control) trece o dată în colțul din dreapta jos, sub orice captură. Nu o
    # acceptă nimeni: ea rămâne pe ecran până se închide aplicația.
    sleep 8
    js 'for (const w of workspace.windowList()) if (w.caption === "Remote Control") w.frameGeometry = {x: workspace.virtualScreenSize.width - w.width, y: workspace.virtualScreenSize.height - w.height - 60, width: w.width, height: w.height};'
    for _ in $(seq $(((ASTEPTARE + 30) / 5))); do
        grep -q '^urmărire: gata' "$D/aplicatia.log" && break
        ydotool mousemove -x 2 -y 0; ydotool mousemove -x -2 -y 0
        sleep 5
    done
    grep -q '^urmărire: gata' "$D/aplicatia.log" || { echo "✗ panoul n-a apărut: $D/aplicatia.log"; exit 1; }
    sleep 2
fi
sleep 1.5

read -r EW EH <<< "$(intreaba ecran "$PANOU_JS raport(\"ecran \" + workspace.virtualScreenSize.width + \" \" + workspace.virtualScreenSize.height);")"
[ -n "${EH:-}" ] || { echo "✗ KWin nu dă mărimea ecranului"; exit 1; }
du() { ydotool mousemove --absolute -x 0 -y 0; sleep 0.5; ydotool mousemove -x "$1" -y "$2"; sleep 0.5; }
departe() { du $((EW - 5)) $((EH / 2)); sleep 1; } # marginea din dreapta: în afara ferestrelor

# decupeaza <fișier> <uuid>…: o captură a ecranului, tăiată pe locul ferestrelor date, cu tot cu rama lor; ce nu e fereastră
# devine culoare plată (teste/decupaj.py)
decupeaza() {
    local f=$1 u
    shift
    local locuri=()
    for u in "$@"; do
        [ -n "$u" ] || { echo "✗ KWin nu găsește fereastra pentru $f"; exit 1; }
        locuri+=("$(info "$u" x)" "$(info "$u" y)" "$(info "$u" width)" "$(info "$u" height)")
    done
    departe
    spectacle -b -n -f -o "$PWD/$D/ecran.png"
    sleep 1
    python3 -I teste/decupaj.py "$PWD/$D/ecran.png" "$OUT/$f" "${locuri[@]}"
}

PANOU=$(panou_uuid)
if [ "$MODO" = tot ]; then
decupeaza panou.png "$PANOU"
read -r PX PY <<< "$(panou_colt)"
read -r _ _ CW _ <<< "$(panou_cheie "$GEOM_CONTINUT")"

# ⌨ lui Durana 1: rândul stă sub patru titluri, la 38 + 4 × 24 + 40 / 2 px de colțul barei de unelte. Butonul stă
# înaintea butonului codului, de 24 px, cu 4 px între ele și 4 px de margine: centrul lui e la 46 px de dreapta.
du $((PX + CW - 46)) $((PY + 154))
ydotool click 0xC0
sleep 2
TASTA=""
for _ in $(seq 20); do TASTA=$(uuid bisonus-macro 'Durana 1 — BisonusMacro'); [ -n "$TASTA" ] && break; sleep 0.1; done
decupeaza fereastra-tastei.png "$PANOU" "$TASTA"
# Esc întâi oprește captarea tastei, apoi închide fereastra fără schimbare
ydotool key 1:1 1:0
sleep 0.3
ydotool key 1:1 1:0
sleep 1

# ⚙ e al patrulea buton al barei de unelte: 4 px de margine, trei butoane de 32 px cu 2 px între ele, apoi jumătatea lui
du $((PX + 4 + 3 * 34 + 16)) $((PY + 19))
ydotool click 0xC0
sleep 2
decupeaza fereastra-principala.png "$(uuid bisonus-macro "$TITLU_SETARI")"
fi

# Arborele se schimbă în fișier, iar aplicația îl recitește singură (urmărirea fișierelor). Ferestrele deschise se închid
# înainte, ca să se deschidă din nou cu arborele nou.
pune_arborele() { printf '%s\n' "$1" > "$D/config/bisonus-macro/macros.json"; sleep 3; }
inchide() { js "for (const w of workspace.windowList()) if (w.resourceClass === \"bisonus-macro\" && w.caption === \"$1\") w.closeWindow();"; sleep 1; }
colt() {
    read -r PX PY <<< "$(panou_colt)"
    read -r _ _ CW _ <<< "$(panou_cheie "$GEOM_CONTINUT")"
}

# editorul.png: fereastra codului lui Demo, cu rândul 4 greșit. Un singur macro, într-o singură categorie: rândul stă sub
# bara de unelte de 38 px și titlul categoriei de 24 px, iar centrul lui e la 20 px de sus. Butonul codului are 24 px, cu
# 4 px de margine: centrul lui e la 16 px de dreapta.
[ "$MODO" = tot ] && pune_arborele "$ARBORE_DEMO"
colt
du $((PX + CW - 16)) $((PY + 38 + 24 + 20))
ydotool click 0xC0
sleep 2
EDITOR=$(uuid bisonus-macro 'Demo — code — BisonusMacro')
# uuid tipărește ultimul uuid cerut, chiar dacă fereastra nu există: se verifică titlul ei
if [[ "$(info "$EDITOR" caption)" != *"— code —"* ]]; then
    EDITOR=""
    departe
    spectacle -b -n -f -o "$PWD/$D/esec-editor.png" >/dev/null 2>&1
    js 'for (const w of workspace.windowList()) callDBus("win.bisonus.proba", "/BisonusMacro", "win.bisonus.BisonusMacro", "raport", "lista " + w.resourceClass + "|" + w.caption);'
    sleep 1
    grep '^kwin: lista ' "$KWIN_RECEPTOR" | tail -12
fi
decupeaza editorul.png "$EDITOR"
inchide 'Demo — code — BisonusMacro'
# pagina-goala.png: fereastra principală, pe Macros, cu arborele gol și cele trei carduri
inchide "$TITLU_SETARI"
pune_arborele '{"arbore":[],"sabloane":{}}'
colt
du $((PX + 4 + 3 * 34 + 16)) $((PY + 19))
ydotool click 0xC0
sleep 2
decupeaza pagina-goala.png "$(uuid bisonus-macro "$TITLU_SETARI")"
inchide "$TITLU_SETARI"

echo "✓ capturile sunt în $OUT"

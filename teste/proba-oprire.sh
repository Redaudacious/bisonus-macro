#!/bin/bash
# proba-oprire.sh — criteriul C6. Un macro `loop { key("a") wait(100) }` scrie în fereastra de probă. Proba îl oprește pe
# trei drumuri: „■ Stop” din bara panoului, apăsat cu ydotool, tasta globală „Stop macro” (F9) și a doua apăsare a
# tastei lui (F7). După fiecare oprire, fereastra nu mai primește nicio tastă timp de o secundă. Aplicația ține tastele
# sub componenta de probă `proba-oprire`, iar F7 și F9 trebuie să fie libere. Proba mișcă mouse-ul pe panou, cu
# profilul plat al dispozitivului ydotoold, și îl reface la ieșire. Pune locul panoului la 40,40 și reface locul omului.
# Omul nu atinge mouse-ul și tastatura cam 15 secunde.
# Probează F9, nu implicita Pause: celelalte probe rulează aplicația sub componenta ei obișnuită și îi lasă acolo
# „Stop macro” pe Pause, iar KGlobalAccel dă Pause acelei componente moarte. Pause rămâne neprobată pe desktop.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-oprire
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
KG="busctl --user call org.kde.kglobalaccel"
REGULI="$HOME/.config/kwinrulesrc"
APP=""
TINTA=""
DEV=""

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba apasă tastele și „■ Stop” cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }
for t in F7 F9; do # KGlobalAccel scrie kglobalshortcutsrc cu întârziere
    for _ in $(seq 100); do grep -qP "[=,\t]$t([,\t]|$)" "$HOME/.config/kglobalshortcutsrc" || break; sleep 0.1; done
    grep -qP "[=,\t]$t([,\t]|$)" "$HOME/.config/kglobalshortcutsrc" && { echo "✗ $t nu e liberă în kglobalshortcutsrc"; exit 1; }
done
goleste() {
    local nume=$1 cale=/component/${1//-/_}
    busctl --user --json=short call org.kde.kglobalaccel "$cale" org.kde.kglobalaccel.Component shortcutNames 2>/dev/null \
        | python3 -c 'import sys, json; [print(a) for a in json.load(sys.stdin)["data"][0]]' 2>/dev/null \
        | while IFS= read -r a; do $KG /kglobalaccel org.kde.KGlobalAccel unregister ss "$nume" "$a" >/dev/null 2>&1; done
    $KG "$cale" org.kde.kglobalaccel.Component cleanUp >/dev/null 2>&1
    true
}

# ydotool cere profilul plat al mouse-ului pentru poziția absolută, ca proba-clic-iesire.sh
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
    goleste proba-oprire
    sleep 0.5 # KWin scrie locul ținut minte la închiderea panoului: locul omului se reface după el
    if [ -n "$LOC" ]; then
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position "$LOC"
    else
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position --delete
    fi
    qdbus6 org.kde.KWin /KWin reconfigure
    true
}
trap curata EXIT
goleste proba-oprire # urma unei rulări căzute
seteaza pointerAccelerationProfileFlat true
kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position 40,40
qdbus6 org.kde.KWin /KWin reconfigure

# Cu ecranul stins, prima tastă apăsată doar îl trezește și se pierde: proba trezește ecranul înainte să apese ceva.
kscreen-doctor --dpms on >/dev/null 2>&1; sleep 1.5

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
SETARI="$PWD/$D/config/bisonus-macrorc"
cat > "$SETARI" <<'EOT'
[General]
versiuneVazuta=99.0.0
latimePanou=420
deasupra=true
ferestre=fereastra-tinta
tinta=
tastaOprire=F9

[Nod][bucla]
tasta=F7
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$SETARI"
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": {},
  "arbore": [ { "categorie": "Proba", "id": "proba", "copii": [
    { "macro": "Bucla", "id": "bucla", "script": "loop {\n    key(\"a\")\n    wait(100)\n}\n" } ] } ] }
EOT

build/fereastra-tinta -1 fereastra-tinta 60000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
grep -qx activ "$D/tinta.log" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }
[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
LANGUAGE=en BISONUS_COMPONENTA=proba-oprire XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$SETARI")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
sleep 1.5 # panoul apare după deschide()
read -r PX PY <<< "$(panou_colt)"
[ -n "${PY:-}" ] || { echo "✗ KWin nu dă locul panoului"; exit 1; }

apasari() { grep -c '^apăsat' "$D/tinta.log"; }
porneste() { # F7 are codul 65; macro-ul scrie „a” la 100 ms
    local n
    n=$(apasari)
    ydotool key 65:1 65:0 >/dev/null
    for _ in $(seq 30); do [ "$(apasari)" -ge $((n + 3)) ] && break; sleep 0.1; done
    [ "$(apasari)" -ge $((n + 3)) ] || { echo "✗ $1: macro-ul n-a pornit: $D/tinta.log, $D/aplicatia.log"; exit 1; }
}
oprit() { # după oprire, nicio tastă timp de o secundă
    sleep 0.3
    local n
    n=$(apasari)
    sleep 1
    [ "$(apasari)" = "$n" ] || { echo "✗ $1: fereastra a primit taste după oprire: $D/tinta.log"; exit 1; }
    echo "✓ $1 a oprit macro-ul"
}

# 1. „■ Stop”: capătul din dreapta al barei, la 20 px de margine și la jumătatea celor 28 px
porneste "■ Stop"
ydotool mousemove --absolute -x 0 -y 0; sleep 0.3
ydotool mousemove -x $((PX + 400)) -y $((PY + 14)); sleep 0.3
ydotool click 0xC0
oprit "■ Stop"
ydotool mousemove -x 500 -y 300 # cursorul pleacă de pe panou

# 2. tasta globală „Stop macro”: F9 are codul 67
porneste "F9"
ydotool key 67:1 67:0 >/dev/null
oprit "F9"

# 3. a doua apăsare a lui F7
porneste "F7, a doua oară"
ydotool key 65:1 65:0 >/dev/null
oprit "F7, a doua oară"

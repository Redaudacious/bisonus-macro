#!/bin/bash
# proba-mouse.sh — criteriul C16. Fereastra de probă stă deschisă, iar un macro `mousemove(X, Y)` și `click()`, pornit cu
# F7, duce cursorul în mijlocul ei și face un clic stâng. Fereastra tipărește „clic stang”, iar KWin dă cursorul la cel
# mult 2 px de punct. Un al doilea macro face `click("right")`, iar fereastra tipărește „clic altul”. Aplicația ține
# tastele sub componenta de probă `proba-mouse`. Proba mișcă cursorul: omul nu atinge mouse-ul cam 15 secunde.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-mouse
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
KG="busctl --user call org.kde.kglobalaccel"
APP=""
TINTA=""

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba apasă F7 cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }
for t in F7 F11; do
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
curata() {
    [ -n "$APP" ] && kill "$APP" 2>/dev/null
    [ -n "$TINTA" ] && kill "$TINTA" 2>/dev/null
    wait 2>/dev/null
    scripting unloadScript bisonus-macro >/dev/null 2>&1
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    goleste proba-mouse
    true
}
trap curata EXIT
goleste proba-mouse

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
build/fereastra-tinta -1 fereastra-tinta 60000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
grep -qx activ "$D/tinta.log" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }
U=$(uuid fereastra-tinta fereastra-tinta)
[ -n "$U" ] || { echo "✗ KWin nu dă fereastra de probă"; exit 1; }
X=$(( $(info "$U" x) + $(info "$U" width) / 2 ))
Y=$(( $(info "$U" y) + $(info "$U" height) / 2 ))

SETARI="$PWD/$D/config/bisonus-macrorc"
cat > "$SETARI" <<'EOT'
[General]
versiuneVazuta=99.0.0
deasupra=false
tinta=

[Nod][stang]
tasta=F7

[Nod][drept]
tasta=F11
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$SETARI"
cat > "$D/config/bisonus-macro/macros.json" <<EOT
{ "sabloane": {}, "arbore": [ { "categorie": "Proba", "id": "proba", "copii": [
    { "macro": "Stang", "id": "stang", "script": "mousemove($X, $Y)\nclick()\n" },
    { "macro": "Drept", "id": "drept", "script": "click(\"right\")\n" } ] } ] }
EOT

[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură și mouse: acceptă-o"
LANGUAGE=en BISONUS_COMPONENTA=proba-mouse XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'mouse EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'mouse EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit mouse-ul EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$SETARI")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
sleep 1.5
activeaza fereastra-tinta
sleep 0.5

ydotool key 65:1 65:0 >/dev/null # F7: Stang
for _ in $(seq 30); do grep -qx 'clic stang' "$D/tinta.log" && break; sleep 0.1; done
grep -qx 'clic stang' "$D/tinta.log" || { echo "✗ fereastra n-a primit clicul stâng: $D/tinta.log, $D/aplicatia.log"; exit 1; }
read -r CX CY <<< "$(intreaba cursor 'function raport(t) { callDBus("win.bisonus.proba", "/BisonusMacro", "win.bisonus.BisonusMacro", "raport", t); } raport("cursor " + Math.round(workspace.cursorPos.x) + " " + Math.round(workspace.cursorPos.y));')"
[ -n "${CY:-}" ] && [ $(( (CX - X) * (CX - X) + (CY - Y) * (CY - Y) )) -le 8 ] || { echo "✗ cursorul e la ${CX:-?},${CY:-?}, nu la $X,$Y"; exit 1; }
echo "✓ mousemove($X, $Y) a dus cursorul în fereastră, iar click() a ajuns ca un clic stâng"

ydotool key 87:1 87:0 >/dev/null # F11: Drept, în același loc
for _ in $(seq 30); do grep -qx 'clic altul' "$D/tinta.log" && break; sleep 0.1; done
grep -qx 'clic altul' "$D/tinta.log" || { echo "✗ fereastra n-a primit clicul drept: $D/tinta.log"; exit 1; }
echo "✓ click(\"right\") a ajuns ca alt buton"

#!/bin/bash
# proba-peste-ferestre.sh — criteriul C24. Aplicația pornește cu clasa ferestrei țintă în listă și 📌 oprit. Cât ținta
# e activă, panoul are titlul al doilea și stratul „Popup” (6). În altă fereastră, panoul are titlul întâi, stratul
# obișnuit (2), nu e minimizat și stă în același loc. Un clic dreapta pe bara panoului îl ridică peste fereastra
# activă, care rămâne activă. Apoi ținta trece pe ecran complet, în stratul 5, iar panoul stă în stratul 6. Proba
# mișcă mouse-ul o dată și ține cam 20 de secunde. Ea încarcă scripturi în KWin-ul omului și le descarcă la ieșire.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-peste-ferestre
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
REGULI="$HOME/.config/kwinrulesrc"
APP=""
TINTA=""
ALTA=""

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba face clicul cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
cat > "$D/config/bisonus-macrorc" <<'EOT'
[General]
versiuneVazuta=99.0.0
deasupra=false
ferestre=fereastra-tinta
ferestreTitluri=fereastra-tinta
tinta=
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$D/config/bisonus-macrorc"
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": { "teleport": [ { "tasta": "4" }, { "pauza": 2000 }, { "tasta": "/" },
    { "text": "$nume" }, { "tasta": "Return" } ] },
  "arbore": [ { "categorie": "Rift", "id": "rift", "copii": [
    { "macro": "Snow 2", "id": "rift-snow-2", "sablon": "teleport" } ] } ] }
EOT

# ydotool cere profilul plat al mouse-ului pentru mișcarea de 1:1, ca în proba panoului
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
    [ -n "$ALTA" ] && kill "$ALTA" 2>/dev/null
    wait 2>/dev/null
    for s in $(seq 0 20); do scripting unloadScript "bisonus-macro-ridicare-$s" >/dev/null 2>&1; done
    scripting unloadScript bisonus-macro >/dev/null 2>&1
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    sleep 0.5 # KWin scrie locul ținut minte la închiderea panoului: locul omului se reface după el
    if [ -n "$LOC" ]; then
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position "$LOC"
    else
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position --delete
    fi
    qdbus6 org.kde.KWin /KWin reconfigure
    goleste proba-peste-ferestre
    true
}
trap curata EXIT
kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position 40,40
qdbus6 org.kde.KWin /KWin reconfigure
sleep 1

# ținta pornește prima: ține numele win.bisonus.proba, prin care află uuid-urile
build/fereastra-tinta -1 fereastra-tinta 60000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
grep -qx activ "$D/tinta.log" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }

[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
BISONUS_COMPONENTA=proba-peste-ferestre XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do [ "$(scripting isScriptLoaded bisonus-macro)" = true ] && break; sleep 0.1; done
[ "$(scripting isScriptLoaded bisonus-macro)" = true ] || { echo "✗ aplicația n-a încărcat scriptul în KWin: $D/aplicatia.log"; exit 1; }
sleep 1
activeaza fereastra-tinta
PANOU=""
for _ in $(seq 20); do PANOU=$(uuid bisonus-macro "$TITLU_DOI"); [ -n "$PANOU" ] && break; sleep 0.1; done
[ -n "$PANOU" ] || { echo "✗ cu ținta activă, KWin nu găsește panoul cu titlul al doilea, $TITLU_DOI"; exit 1; }
[ "$(info "$PANOU" layer)" = 6 ] || { echo "✗ cu ținta activă, panoul are stratul $(info "$PANOU" layer), nu 6"; exit 1; }
locul() { echo "$(info "$PANOU" x),$(info "$PANOU" y) $(info "$PANOU" width)x$(info "$PANOU" height)"; }
LOCUL=$(locul)

build/fereastra-tinta -1 fereastra-alta 60000 > "$D/alta.log" 2>&1 &
ALTA=$!
for _ in $(seq 50); do grep -qx activ "$D/alta.log" && break; sleep 0.1; done
sleep 0.5
[ "$(info "$PANOU" caption)" = BisonusMacro ] || { echo "✗ în fereastra din afara listei, panoul are titlul „$(info "$PANOU" caption)”"; exit 1; }
[ "$(info "$PANOU" layer)" = 2 ] || { echo "✗ în fereastra din afara listei, panoul are stratul $(info "$PANOU" layer), nu 2"; exit 1; }
[ "$(info "$PANOU" minimized)" = false ] || { echo "✗ în fereastra din afara listei, panoul e minimizat"; exit 1; }
[ "$(locul)" = "$LOCUL" ] || { echo "✗ panoul s-a mutat la $(locul), de la $LOCUL"; exit 1; }

# clicul dreapta pe spațiul gol al barei de unelte, la 200 px și 19 px de colțul ei: ridică panoul, fără să pornească nimic
stiva() {
    js 'const s = workspace.stackingOrder.map(w => w.resourceClass); callDBus("win.bisonus.proba", "/BisonusMacro", "win.bisonus.BisonusMacro", "raport", "stiva " + s.indexOf("bisonus-macro") + " " + s.indexOf("fereastra-alta") + " " + (workspace.activeWindow ? workspace.activeWindow.resourceClass : "nimic"));'
    sleep 0.5
    grep '^kwin: stiva ' "$D/tinta.log" | tail -1 | cut -d' ' -f3-
}
set -- $(stiva)
[ "$1" -lt "$2" ] && [ "$3" = fereastra-alta ] || { echo "✗ înaintea clicului, panoul nu stă sub fereastra activă: $*"; exit 1; }
seteaza pointerAccelerationProfileFlat true
read -r PX PY <<< "$(panou_colt)"
ydotool mousemove --absolute -x 0 -y 0
sleep 0.3
ydotool mousemove -x $((PX + 200)) -y $((PY + 19))
sleep 0.3
ydotool click 0xC1
sleep 1
set -- $(stiva)
[ "$1" -gt "$2" ] || { echo "✗ după clic, panoul stă tot sub fereastra activă: $*"; exit 1; }
[ "$3" = fereastra-alta ] || { echo "✗ după clic, fereastra activă e $3, nu fereastra-alta"; exit 1; }
ydotool mousemove -x 300 -y 300

js 'for (const w of workspace.windowList()) if (w.resourceClass === "fereastra-tinta" && w.normalWindow) { w.fullScreen = true; workspace.activeWindow = w; }'
sleep 1
T=$(uuid fereastra-tinta fereastra-tinta)
[ "$(info "$T" layer)" = 5 ] || { echo "✗ ținta pe ecran complet are stratul $(info "$T" layer), nu 5"; exit 1; }
[ "$(info "$PANOU" layer)" = 6 ] || { echo "✗ peste ținta pe ecran complet, panoul are stratul $(info "$PANOU" layer)"; exit 1; }
echo "✓ panoul stă în stratul 6 peste țintă, chiar pe ecran complet, e obișnuit în altă fereastră, la $LOCUL, și se ridică la clic"

#!/bin/bash
# proba-bara.sh — C4 al lucrării „Bara tastelor peste joc”. Aplicația pornește sub componenta `proba-bara`, cu trei
# macro-uri cu tastă, deci bara are trei sloturi. Fereastra-țintă stă sub bară și are focusul. Proba iese cu 0 numai
# dacă toate cinci sunt adevărate:
# 1. KWin arată bara în stratul 6, iar ținta rămâne activă și după ce bara apare din nou;
# 2. un clic ydotool pe un slot ajunge la țintă (necunoscuta 3);
# 3. în editare, același clic ajunge la bară, iar după ieșire trece din nou la țintă (necunoscuta 1);
# 4. o tragere ydotool de ⠿ mută bara (necunoscuta 4);
# 5. după două comutări ale editării, bara stă la cel mult 2 px de locul de dinainte (necunoscuta 2).
# Codul de ieșire spune ce a picat: 1 pregătirea, 2 stratul sau focusul, 3 clicul prin bară, 4 clicul în editare,
# 5 mutarea, 6 locul după comutări, 7 locul nescris în setări. Cere sesiunea KDE pe Wayland, ydotoold și mouse-ul
# neatins cam 30 de secunde. Pune locul panoului la 40,40 și al barei la 600,500 în regula KWin și le reface la ieșire.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-bara
ACORD=build/acord.token # tokenul acordului trece de la o rulare la alta: fereastra de acord apare o dată
ASTEPTARE=${ASTEPTARE:-120} # secunde pentru portal și pentru acordul omului
APP=""
TINTA=""
DEV=""
REGULI="$HOME/.config/kwinrulesrc"
TITLU_BARA='Keys · BisonusMacro'

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba face clicul cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
cat > "$D/config/bisonus-macrorc" <<'EOT'
[General]
versiuneVazuta=99.0.0
latimePanou=420
ferestre=fereastra-tinta
ferestreTitluri=fereastra-tinta
tinta=
tastaBara=Ctrl+Alt+Shift+F8
tastaBaraEditare=Ctrl+Alt+Shift+F9

[Nod][f7]
tasta=F7

[Nod][f8]
tasta=F8

[Nod][f11]
tasta=F11
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$D/config/bisonus-macrorc"
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": {}, "arbore": [ { "categorie": "Proba", "id": "proba", "copii": [
    { "macro": "Unu", "id": "f7", "script": "key(\"4\")\n" },
    { "macro": "Doi", "id": "f8", "script": "key(\"4\")\n" },
    { "macro": "Trei", "id": "f11", "script": "key(\"4\")\n" } ] } ] }
EOT

# ydotool cere profilul plat al mouse-ului pentru mișcarea de 1:1. Proba îl pune și îl reface la ieșire.
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
LOCB=$(kreadconfig6 --file "$REGULI" --group bisonus-macro-bara --key position)
refa_loc() { # refa_loc <grupul> <locul vechi, poate gol>
    if [ -n "$2" ]; then
        kwriteconfig6 --file "$REGULI" --group "$1" --key position "$2"
    else
        kwriteconfig6 --file "$REGULI" --group "$1" --key position --delete
    fi
}
curata() {
    [ "$PLAT" = true ] && seteaza pointerAccelerationProfileFlat true
    [ "$ADAPTIV" = true ] && seteaza pointerAccelerationProfileAdaptive true
    opreste "$APP" "$TINTA"
    for n in $(seq 0 20); do scripting unloadScript "bisonus-macro-bara-$n" >/dev/null 2>&1; done
    scripting unloadScript bisonus-macro >/dev/null 2>&1 # kill nu-l lasă pe aplicație să-l descarce
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    sleep 0.5 # KWin scrie locurile ținute minte la închiderea ferestrelor: locurile omului se refac după el
    refa_loc bisonus-macro "$LOC"
    refa_loc bisonus-macro-bara "$LOCB"
    qdbus6 org.kde.KWin /KWin reconfigure
    goleste proba-bara
    true
}
trap curata EXIT
seteaza pointerAccelerationProfileFlat true
kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position 40,40
kwriteconfig6 --file "$REGULI" --group bisonus-macro-bara --key position 600,500
qdbus6 org.kde.KWin /KWin reconfigure
sleep 1

# ținta pornește prima: ține numele win.bisonus.proba, prin care răspund întrebările către KWin
build/fereastra-tinta -1 fereastra-tinta 90000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
grep -qx activ "$D/tinta.log" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }

[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
BISONUS_COMPONENTA=proba-bara XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$D/config/bisonus-macrorc")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
sleep 2 # bara apare după regula KWin

BARA_JS='const bara = workspace.windowList().find(w => w.resourceClass === "bisonus-macro" && w.caption === "'"$TITLU_BARA"'") || null; function raport(t) { callDBus("win.bisonus.proba", "/BisonusMacro", "win.bisonus.BisonusMacro", "raport", t); }'
# bara_cheie <expresie JS>: valoarea expresiei pentru `bara`, sau „lipsa” cât KWin nu are bara
bara_cheie() { intreaba bara "$BARA_JS raport(\"bara \" + (bara ? ($1) : \"lipsa\"));"; }
loc() { bara_cheie '[bara.frameGeometry.x, bara.frameGeometry.y, bara.frameGeometry.width, bara.frameGeometry.height].map(Math.round).join(" ")'; }
invoca() { busctl --user call org.kde.kglobalaccel /component/proba_bara org.kde.kglobalaccel.Component invokeShortcut s "$1" >/dev/null; }
nr() { grep -c "$1" "$D/aplicatia.log"; }
clicuri() { grep -c '^clic stang' "$D/tinta.log"; }
# asteapta <rândul> <de câte ori>: până la 3 secunde, până când jurnalul aplicației are rândul de atâtea ori
asteapta() { for _ in $(seq 30); do [ "$(nr "$1")" -ge "$2" ] && return 0; sleep 0.1; done; return 1; }
# ydotool 1.0.4: --absolute într-o singură mișcare ajunge la poziții greșite. Colțul 0,0, apoi mișcare relativă.
du() { ydotool mousemove --absolute -x 0 -y 0; sleep 0.5; ydotool mousemove -x "$1" -y "$2"; sleep 0.5; }
# misca: un pixel dus și întors, ca KWin să reia fereastra de sub cursor după o schimbare a regiunii de intrare
misca() { ydotool mousemove -x 1 -y 0; ydotool mousemove -x -1 -y 0; sleep 0.2; }

# 1. stratul 6 și focusul
[ "$(bara_cheie 'bara.layer')" = 6 ] || { echo "✗ 1: bara nu e în stratul 6: „$(bara_cheie 'bara.layer')”, $D/aplicatia.log"; exit 2; }
activeaza fereastra-tinta
sleep 0.5
invoca bara
sleep 1
[ "$(bara_cheie 'bara.layer')" = lipsa ] || { echo "✗ 1: tasta barei n-a ascuns bara: $D/aplicatia.log"; exit 2; }
invoca bara
sleep 1
[ "$(bara_cheie 'bara.layer')" = 6 ] || { echo "✗ 1: bara arătată din nou nu e în stratul 6"; exit 2; }
[ "$(activa)" = fereastra-tinta ] || { echo "✗ 1: după apariția barei, fereastra activă e „$(activa)”"; exit 2; }
echo "✓ 1: bara stă în stratul 6, iar ținta rămâne activă când bara apare"

# 2. clicul trece prin bară. Ținta se mută sub bară, cu 60 px de jur împrejur.
read -r BX BY BW BH <<< "$(loc)"
[ -n "${BH:-}" ] || { echo "✗ KWin nu dă locul barei"; exit 1; }
js "for (const w of workspace.windowList()) if (w.resourceClass === \"fereastra-tinta\") w.frameGeometry = {x: $((BX - 60)), y: $((BY - 60)), width: $((BW + 120)), height: $((BH + 120))};"
sleep 0.5
activeaza fereastra-tinta
sleep 0.5
CX=$((BX + 30)) # în primul slot, și cu mânerul de 20 px din editare
CY=$((BY + BH / 2))
N=$(clicuri)
du "$CX" "$CY"
ydotool click 0xC0
sleep 1
[ "$(clicuri)" -gt "$N" ] || { echo "✗ 2: clicul pe slot n-a ajuns la țintă: $D/tinta.log"; exit 3; }
echo "✓ 2: clicul pe slot a trecut prin bară la țintă"

# 3. editarea primește clicul, iar ieșirea îl lasă să treacă din nou
invoca bara-editare
asteapta 'bara: editare true' 1 || { echo "✗ 3: tasta editării n-a pornit editarea: $D/aplicatia.log"; exit 4; }
sleep 0.5
misca
N=$(clicuri)
A=$(nr 'bara: apăsare')
ydotool click 0xC0
sleep 1
[ "$(clicuri)" = "$N" ] && [ "$(nr 'bara: apăsare')" -gt "$A" ] || { echo "✗ 3: în editare, clicul n-a ajuns la bară: $D/tinta.log, $D/aplicatia.log"; exit 4; }
invoca bara-editare
asteapta 'bara: editare false' 1 || { echo "✗ 3: a doua apăsare n-a oprit editarea: $D/aplicatia.log"; exit 4; }
sleep 0.5
misca
N=$(clicuri)
ydotool click 0xC0
sleep 1
[ "$(clicuri)" -gt "$N" ] || { echo "✗ 3: după editare, clicul nu mai trece la țintă: $D/tinta.log"; exit 4; }
echo "✓ 3: în editare clicul ajunge la bară, iar după ieșire trece din nou la țintă"

# 4. tragerea de ⠿: mânerul are 16 px, după marginea de 4 px
invoca bara-editare
asteapta 'bara: editare true' 2 || { echo "✗ 4: editarea n-a pornit a doua oară: $D/aplicatia.log"; exit 5; }
sleep 0.5
read -r BX BY BW BH <<< "$(loc)"
du $((BX + 12)) $((BY + BH / 2))
ydotool click 0x40 # apăsat, fără eliberare
sleep 0.3
for _ in $(seq 10); do ydotool mousemove -x 10 -y 6; sleep 0.05; done
ydotool click 0x80 # eliberat
sleep 1
read -r MX MY _ _ <<< "$(loc)"
[ $((MX - BX)) -ge 50 ] && [ $((MY - BY)) -ge 30 ] || { echo "✗ 4: tragerea de ⠿ n-a mutat bara: de la $BX,$BY la $MX,$MY"; exit 5; }
echo "✓ 4: tragerea de ⠿ a mutat bara de la $BX,$BY la $MX,$MY"

# 5. locul, scris în setări la ieșire și ținut după două comutări
invoca bara-editare
asteapta 'bara: editare false' 2 || { echo "✗ 5: editarea n-a ieșit: $D/aplicatia.log"; exit 6; }
sleep 1
read -r L0X L0Y _ _ <<< "$(loc)"
setare() { kreadconfig6 --file "$PWD/$D/config/bisonus-macrorc" --group General --key "$1"; }
for _ in $(seq 30); do [ "$(setare baraX)" = "$L0X" ] && break; sleep 0.1; done
[ "$(setare baraX)" = "$L0X" ] && [ "$(setare baraY)" = "$L0Y" ] || { echo "✗ 5: setările au locul „$(setare baraX),$(setare baraY)”, nu $L0X,$L0Y"; exit 7; }
invoca bara-editare
asteapta 'bara: editare true' 3 || { echo "✗ 5: editarea n-a pornit a treia oară"; exit 6; }
sleep 0.5
invoca bara-editare
asteapta 'bara: editare false' 3 || { echo "✗ 5: editarea n-a ieșit a treia oară"; exit 6; }
sleep 1
read -r L2X L2Y _ _ <<< "$(loc)"
DX=$((L2X - L0X))
DY=$((L2Y - L0Y))
[ "${DX#-}" -le 2 ] && [ "${DY#-}" -le 2 ] || { echo "✗ 5: după două comutări, bara a venit de la $L0X,$L0Y la $L2X,$L2Y"; exit 6; }
echo "✓ 5: după două comutări, bara stă la $L2X,$L2Y, față de $L0X,$L0Y"

#!/bin/bash
# proba-panou.sh — criteriile C2 și C3. Un clic pe primul macro al panoului îl pregătește (hotărârea 22).
# Proba mută cursorul de pe panou, iar macro-ul scrie „4”, apoi „/Snow 2” și Enter în fereastra cu
# focusul, iar fereastra păstrează focusul. Cu --tine-w sau --tine-shift, o
# tastatură de probă ține W sau Shift stâng cât scrie aplicația: tasta se eliberează în fereastră înainte
# de „/”, după „/” intră exact „Snow 2”, iar după Enter fereastra primește tasta apăsată din nou.
# Cere sesiunea KDE pe Wayland, ydotoold pornit și mouse-ul neatins cam 12 secunde. Aplicația blochează
# tastaturile fizice cam 0,6 secunde. La prima rulare, KDE arată fereastra de acord pentru tastatură.
# Proba pune locul panoului în regula KWin a aplicației, din ~/.config/kwinrulesrc, și reface locul omului la ieșire.
# Colțul barei de unelte îl citește de la KWin, cu teste/kwin.sh: rama are înălțimea temei.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
TASTA=""
[ "${1:-}" = --tine-w ] && TASTA=W
[ "${1:-}" = --tine-shift ] && TASTA=Shift
D=build/proba
[ -n "$TASTA" ] && D=build/proba-tine-$TASTA
ACORD=build/acord.token # tokenul acordului trece de la o rulare la alta: fereastra de acord apare o dată
ASTEPTARE=${ASTEPTARE:-120} # secunde pentru portal și pentru acordul omului
APP=""
TINTA=""
TASTATURA=""
DEV=""
REGULI="$HOME/.config/kwinrulesrc"

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba face clicul cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] && [ -x build/tastatura-proba ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
cat > "$D/config/bisonus-macrorc" <<'EOT'
[General]
versiuneVazuta=99.0.0
latimePanou=420
deasupra=true
ferestre=fereastra-tinta
tinta=
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$D/config/bisonus-macrorc"
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{
  "sabloane": { "teleport": [
    { "tasta": "4" }, { "pauza": 2000 }, { "tasta": "/" },
    { "text": "$nume" }, { "tasta": "Return" } ] },
  "arbore": [ { "categorie": "Bloodlines", "id": "bloodlines", "copii": [
    { "categorie": "Rift", "id": "rift", "copii": [
      { "categorie": "Snow", "id": "rift-snow", "culoare": "#378ADD", "copii": [
        { "macro": "Snow 2", "id": "rift-snow-2", "sablon": "teleport" } ] } ] } ] } ]
}
EOT

# ydotool cere profilul plat al mouse-ului pentru poziția absolută. Proba îl pune și îl reface la ieșire.
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
    [ -n "$TASTATURA" ] && kill "$TASTATURA" 2>/dev/null
    wait 2>/dev/null # a doua rulare din ./test pornește îndată: programele primei trebuie să fi ieșit
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript bisonus-macro >/dev/null 2>&1 # kill nu-l lasă pe aplicație să-l descarce
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    # KWin scrie locul ținut minte la închiderea panoului: locul omului se reface după el
    sleep 0.5
    if [ -n "$LOC" ]; then
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position "$LOC"
    else
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position --delete
    fi
    qdbus6 org.kde.KWin /KWin reconfigure
    goleste proba-panou
    true
}
trap curata EXIT
seteaza pointerAccelerationProfileFlat true

# Panoul stă la 40,40, pe ecranul din colțul 0,0, unde pornește și mișcarea absolută a lui ydotool.
kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position 40,40
qdbus6 org.kde.KWin /KWin reconfigure
sleep 1

# Aplicația pornește întâi: deschide() așteaptă portalul și, la prima rulare, acordul omului.
[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
BISONUS_COMPONENTA=proba-panou XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$D/config/bisonus-macrorc")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
sleep 1 # panoul apare după deschide()

# cu o tastă ținută, fereastra mai stă 2000 ms după Enter, ca să primească tasta apăsată din nou
RAMANE=0
[ -n "$TASTA" ] && RAMANE=2000
build/fereastra-tinta $RAMANE > "$D/tinta.log" 2>&1 &
TINTA=$!
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
grep -qx activ "$D/tinta.log" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }
# ținta ține numele win.bisonus.proba: prin el, KWin dă colțul barei de unelte
export KWIN_RECEPTOR="$D/tinta.log"
read -r PX PY <<< "$(panou_colt)"
[ -n "${PY:-}" ] || { echo "✗ KWin nu dă locul panoului: $D/tinta.log"; exit 1; }

if [ -n "$TASTA" ]; then
    # tasta se apasă înaintea clicului și rămâne apăsată până după Enter
    build/tastatura-proba 6000 "$TASTA" > "$D/tastatura.log" 2>&1 &
    TASTATURA=$!
    for _ in $(seq 30); do grep -qx 'apăsat' "$D/tastatura.log" && break; sleep 0.1; done
    grep -qx 'apăsat' "$D/tastatura.log" || { echo "✗ tastatura de probă n-a apăsat $TASTA: $D/tastatura.log"; exit 1; }
fi

# primul macro: 80 px și 130 px de colțul barei de unelte, sub trei titluri de categorie: 38 + 3 × 24 + 40 / 2
# ydotool 1.0.4: --absolute într-o singură mișcare ajunge la poziții greșite (probat pe KWin). Colțul 0,0, apoi mișcare relativă.
ydotool mousemove --absolute -x 0 -y 0
sleep 0.5
ydotool mousemove -x $((PX + 80)) -y $((PY + 130))
sleep 0.5
ydotool click 0xC0
# hotărârea 22: macro-ul pornește când cursorul iese de pe panou. 500 px la dreapta și 300 px în jos ies din el.
sleep 0.3
ydotool mousemove -x 500 -y 300
for _ in $(seq 80); do grep -q '^text: ' "$D/tinta.log" && break; sleep 0.1; done

# „focus pierdut” numără numai înaintea rândului „text:”: la Enter fereastra se închide și îl pierde oricum
sed '/^text: /q' "$D/tinta.log" | grep -qx 'focus pierdut' && { echo "✗ fereastra țintă a pierdut focusul"; exit 1; }
T=$(sed -n 's/^text: //p' "$D/tinta.log")
if [ -z "$TASTA" ]; then
    [ "$T" = "4/Snow 2" ] || { echo "✗ fereastra țintă a primit „$T”, nu „4/Snow 2”. Jurnalul aplicației: $D/aplicatia.log"; exit 1; }
    echo "✓ fereastra țintă a primit „4/Snow 2” și Enter, cu focusul păstrat"
    exit 0
fi

# W ținut dinainte se repetă în câmp până la jutsu, iar Shift ținut face din 4 „$”. După „/” intră exact „Snow 2”.
MODEL='w*4w*/Snow 2'
[ "$TASTA" = Shift ] && MODEL='[4$]/Snow 2'
echo "$T" | grep -qxE "$MODEL" || { echo "✗ cu $TASTA ținut, fereastra țintă a primit „$T”: după „/” a intrat altceva decât „Snow 2”. Jurnalul aplicației: $D/aplicatia.log"; exit 1; }
sed '/^apăsat \/$/q' "$D/tinta.log" | grep -qx "eliberat $TASTA" || { echo "✗ cu $TASTA ținut, fereastra țintă n-a primit eliberarea înainte de „/”: $D/tinta.log"; exit 1; }
sleep 2.5
sed -n '/^text: /,$p' "$D/tinta.log" | grep -qx "apăsat $TASTA" || { echo "✗ după Enter, fereastra țintă n-a primit $TASTA apăsat: $D/tinta.log"; exit 1; }
echo "✓ cu $TASTA ținut: $TASTA eliberat înainte de „/”, apoi exact „/Snow 2” și Enter, apoi $TASTA apăsat"

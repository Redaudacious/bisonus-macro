#!/bin/bash
# proba-plasa.sh — criteriul C15. O tastatură de probă, dezactivată prin KWin și scrisă cu grupul ei în
# lista „golite” a aplicației, e reactivată de aplicație la pornire, iar lista rămâne goală. Lista n-are
# niciun număr event<N>: plasa găsește tastatura după vendor, product și nume, ca după o repornire a
# calculatorului. Un grup fără dispozitiv se sare. Proba atinge numai tastatura de probă. Cere sesiunea
# KDE pe Wayland și tokenul acordului din build/acord.token.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
D=build/proba-plasa
ACORD=build/acord.token # tokenul acordului trece de la o rulare la alta: fereastra de acord apare o dată
ASTEPTARE=${ASTEPTARE:-120} # secunde pentru portal și pentru acordul omului
APP=""
TASTATURA=""
CALE=""

liber_sau_iesi "probei"
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/tastatura-proba ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
printf '{ "sabloane": {}, "arbore": [] }\n' > "$D/config/bisonus-macro/macros.json" # fără copia din /usr/share
enabled() { busctl --user get-property org.kde.KWin "$CALE" org.kde.KWin.InputDevice enabled; }
curata() {
    # tastatura de probă nu rămâne oprită, oricum ar ieși proba
    [ -n "$CALE" ] && busctl --user set-property org.kde.KWin "$CALE" org.kde.KWin.InputDevice enabled b true 2>/dev/null
    [ -n "$APP" ] && kill "$APP" 2>/dev/null
    [ -n "$TASTATURA" ] && kill "$TASTATURA" 2>/dev/null
    wait 2>/dev/null
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript bisonus-macro >/dev/null 2>&1 # kill nu-l lasă pe aplicație să-l descarce
    goleste proba-plasa
    true
}
trap curata EXIT

# tastatura de probă doar există: nu apasă nimic
build/tastatura-proba 60000 nimic > "$D/tastatura.log" 2>&1 &
TASTATURA=$!
for _ in $(seq 50); do
    for s in $(qdbus6 org.kde.KWin /org/kde/KWin/InputDevice org.kde.KWin.InputDeviceManager.devicesSysNames); do
        [ "$(busctl --user get-property org.kde.KWin "/org/kde/KWin/InputDevice/$s" org.kde.KWin.InputDevice name)" = 's "bisonus-proba"' ] && CALE=/org/kde/KWin/InputDevice/$s
    done
    [ -n "$CALE" ] && break
    sleep 0.1
done
[ -n "$CALE" ] || { echo "✗ KWin nu vede tastatura de probă: $D/tastatura.log"; exit 1; }
N=${CALE##*/event}

busctl --user set-property org.kde.KWin "$CALE" org.kde.KWin.InputDevice enabled b false
[ "$(enabled)" = "b false" ] || { echo "✗ KWin n-a dezactivat tastatura de probă event$N"; exit 1; }
S=/sys/class/input/event$N/device
GRUP="$(cat $S/id/vendor):$(cat $S/id/product):$(cat $S/name)"
printf '[General]\nversiuneVazuta=99.0.0\n\n[Golire]\ngolite=%s,0000:0000:bisonus-lipsa\n' "$GRUP" > "$D/config/bisonus-macrorc"
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$D/config/bisonus-macrorc"

[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
BISONUS_COMPONENTA=proba-plasa XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$D/config/bisonus-macrorc")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"

# plasa rulează în constructorul lui ScriereSigura, după tastatura EIS
for _ in $(seq 50); do grep -q 'plasa: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'plasa: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a scris „plasa: gata” în 5 s după tastatura EIS: $D/aplicatia.log"; exit 1; }
[ "$(enabled)" = "b true" ] || { echo "✗ după pornire, tastatura de probă event$N e încă dezactivată: $D/aplicatia.log"; exit 1; }
L=$(sed -n 's/^golite=//p' "$D/config/bisonus-macrorc")
[ -z "$L" ] || { echo "✗ după pornire, lista golite are încă „$L”"; exit 1; }
echo "✓ plasa a reactivat tastatura de probă event$N, iar lista golite e goală"

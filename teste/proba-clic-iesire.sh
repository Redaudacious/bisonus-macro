#!/bin/bash
# proba-clic-iesire.sh — hotărârea 22 și C9, fără Sober. Un clic pe un rând pregătește macro-ul, iar el
# pornește abia când cursorul iese de pe panou. Trei părți, fiecare cu fereastra țintă în focus:
# 1. clicul, cu cursorul ținut pe panou 1 secundă: ținta nu primește nicio tastă;
# 2. cursorul iese de pe panou: ținta primește „4/Snow 2” și Enter;
# 3. un al doilea clic, cursorul ținut pe panou 4 secunde, apoi ieșirea: ținta nu primește nimic,
#    fiindcă după 3 secunde macro-ul s-a anulat.
# Ținta păstrează focusul prin click-to-focus, oriunde ar sta cursorul: proba mută cursorul numai de pe
# panou, pe un loc liber. Jurnalul aplicației („panou: macro pregătit”, „panou: macro anulat”) arată că
# clicul a pregătit macro-ul și că anularea a venit de la temporizator. Cere sesiunea KDE pe Wayland,
# ydotoold pornit și mouse-ul neatins cam 20 de secunde. Pune locul panoului în regula KWin și îl reface
# la ieșire, ca proba-panou.sh.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-clic-iesire
ACORD=build/acord.token # tokenul acordului trece de la o rulare la alta: fereastra de acord apare o dată
ASTEPTARE=${ASTEPTARE:-120} # secunde pentru portal și pentru acordul omului
APP=""
TINTA=""
DEV=""
REGULI="$HOME/.config/kwinrulesrc"

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
    goleste proba-clic-iesire
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
BISONUS_COMPONENTA=proba-clic-iesire XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$D/config/bisonus-macrorc")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
sleep 1 # panoul apare după deschide()


# primul macro: 80 px și 130 px de colțul barei de unelte, citit de la KWin, sub trei titluri de categorie
# ydotool 1.0.4: --absolute într-o singură mișcare ajunge la poziții greșite (probat pe KWin). Colțul 0,0, apoi mișcare relativă.
pe_panou() { ydotool mousemove --absolute -x 0 -y 0; sleep 0.5; ydotool mousemove -x $((PX + 80)) -y $((PY + 130)); sleep 0.5; }
de_pe_panou() { ydotool mousemove -x 500 -y 300; } # 500 px la dreapta și 300 px în jos ies de pe panou
nr() { grep -c "$1" "$D/aplicatia.log"; }
tinta() { # $1 = jurnalul țintei; ținta se închide după Enter
    build/fereastra-tinta 0 > "$1" 2>&1 &
    TINTA=$!
    for _ in $(seq 50); do grep -qx activ "$1" && break; sleep 0.1; done
    grep -qx activ "$1" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }
}

# partea 1
tinta "$D/tinta1.log"
export KWIN_RECEPTOR="$D/tinta1.log" # prima țintă ține numele win.bisonus.proba
read -r PX PY <<< "$(panou_colt)"
[ -n "${PY:-}" ] || { echo "✗ KWin nu dă locul panoului: $D/tinta1.log"; exit 1; }
pe_panou
ydotool click 0xC0
sleep 1
grep -q '^apăsat' "$D/tinta1.log" && { echo "✗ partea 1: ținta a primit o tastă cu cursorul pe panou: $D/tinta1.log"; exit 1; }
[ "$(nr 'panou: macro pregătit')" = 1 ] || { echo "✗ partea 1: clicul n-a pregătit macro-ul: $D/aplicatia.log"; exit 1; }
echo "✓ 1: clicul a pregătit macro-ul, iar ținta n-a primit nicio tastă cu cursorul pe panou"

# partea 2
de_pe_panou
for _ in $(seq 80); do grep -q '^text: ' "$D/tinta1.log" && break; sleep 0.1; done
T=$(sed -n 's/^text: //p' "$D/tinta1.log")
[ "$T" = "4/Snow 2" ] || { echo "✗ partea 2: ținta a primit „$T”, nu „4/Snow 2”: $D/tinta1.log"; exit 1; }
echo "✓ 2: după ieșirea cursorului, ținta a primit „4/Snow 2” și Enter"
wait "$TINTA" 2>/dev/null # ținta se închide după Enter
sleep 1 # macro-ul se termină, iar rândul se activează din nou

# partea 3
tinta "$D/tinta2.log"
pe_panou
ydotool click 0xC0
sleep 4
[ "$(nr 'panou: macro pregătit')" = 2 ] || { echo "✗ partea 3: al doilea clic n-a pregătit macro-ul: $D/aplicatia.log"; exit 1; }
[ "$(nr 'panou: macro anulat')" = 1 ] || { echo "✗ partea 3: macro-ul n-a fost anulat după 3 secunde: $D/aplicatia.log"; exit 1; }
de_pe_panou
sleep 3
grep -qE '^(apăsat|text: )' "$D/tinta2.log" && { echo "✗ partea 3: ținta a primit taste după anulare: $D/tinta2.log"; exit 1; }
echo "✓ 3: după 4 secunde pe panou macro-ul s-a anulat, iar ținta n-a primit nimic"

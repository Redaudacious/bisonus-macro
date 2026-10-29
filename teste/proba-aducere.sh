#!/bin/bash
# proba-aducere.sh — criteriul C34. Fereastra de probă e ținta, iar altă fereastră e activă. F7, tasta lui Snow 2,
# aduce ținta în față, iar ținta primește exact „4”, „/Snow 2” și Enter. Apoi la fel, cu ținta minimizată. Apoi
# ținta e o clasă fără fereastră: nicio fereastră nu primește nimic, iar jurnalul aplicației scrie „is not open”.
# Aplicația ține tastele sub componenta de probă `proba-aducere`, iar F7 trebuie să fie liberă. Proba apasă F7 cu
# ydotool și nu mișcă mouse-ul. Aplicația blochează tastaturile fizice cam 0,6 secunde de două ori. Ține cam 20 de
# secunde. Ea încarcă scripturi în KWin-ul omului și le descarcă la ieșire.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-aducere
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
KG="busctl --user call org.kde.kglobalaccel"
APP=""
TINTA=""
ALTA=""

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba apasă F7 cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }
# KGlobalAccel scrie kglobalshortcutsrc cu întârziere: după o rulare anterioară, F7 mai stă acolo câteva secunde
for _ in $(seq 100); do grep -qP "[=,\t]F7([,\t]|$)" "$HOME/.config/kglobalshortcutsrc" || break; sleep 0.1; done
if grep -qP "[=,\t]F7([,\t]|$)" "$HOME/.config/kglobalshortcutsrc"; then
    echo "✗ F7 nu e liberă în kglobalshortcutsrc: alege altă tastă în probă"; exit 1
fi

# scoate fiecare acțiune a componentei de probă și componenta, ca F7 să rămână liberă
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
    [ -n "$ALTA" ] && kill "$ALTA" 2>/dev/null
    wait 2>/dev/null
    scripting unloadScript bisonus-macro >/dev/null 2>&1
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    goleste proba-aducere
    true
}
trap curata EXIT
goleste proba-aducere # urma unei rulări căzute

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
SETARI="$PWD/$D/config/bisonus-macrorc"
cat > "$SETARI" <<'EOT'
[General]
versiuneVazuta=99.0.0
deasupra=true
ferestre=fereastra-tinta
tinta=fereastra-tinta
tintaTitlu=fereastra-tinta

[Nod][rift-snow-2]
tasta=F7
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$SETARI"
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": { "teleport": [ { "tasta": "4" }, { "pauza": 500 }, { "tasta": "/" },
    { "text": "$nume" }, { "tasta": "Return" } ] },
  "arbore": [ { "categorie": "Rift", "id": "rift", "copii": [
    { "macro": "Snow 2", "id": "rift-snow-2", "sablon": "teleport" } ] } ] }
EOT

build/fereastra-tinta -1 fereastra-tinta 60000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
LANGUAGE=en BISONUS_COMPONENTA=proba-aducere XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
sleep 1.5
build/fereastra-tinta -1 fereastra-alta 60000 > "$D/alta.log" 2>&1 &
ALTA=$!
for _ in $(seq 50); do grep -qx activ "$D/alta.log" && break; sleep 0.1; done
grep -qx activ "$D/alta.log" || { echo "✗ cealaltă fereastră n-a primit focusul"; exit 1; }

# tasta F7 are codul 65; ydotool o apasă prin dispozitivul lui, ca omul
apasa_f7() { ydotool key 65:1 65:0 >/dev/null; }
texte() { grep -c '^text: ' "$D/tinta.log"; }
asteapta_text() { for _ in $(seq 60); do [ "$(texte)" -ge "$1" ] && break; sleep 0.1; done; }

apasa_f7
asteapta_text 1
[ "$(sed -n 's/^text: //p' "$D/tinta.log" | sed -n 1p)" = "4/Snow 2" ] || { echo "✗ ținta n-a primit „4/Snow 2” și Enter: $D/tinta.log, $D/aplicatia.log"; exit 1; }

sleep 2 # macro-ul rulează încă puțin după Enter: o apăsare în acest timp se ignoră
activeaza fereastra-alta
js 'for (const w of workspace.windowList()) if (w.resourceClass === "fereastra-tinta" && w.normalWindow) w.minimized = true;'
sleep 0.5
apasa_f7
asteapta_text 2
[ "$(sed -n 's/^text: //p' "$D/tinta.log" | sed -n 2p)" = "4/Snow 2" ] || { echo "✗ ținta minimizată n-a primit „4/Snow 2” și Enter: $D/tinta.log"; exit 1; }

kwriteconfig6 --file "$SETARI" --group General --key tinta fereastra-lipsa
kwriteconfig6 --file "$SETARI" --group General --key tintaTitlu fereastra-lipsa
sleep 1.5 # urmărirea recitește setările
activeaza fereastra-alta
sleep 0.5
apasa_f7
sleep 2.5
[ "$(texte)" = 2 ] || { echo "✗ cu ținta lipsă, ținta de probă a primit un text"; exit 1; }
grep -q '^apăsat ' "$D/alta.log" && { echo "✗ cealaltă fereastră a primit taste: $D/alta.log"; exit 1; }
grep -q 'is not open' "$D/aplicatia.log" || { echo "✗ jurnalul aplicației n-are „is not open”: $D/aplicatia.log"; exit 1; }
echo "✓ F7 aduce ținta în față, și minimizată, iar cu ținta lipsă nu scrie nimic"

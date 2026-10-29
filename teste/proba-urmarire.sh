#!/bin/bash
# proba-urmarire.sh — urmărirea fișierelor din C13 și C19, fără Sober. Aplicația pornește cu setări de probă.
# Proba schimbă descrierea din macros.json de două ori: o dată prin înlocuirea fișierului, ca un editor, apoi
# prin scrierea în același fișier. Apoi pune aceeași tastă pe două macro-uri, în bisonus-macrorc. Aplicația
# recitește fiecare schimbare singură și găsește conflictul nou, fără niciun clic.
# Cere sesiunea KDE pe Wayland. Proba nu mișcă mouse-ul și nu apasă nicio tastă. Ține cam 10 secunde.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
D=build/proba-urmarire
ACORD=build/acord.token # tokenul acordului, de la proba panoului: fereastra de acord nu mai apare
ASTEPTARE=${ASTEPTARE:-120}
APP=""

liber_sau_iesi "probei"
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] || { echo "✗ lipsește build/bisonus-macro: rulează cmake --build build"; exit 1; }

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
SETARI="$PWD/$D/config/bisonus-macrorc"
M="$D/config/bisonus-macro/macros.json"
printf '[General]\nversiuneVazuta=99.0.0\n' > "$SETARI"
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$SETARI"
macrouri() { # $1 = descrierea lui Snow 2
    cat <<EOT
{ "sabloane": { "teleport": [ { "tasta": "4" }, { "pauza": 2000 }, { "tasta": "/" },
    { "text": "\$nume" }, { "tasta": "Return" } ] },
  "arbore": [ { "categorie": "Rift", "id": "rift", "copii": [
    { "macro": "Snow 2", "id": "rift-snow-2", "descriere": "$1", "sablon": "teleport" },
    { "macro": "Rain 1", "id": "rift-rain-1", "sablon": "teleport" } ] } ] }
EOT
}
macrouri "prima" > "$M"
trap '[ -n "$APP" ] && kill "$APP" 2>/dev/null; wait 2>/dev/null; qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript bisonus-macro >/dev/null 2>&1; goleste proba-urmarire; true' EXIT

[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
BISONUS_COMPONENTA=proba-urmarire XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'urmărire: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'urmărire: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a pornit urmărirea în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$SETARI")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
sleep 1

nr() { grep -c "$1" "$D/aplicatia.log"; }
asteapta() { # $1 = textul din jurnal, $2 = de câte ori, în cel mult 3 secunde
    for _ in $(seq 30); do [ "$(nr "$1")" -ge "$2" ] && return 0; sleep 0.1; done
    return 1
}

# partea 1: un editor înlocuiește fișierul
macrouri "a doua" > "$M.nou" && mv "$M.nou" "$M"
asteapta 'urmărire: macros.json schimbat' 1 || { echo "✗ 1: fișierul înlocuit nu s-a recitit: $D/aplicatia.log"; exit 1; }
echo "✓ 1: macros.json înlocuit ca de un editor s-a recitit singur"

# partea 2: scrierea în același fișier; urmărirea trebuie să fi trecut pe fișierul nou
macrouri "a treia" > "$M"
asteapta 'urmărire: macros.json schimbat' 2 || { echo "✗ 2: scrierea după înlocuire nu s-a recitit: $D/aplicatia.log"; exit 1; }
echo "✓ 2: macros.json scris în același fișier s-a recitit singur"

# partea 3: aceeași tastă pe două macro-uri, puse de mână în setări
kwriteconfig6 --file "$SETARI" --group Nod --group rift-snow-2 --key tasta F2
kwriteconfig6 --file "$SETARI" --group Nod --group rift-rain-1 --key tasta F2
asteapta 'urmărire: setările schimbate' 1 || { echo "✗ 3: setările schimbate de mână nu s-au recitit: $D/aplicatia.log"; exit 1; }
asteapta 'cazul 2: conflict nou: rift-rain-1, rift-snow-2' 1 || { echo "✗ 3: conflictul nou n-a apărut: $D/aplicatia.log"; exit 1; }
echo "✓ 3: aceeași tastă pe două macro-uri, pusă de mână, a dat conflictul fără niciun clic"

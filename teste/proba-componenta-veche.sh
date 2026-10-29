#!/bin/bash
# proba-componenta-veche.sh — sarcina 9c. La pornire, aplicația curăță două urme ale numelor vechi, ca tastele
# legate pe vremea lor să ajungă la ea:
#   1) componenta KGlobalAccel a numelui vechi, `bloodlines_macro`;
#   2) acțiunile cu nume vechi din propria componentă, de pildă `macro: Snow 1` din 0.2.0.
# Proba nu atinge componentele omului, `bisonus_macro` și `bloodlines_macro`. Ține două taste libere (F7 și
# F11) sub componente de probă: `proba-veche` pentru 1 și `proba-bisonus` pentru 2. Pornește aplicația cu
# BISONUS_COMPONENTA_VECHE=proba-veche și BISONUS_COMPONENTA=proba-bisonus. Pe codul vechi tastele rămân
# la acțiunile vechi. La capăt nicio tastă de probă nu rămâne în kglobalacceld, ca a doua rulare să treacă.
# Cere sesiunea KDE pe Wayland, aplicația omului închisă și tokenul acordului din build/acord.token.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
D=build/proba-componenta-veche
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
TASTA=F7  # ținută de componenta veche; setările o pun la Snow 2
KOD=16777270 # Qt::Key_F7, cum o dă KGlobalAccel în allShortcutInfos
TASTA_VECHE=F11 # ținută de acțiunea cu nume vechi din componenta aplicației
KOD_VECHI=16777274 # Qt::Key_F11
KG="busctl --user call org.kde.kglobalaccel"
APP=""
VECHE=""

liber_sau_iesi "probei"
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/componenta-veche ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }
# tastele trebuie să fie libere la toate componentele, ca proba să nu ia tasta nimănui
for t in $TASTA $TASTA_VECHE; do
    if grep -qP "[=,\t]$t([,\t]|$)" "$HOME/.config/kglobalshortcutsrc"; then
        echo "✗ $t nu e liberă în kglobalshortcutsrc: alege altă tastă în probă"; exit 1
    fi
done

# scoate fiecare acțiune a unei componente de probă și componenta; nu se cheamă pe cele ale omului
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
    [ -n "$VECHE" ] && kill $VECHE 2>/dev/null
    wait 2>/dev/null
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript bisonus-macro >/dev/null 2>&1 # kill nu-l lasă pe aplicație să-l descarce
    # componentele de probă nu rămân, nici dacă aplicația a căzut
    goleste proba-veche
    goleste proba-bisonus
    true
}
trap curata EXIT
goleste proba-veche # urma unei rulări căzute
goleste proba-bisonus

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"

# componentele de probă țin tastele, apoi ies fără destructori: rămân active, fără proces, ca cele vechi
QT_QPA_PLATFORM=offscreen build/componenta-veche 500 proba-veche "panou=$TASTA" > "$D/veche.log" 2>&1 &
VECHE=$!
QT_QPA_PLATFORM=offscreen build/componenta-veche 500 proba-bisonus "macro: Snow 1=$TASTA_VECHE" > "$D/vechi.log" 2>&1 &
VECHE="$VECHE $!"
for _ in $(seq 50); do grep -q gata "$D/veche.log" && grep -q gata "$D/vechi.log" && break; sleep 0.1; done
tine() { busctl --user call org.kde.kglobalaccel "/component/$1" org.kde.kglobalaccel.Component allShortcutInfos 2>/dev/null | grep -q " $2"; }
for _ in $(seq 50); do tine proba_veche $KOD && tine proba_bisonus $KOD_VECHI && break; sleep 0.1; done
tine proba_veche $KOD || { echo "✗ componenta de probă n-a primit $TASTA: $D/veche.log"; exit 1; }
tine proba_bisonus $KOD_VECHI || { echo "✗ acțiunea veche n-a primit $TASTA_VECHE: $D/vechi.log"; exit 1; }
wait $VECHE; VECHE=""
for c in proba_veche proba_bisonus; do
    busctl --user call org.kde.kglobalaccel /component/$c org.kde.kglobalaccel.Component isActive | grep -qx 'b true' \
        || { echo "✗ componenta $c nu a rămas activă după ieșirea procesului"; exit 1; }
done

# macros.json al probei, ca rezultatul să nu depindă de fișierul pachetului din /usr/share
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": {},
  "arbore": [ { "categorie": "Rift", "id": "rift", "copii": [
    { "macro": "Snow 2", "id": "rift-snow-2", "script": "key(\"4\")\n" } ] } ] }
EOT
# setările pun F7 la Snow 2
printf '[General]\nversiuneVazuta=99.0.0\n\n[Nod][rift-snow-2]\ntasta=%s\n' "$TASTA" > "$D/config/bisonus-macrorc"
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$D/config/bisonus-macrorc"
[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
BISONUS_COMPONENTA_VECHE=proba-veche BISONUS_COMPONENTA=proba-bisonus XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$D/config/bisonus-macrorc")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
sleep 1 # aplicația leagă tastele după tastatura EIS

# 1) componenta veche nu mai ține tasta
tine proba_veche $KOD && { echo "✗ tasta $TASTA a rămas la componenta veche proba_veche"; exit 1; }
# 2) tasta e a aplicației
tine proba_bisonus $KOD || { echo "✗ tasta $TASTA nu e la componenta aplicației, proba_bisonus"; exit 1; }
echo "✓ tasta $TASTA a trecut de la componenta veche la componenta aplicației"
# 3) acțiunea cu nume vechi a pierdut tasta și a ieșit din componentă
tine proba_bisonus $KOD_VECHI && { echo "✗ tasta $TASTA_VECHE a rămas la acțiunea veche macro: Snow 1"; exit 1; }
busctl --user --json=short call org.kde.kglobalaccel /component/proba_bisonus org.kde.kglobalaccel.Component shortcutNames \
    | grep -qF '"macro: Snow 1"' && { echo "✗ acțiunea veche macro: Snow 1 a rămas în componentă"; exit 1; }
echo "✓ acțiunea veche macro: Snow 1 a ieșit din componenta aplicației"

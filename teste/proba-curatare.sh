#!/bin/bash
# proba-curatare.sh — criteriul C15. Aplicația rulează cu HOME mutat în build/, sub componenta de probă `proba-curatare`,
# și lasă regula KWin, scriptul încărcat și tastele globale. După `kill`, `--cleanup` le scoate pe toate, iar setările
# rămân. Apoi `--cleanup --all` le șterge și lasă copia în HOME. Cu aplicația pornită, `--cleanup` refuză după 5 secunde.
# HOME mutat ține regula de probă departe de kwinrulesrc al omului: proba îl compară la capăt octet cu octet. Proba nu
# mișcă mouse-ul și nu apasă taste. Ține cam 25 de secunde.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-curatare
ACASA="$PWD/$D/home"
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
KG="busctl --user call org.kde.kglobalaccel"
SCURTATURI="$HOME/.config/kglobalshortcutsrc"
REGULI_OMULUI="$HOME/.config/kwinrulesrc"
APP=""

liber_sau_iesi "probei"
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] || { echo "✗ lipsește build/bisonus-macro: rulează cmake --build build"; exit 1; }
SUMA=$(sha256sum "$REGULI_OMULUI" 2>/dev/null)
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
    wait 2>/dev/null
    scripting unloadScript bisonus-macro >/dev/null 2>&1
    goleste proba-curatare
    true
}
trap curata EXIT
incarcat() { [ "$(scripting isScriptLoaded bisonus-macro)" = true ]; }
goleste proba-curatare # urma unei rulări căzute
scripting unloadScript bisonus-macro >/dev/null 2>&1 # un script rămas de la o rulare căzută ar trece drept cel al aplicației
for _ in $(seq 50); do incarcat 2>/dev/null || break; sleep 0.1; done
# aplicația, cu HOME mutat; XDG_CONFIG_HOME și XDG_DATA_HOME ale omului nu trec
# `pornita` ia locul subshell-ului cu exec, ca $! din fundal să fie chiar procesul aplicației și `kill` să-l ajungă
pornita() { exec env -u XDG_CONFIG_HOME -u XDG_DATA_HOME HOME="$ACASA" LANGUAGE=en BISONUS_COMPONENTA=proba-curatare \
    BISONUS_COMPONENTA_VECHE=proba-curatare-veche build/bisonus-macro "$@"; }
ruleaza() { (pornita "$@"); }
are_componenta() { grep -q '^\[proba-curatare\]' "$SCURTATURI"; }

rm -rf "$D"
mkdir -p "$ACASA/.config/bisonus-macro"
cat > "$ACASA/.config/bisonus-macrorc" <<'EOT'
[General]
versiuneVazuta=99.0.0
deasupra=true
ferestre=fereastra-tinta
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$ACASA/.config/bisonus-macrorc"
cat > "$ACASA/.config/bisonus-macro/macros.json" <<'EOT'
{ "sabloane": {}, "arbore": [ { "categorie": "Proba", "id": "proba", "copii": [
    { "macro": "Snow 2", "id": "snow-2", "script": "key(\"4\")\n" } ] } ] }
EOT

[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
pornita > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
for _ in $(seq 100); do are_componenta && break; sleep 0.1; done # KGlobalAccel scrie cu întârziere
are_componenta || { echo "✗ componenta proba-curatare nu apare în kglobalshortcutsrc"; exit 1; }
grep -q '^\[bisonus-macro\]' "$ACASA/.config/kwinrulesrc" || { echo "✗ aplicația n-a scris regula în $ACASA/.config/kwinrulesrc"; exit 1; }
for _ in $(seq 100); do incarcat && break; sleep 0.1; done # aplicația îl încarcă după tastatură
incarcat || { echo "✗ scriptul bisonus-macro nu e încărcat în KWin"; exit 1; }
ls "$ACASA/.local/share/bisonus-macro/"*.js >/dev/null 2>&1 || { echo "✗ lipsesc scripturile din $ACASA/.local/share/bisonus-macro"; exit 1; }
echo "✓ aplicația a lăsat regula, scriptul și tastele"

# cu aplicația pornită, --cleanup refuză după 5 secunde
ruleaza --cleanup > "$D/refuz.log" 2>&1
[ $? = 1 ] && grep -q 'is running' "$D/refuz.log" || { echo "✗ --cleanup n-a refuzat cu aplicația pornită: $D/refuz.log"; exit 1; }
echo "✓ --cleanup refuză cât rulează aplicația"

kill "$APP"
wait "$APP" 2>/dev/null
APP=""
ruleaza --cleanup > "$D/curatare.log" 2>&1 || { echo "✗ --cleanup a ieșit cu eroare: $D/curatare.log"; exit 1; }
grep -q '^\[bisonus-macro' "$ACASA/.config/kwinrulesrc" && { echo "✗ regula a rămas în kwinrulesrc"; exit 1; }
incarcat && { echo "✗ scriptul a rămas încărcat în KWin"; exit 1; }
ls "$ACASA/.local/share/bisonus-macro/"*.js >/dev/null 2>&1 && { echo "✗ au rămas fișiere .js"; exit 1; }
for _ in $(seq 100); do are_componenta || break; sleep 0.1; done
are_componenta && { echo "✗ componenta proba-curatare a rămas în kglobalshortcutsrc"; exit 1; }
[ -f "$ACASA/.config/bisonus-macrorc" ] && [ -f "$ACASA/.config/bisonus-macro/macros.json" ] || { echo "✗ --cleanup a șters setările"; exit 1; }
echo "✓ --cleanup a scos regula, scriptul și tastele, iar setările au rămas"

ruleaza --cleanup --all > "$D/tot.log" 2>&1 || { echo "✗ --cleanup --all a ieșit cu eroare: $D/tot.log"; exit 1; }
[ -e "$ACASA/.config/bisonus-macrorc" ] || [ -e "$ACASA/.config/bisonus-macro" ] && { echo "✗ setările au rămas după --all"; exit 1; }
COPIE=$(ls "$ACASA"/bisonus-macro-backup-*.json 2>/dev/null | head -1)
[ -n "$COPIE" ] && grep -q 'Snow 2' "$COPIE" || { echo "✗ copia lipsește sau n-are macro-ul: $D/tot.log"; exit 1; }
echo "✓ --cleanup --all a șters setările și a lăsat copia $(basename "$COPIE")"

[ "$(sha256sum "$REGULI_OMULUI" 2>/dev/null)" = "$SUMA" ] || { echo "✗ kwinrulesrc al omului s-a schimbat"; exit 1; }
echo "✓ kwinrulesrc al omului a rămas octet cu octet"

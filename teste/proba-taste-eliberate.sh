#!/bin/bash
# proba-taste-eliberate.sh — criteriul C47. Rift, o categorie sub Zona, are tasta F7. Cu comutatorul general oprit,
# F7 nu e în KGlobalAccel, iar ținta o primește ca tastă obișnuită. După ce setările pornesc comutatorul, F7 revine în
# KGlobalAccel fără repornire. Apoi Zona se oprește, F7 se eliberează din nou, iar ținta o primește iar.
# Aplicația ține tastele sub componenta de probă `proba-taste-eliberate`. Proba apasă F7 cu ydotool și nu mișcă mouse-ul.
# Ținta tipărește „apăsat F7” pentru F7. Ține cam 15 secunde. Ea încarcă scripturi în KWin-ul omului și le descarcă la ieșire.
set -u
cd "$(dirname "$0")/.."
source teste/kwin.sh
source teste/componenta.sh
D=build/proba-taste-eliberate
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
COMPONENTA=proba-taste-eliberate
KOD=16777270 # Qt::Key_F7, cum o dă KGlobalAccel în allShortcutInfos
APP=""
TINTA=""

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba apasă F7 cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }
# KGlobalAccel scrie kglobalshortcutsrc cu întârziere: după o rulare anterioară, F7 mai stă acolo câteva secunde
for _ in $(seq 100); do grep -qP "[=,\t]F7([,\t]|$)" "$HOME/.config/kglobalshortcutsrc" || break; sleep 0.1; done
if grep -qP "[=,\t]F7([,\t]|$)" "$HOME/.config/kglobalshortcutsrc"; then
    echo "✗ F7 nu e liberă în kglobalshortcutsrc: alege altă tastă în probă"; exit 1
fi

curata() {
    [ -n "$APP" ] && kill "$APP" 2>/dev/null
    [ -n "$TINTA" ] && kill "$TINTA" 2>/dev/null
    wait 2>/dev/null
    scripting unloadScript bisonus-macro >/dev/null 2>&1
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    goleste "$COMPONENTA"
    true
}
trap curata EXIT
goleste "$COMPONENTA" # urma unei rulări căzute

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
SETARI="$PWD/$D/config/bisonus-macrorc"
cat > "$SETARI" <<'EOT'
[General]
versiuneVazuta=99.0.0
activ=false

[Nod][rift]
tasta=F7
EOT
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$SETARI"
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{ "arbore": [ { "categorie": "Zona", "id": "zona", "copii": [
    { "categorie": "Rift", "id": "rift", "copii": [
      { "macro": "Snow 2", "id": "rift-snow-2", "pasi": [ { "tasta": "4" } ] } ] } ] } ] }
EOT

build/fereastra-tinta -1 fereastra-tinta 60000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
LANGUAGE=en BISONUS_COMPONENTA="$COMPONENTA" XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
sleep 1.5

# tine: F7 e la componenta de probă. asteapta <tine|liber>: așteaptă până la 5 secunde starea cerută
tine() { busctl --user call org.kde.kglobalaccel "/component/${COMPONENTA//-/_}" org.kde.kglobalaccel.Component allShortcutInfos 2>/dev/null | grep -q " $KOD"; }
asteapta() { for _ in $(seq 50); do if [ "$1" = tine ]; then tine && return 0; else tine || return 0; fi; sleep 0.1; done; return 1; }
apasa_f7() { activeaza fereastra-tinta; sleep 0.3; ydotool key 65:1 65:0 >/dev/null; sleep 0.7; }
apasari() { grep -c '^apăsat F7$' "$D/tinta.log"; }

# 1. comutatorul general oprit: F7 nu e prinsă și ajunge la țintă
tine && { echo "✗ cu comutatorul oprit, F7 e în KGlobalAccel: $D/aplicatia.log"; exit 1; }
apasa_f7
[ "$(apasari)" = 1 ] || { echo "✗ cu comutatorul oprit, ținta n-a primit F7: $D/tinta.log"; exit 1; }

# 2. comutatorul pornit prin setări: F7 revine fără repornire
kwriteconfig6 --file "$SETARI" --group General --key activ true
asteapta tine || { echo "✗ după pornirea comutatorului, F7 n-a revenit în KGlobalAccel: $D/aplicatia.log"; exit 1; }

# 3. Zona oprită: tasta lui Rift se eliberează și ajunge la țintă
kwriteconfig6 --file "$SETARI" --group Nod --group zona --key activ false
asteapta liber || { echo "✗ cu Zona oprită, F7 a rămas în KGlobalAccel: $D/aplicatia.log"; exit 1; }
apasa_f7
[ "$(apasari)" = 2 ] || { echo "✗ cu Zona oprită, ținta n-a primit F7: $D/tinta.log"; exit 1; }
echo "✓ cu comutatorul oprit sau cu Zona oprită, F7 nu e prinsă și ajunge la țintă; cu comutatorul pornit, F7 revine singură"

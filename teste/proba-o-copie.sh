#!/bin/bash
# proba-o-copie.sh — criteriul C4. După două porniri rulează un singur proces bisonus-macro: a doua
# pornire desface panoul primei copii și iese cu 0. Cere sesiunea KDE, cu D-Bus. Numără copiile pornite de ea, după PID:
# aplicația omului poartă același nume de proces și nu intră în număr.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
D=build/proba-o-copie

liber_sau_iesi "probei"
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] || { echo "✗ lipsește build/bisonus-macro: rulează cmake --build build"; exit 1; }

PRIMA=
DOUA=
rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
printf '{ "sabloane": {}, "arbore": [] }\n' > "$D/config/bisonus-macro/macros.json" # fără copia din /usr/share
trap 'opreste "$PRIMA" "$DOUA"; qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript bisonus-macro >/dev/null 2>&1; goleste proba-o-copie; true' EXIT

BISONUS_COMPONENTA=proba-o-copie XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/prima.log" 2>&1 &
PRIMA=$!
sleep 2
BISONUS_COMPONENTA=proba-o-copie XDG_CONFIG_HOME="$PWD/$D/config" timeout 10 build/bisonus-macro > "$D/a-doua.log" 2>&1 &
DOUA=$!
wait "$DOUA"
COD=$?
sleep 1
N=$(vii "$PRIMA" "$DOUA")
[ "$COD" = 0 ] || { echo "✗ a doua pornire a ieșit cu $COD, nu cu 0. Jurnalul ei: $D/a-doua.log"; exit 1; }
[ "$N" = 1 ] || { echo "✗ rulează $N procese bisonus-macro, nu 1"; exit 1; }
echo "✓ după două porniri rulează un singur proces bisonus-macro"

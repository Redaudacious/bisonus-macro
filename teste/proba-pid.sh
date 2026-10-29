#!/bin/bash
# proba-pid.sh — C49. Probele de desktop opresc numai procesele lor, după PID, și caută aplicația pe D-Bus, după numele
# win.bisonus.bisonus-macro, nu după numele procesului. Aplicația pornită de om din meniu poartă același nume de proces:
# o probă care o închidea cu pkill sau o lua drept „copie rămasă” a stricat lucrul omului. Fără desktop.
# O „momeală” e un sleep numit bisonus-macro: are numele procesului, dar nu ține numele de pe D-Bus.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
D=build/momeala
NUME=bisonus # numele aplicației, rupt în două ca grepul de mai jos să nu se găsească pe sine

MOMEALA=
PROPRIU=
cu_eroare() { echo "✗ $1"; exit 1; }
curata() {
    [ -n "$MOMEALA" ] && kill "$MOMEALA" 2>/dev/null
    [ -n "$PROPRIU" ] && kill "$PROPRIU" 2>/dev/null
    wait 2>/dev/null
    true
}
trap curata EXIT

rm -rf "$D"
mkdir -p "$D"
cp /usr/bin/sleep "$D/$NUME-macro"
"$D/$NUME-macro" 600 &
MOMEALA=$!
"$D/$NUME-macro" 600 & # un proces al probei, pornit la fel
PROPRIU=$!
sleep 0.2

# momeala are numele procesului, deci un pgrep pe nume ar lua-o drept aplicația
pgrep -x "$NUME-macro" | grep -qx "$MOMEALA" || cu_eroare "momeala nu poartă numele $NUME-macro: proba nu probează nimic"

# verificarea de la pornire: numai numele de pe D-Bus, deci momeala nu o oprește
type aplicatia_ruleaza >/dev/null 2>&1 || cu_eroare "teste/componenta.sh nu are aplicatia_ruleaza"
[ "$(pid_aplicatie)" != "$MOMEALA" ] || cu_eroare "verificarea a luat momeala drept aplicația"
(source teste/componenta.sh; liber_sau_iesi "probei" >/dev/null) || aplicatia_ruleaza || cu_eroare "liber_sau_iesi a refuzat fără aplicație"

# oprirea: lista goală nu oprește nimic, iar lista cu un PID oprește numai pe el
type opreste >/dev/null 2>&1 || cu_eroare "teste/componenta.sh nu are opreste"
opreste ""
kill -0 "$MOMEALA" 2>/dev/null || cu_eroare "opreste cu lista goală a oprit momeala"
kill -0 "$PROPRIU" 2>/dev/null || cu_eroare "opreste cu lista goală a oprit procesul probei"
[ "$(vii "$MOMEALA" "$PROPRIU")" = 2 ] || cu_eroare "vii nu numără cele două procese"
opreste "$PROPRIU"
kill -0 "$PROPRIU" 2>/dev/null && cu_eroare "opreste n-a oprit procesul probei"
kill -0 "$MOMEALA" 2>/dev/null || cu_eroare "opreste a oprit momeala, care nu era în listă"
PROPRIU=
[ "$(vii "$MOMEALA")" = 1 ] || cu_eroare "vii nu numără momeala vie"

# nicio probă nu oprește procese după nume și nu caută aplicația după nume
if grep -nE "(pkill|killall).*$NUME-macro" teste/*.sh test; then cu_eroare "o probă oprește procesele după nume"; fi
if grep -nE "pgrep -x $NUME-macro" teste/*.sh test | grep -v "proba-pid.sh"; then cu_eroare "o probă caută aplicația după numele procesului"; fi
for f in teste/*.sh test; do bash -n "$f" || cu_eroare "$f nu trece bash -n"; done

# la capăt momeala trăiește încă: o oprește proba, după PID
kill -0 "$MOMEALA" 2>/dev/null || cu_eroare "momeala a murit"
echo "✓ momeala numită $NUME-macro a trăit, iar probele caută aplicația pe D-Bus și opresc după PID"

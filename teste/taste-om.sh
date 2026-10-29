#!/bin/bash
# taste-om.sh — păzește tastele omului din kglobalshortcutsrc.
#   bash teste/taste-om.sh salveaza <fișier>   scrie în fișier secțiunile componentelor aplicației
#   bash teste/taste-om.sh compara <fișier>    iese cu 1 și arată diferența, dacă secțiunile s-au schimbat
# Componentele sunt ale omului: bisonus-macro, bisonus_macro, bloodlines-macro, bloodlines_macro. Locul fișierului: $HOME/.config.
set -u
[ $# = 2 ] && { [ "$1" = salveaza ] || [ "$1" = compara ]; } || { echo "folosire: taste-om.sh salveaza|compara <fișier>" >&2; exit 2; }
RC="$HOME/.config/kglobalshortcutsrc"

sectiunile() {
    [ -f "$RC" ] || return 0
    awk '/^\[/ { tine = ($0 == "[bisonus-macro]" || $0 == "[bisonus_macro]" || $0 == "[bloodlines-macro]" || $0 == "[bloodlines_macro]") }
         tine && NF' "$RC"
}

case $1 in
salveaza) sectiunile > "$2" ;;
compara)
    [ -f "$2" ] || { echo "lipsește $2: rulează întâi salveaza" >&2; exit 2; }
    if ! diff -u "$2" <(sectiunile) >&2; then
        echo "✗ tastele omului din kglobalshortcutsrc s-au schimbat" >&2
        exit 1
    fi
    ;;
esac

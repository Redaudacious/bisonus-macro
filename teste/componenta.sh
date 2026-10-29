#!/bin/bash
# componenta.sh — o probă care pornește aplicația o ține sub o componentă KGlobalAccel de probă, nu sub `bisonus-macro`
# a omului. Se încarcă cu `source teste/componenta.sh`; aplicația pornește cu BISONUS_COMPONENTA=<nume>.
KG="busctl --user call org.kde.kglobalaccel"

# goleste <nume>: scoate fiecare acțiune a componentei de probă și componenta, ca tastele ei să nu rămână în kglobalshortcutsrc
goleste() {
    local nume=$1 cale=/component/${1//-/_}
    busctl --user --json=short call org.kde.kglobalaccel "$cale" org.kde.kglobalaccel.Component shortcutNames 2>/dev/null \
        | python3 -c 'import sys, json; [print(a) for a in json.load(sys.stdin)["data"][0]]' 2>/dev/null \
        | while IFS= read -r a; do $KG /kglobalaccel org.kde.KGlobalAccel unregister ss "$nume" "$a" >/dev/null 2>&1; done
    $KG "$cale" org.kde.kglobalaccel.Component cleanUp >/dev/null 2>&1
    true
}

# Aplicația omului și probele poartă același nume de proces. Ce intră în conflict cu o probă e numele unic de pe D-Bus,
# win.bisonus.bisonus-macro, pus de KDBusService::Unique. De aceea probele caută numele acela și opresc după PID,
# niciodată după numele procesului.
# pid_aplicatie: PID-ul celui care ține numele; gol când nimeni nu-l ține (sau când nu e D-Bus)
pid_aplicatie() { busctl --user status win.bisonus.bisonus-macro 2>/dev/null | sed -n 's/^PID=//p'; }
aplicatia_ruleaza() { [ -n "$(pid_aplicatie)" ]; }
# liber_sau_iesi <ce urmează>: oprește scriptul cu 1 dacă aplicația rulează
liber_sau_iesi() { aplicatia_ruleaza && { echo "✗ aplicația rulează deja: închide-o înaintea $1"; exit 1; }; true; }
# opreste <PID>...: oprește numai procesele date și le așteaptă; un PID gol sau mort nu face nimic
opreste() {
    local p
    for p in "$@"; do
        [ -n "$p" ] || continue
        kill "$p" 2>/dev/null
        wait "$p" 2>/dev/null
    done
    true
}
# vii <PID>...: câte dintre procesele date trăiesc
vii() {
    local n=0 p
    for p in "$@"; do
        [ -n "$p" ] && kill -0 "$p" 2>/dev/null && n=$((n + 1))
    done
    echo "$n"
}

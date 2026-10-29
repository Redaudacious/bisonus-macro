#!/bin/bash
# proba-taste-om.sh — arată că teste/taste-om.sh prinde orice schimbare a tastelor omului din kglobalshortcutsrc.
# Nu cere desktop: lucrează pe un kglobalshortcutsrc de probă, cu HOME mutat. Ține sub o secundă.
set -u
cd "$(dirname "$0")/.."
D=build/proba-taste-om
rm -rf "$D"
mkdir -p "$D/.config"
export HOME="$PWD/$D"
RC="$HOME/.config/kglobalshortcutsrc"
SALVAT="$D/salvat"
CAZ=0

# asteapta <cod> <ce>: comanda de după ele trebuie să iasă cu codul dat
asteapta() {
    local cod=$1 ce=$2; shift 2
    CAZ=$((CAZ + 1))
    "$@" >"$D/ultima.log" 2>&1
    local ies=$?
    [ "$ies" = "$cod" ] || { echo "✗ cazul $CAZ ($ce): cod $ies, trebuia $cod"; cat "$D/ultima.log"; exit 1; }
}

cat > "$RC" <<'EOT'
[ForceGlobalShortcuts]
foo=1

[bisonus-macro]
_k_friendly_name=BisonusMacro
macro: Snow 1=F11,none,Snow 1

[bloodlines_macro]
_k_friendly_name=Bloodlines
Stop=Pause,none,Stop

[kwin]
Show Desktop=Meta+D,none,Show Desktop
EOT

asteapta 0 "salvarea" bash teste/taste-om.sh salveaza "$SALVAT"
asteapta 0 "nicio schimbare" bash teste/taste-om.sh compara "$SALVAT"

# schimbarea unei secțiuni care nu e a omului nu se vede
sed -i 's/^Show Desktop=.*/Show Desktop=Meta+X,none,Show Desktop/' "$RC"
asteapta 0 "altă componentă" bash teste/taste-om.sh compara "$SALVAT"

# o tastă schimbată
sed -i 's/^macro: Snow 1=.*/macro: Snow 1=F7,none,Snow 1/' "$RC"
asteapta 1 "tastă schimbată" bash teste/taste-om.sh compara "$SALVAT"
bash teste/taste-om.sh salveaza "$SALVAT"

# o secțiune nouă a omului
printf '\n[bloodlines-macro]\nStop=F9,none,Stop\n' >> "$RC"
asteapta 1 "secțiune nouă" bash teste/taste-om.sh compara "$SALVAT"
bash teste/taste-om.sh salveaza "$SALVAT"

# o secțiune dispărută
sed -i '/^\[bloodlines_macro\]/,/^$/d' "$RC"
asteapta 1 "secțiune dispărută" bash teste/taste-om.sh compara "$SALVAT"
bash teste/taste-om.sh salveaza "$SALVAT"

# fișierul întreg dispărut
rm "$RC"
asteapta 1 "fișier dispărut" bash teste/taste-om.sh compara "$SALVAT"

# fără fișier la salvare și la comparare: nimic de păzit
bash teste/taste-om.sh salveaza "$SALVAT"
asteapta 0 "fără fișier" bash teste/taste-om.sh compara "$SALVAT"

# argumente greșite
asteapta 2 "argumente" bash teste/taste-om.sh compara

echo "✓ teste/taste-om.sh: $CAZ cazuri"

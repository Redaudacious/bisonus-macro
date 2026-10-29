#!/bin/bash
# proba-rama.sh — prima sarcină a planului 17 și C42, fără Sober. Aplicația pornește cu clasa ferestrei țintă în listă
# și 📌 oprit, iar panoul stă la 40,40. Proba citește de la KWin titlul, stratul și locul panoului și face cu ydotool:
# 1. titlurile: cu ținta activă, titlul al doilea și stratul 6; în altă fereastră, BisonusMacro și stratul 2;
# 2. tragerea barei de sus mută panoul, iar ținta rămâne activă;
# 3. marginea din dreapta lățește panoul, iar aplicația ține minte lățimea nouă;
# 4. □ mărește panoul, iar al doilea clic îl pune la loc;
# 5. – scoate panoul din KWin, iar clicul pe tavă îl aduce înapoi, neminimizat;
# 6. ✕ oprește aplicația.
# Cu RAMA=1 din teste/kwin.sh, bara de sus e rama lui KWin, iar proba face capturile build/proba-rama/rama.png,
# titlu-1.png, titlu-2.png și lista.png. Cu RAMA=0, bara de sus e a aplicației, iar panoul n-are ramă.
# Codul de ieșire alege drumul primei sarcini: 2 cu RAMA=1, la tragere, margine, □, – sau ✕ (rezerva); 3 la titlul al
# doilea (rezerva necunoscutei 44); 4 la – care lasă panoul minimizat (rezerva necunoscutei 45); 1 la orice altceva.
# X_INCHIDE (centrul lui ✕, în px de la marginea dreaptă a ramei) și PAS (distanța dintre centrele a două butoane)
# schimbă locul clicurilor pe butoanele ramei. Omul nu atinge mouse-ul cam 40 de secunde.
set -u
cd "$(dirname "$0")/.."
source teste/componenta.sh
source teste/kwin.sh
D=build/proba-rama
ACORD=build/acord.token
ASTEPTARE=${ASTEPTARE:-120}
REGULI="$HOME/.config/kwinrulesrc"
APP=""
TINTA=""
ALTA=""
DEV=""

liber_sau_iesi "probei"
pgrep -x ydotoold >/dev/null || { echo "✗ ydotoold nu rulează: proba face clicurile cu ydotool"; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "✗ proba cere sesiunea grafică Wayland"; exit 1; }
[ -x build/bisonus-macro ] && [ -x build/fereastra-tinta ] || { echo "✗ lipsesc programele din build/: rulează cmake --build build"; exit 1; }

rm -rf "$D"
mkdir -p "$D/config/bisonus-macro"
printf '[General]\nversiuneVazuta=99.0.0\nlatimePanou=420\ndeasupra=false\nferestre=fereastra-tinta\ntinta=\ntransparent=true\nopacitate=30\n' > "$D/config/bisonus-macrorc"
[ -s "$ACORD" ] && printf '\n[Portal]\nrestore_token=%s\n' "$(cat "$ACORD")" >> "$D/config/bisonus-macrorc"
cat > "$D/config/bisonus-macro/macros.json" <<'EOT'
{
  "sabloane": { "teleport": [
    { "tasta": "4" }, { "pauza": 2000 }, { "tasta": "/" },
    { "text": "$nume" }, { "tasta": "Return" } ] },
  "arbore": [ { "categorie": "Bloodlines", "id": "bloodlines", "copii": [
    { "categorie": "Rift", "id": "rift", "copii": [
      { "categorie": "Snow", "id": "rift-snow", "culoare": "#378ADD", "copii": [
        { "macro": "Snow 2", "id": "rift-snow-2", "descriere": "outside of Snow Village", "sablon": "teleport" } ] } ] } ] } ]
}
EOT

# ydotool cere profilul plat al mouse-ului pentru mișcarea de 1:1. Proba îl pune și îl reface la ieșire.
for s in $(qdbus6 org.kde.KWin /org/kde/KWin/InputDevice org.kde.KWin.InputDeviceManager.devicesSysNames); do
    [ "$(busctl --user get-property org.kde.KWin "/org/kde/KWin/InputDevice/$s" org.kde.KWin.InputDevice name)" = 's "ydotoold virtual device"' ] && DEV=$s
done
[ -n "$DEV" ] || { echo "✗ KWin nu vede dispozitivul ydotoold"; exit 1; }
CALE=/org/kde/KWin/InputDevice/$DEV
proprietate() { busctl --user get-property org.kde.KWin "$CALE" org.kde.KWin.InputDevice "$1" | cut -d' ' -f2; }
seteaza() { busctl --user set-property org.kde.KWin "$CALE" org.kde.KWin.InputDevice "$1" b "$2"; }
PLAT=$(proprietate pointerAccelerationProfileFlat)
ADAPTIV=$(proprietate pointerAccelerationProfileAdaptive)
LOC=$(kreadconfig6 --file "$REGULI" --group bisonus-macro --key position)
curata() {
    ydotool click 0x80 >/dev/null 2>&1 # butonul nu rămâne apăsat după o cădere în mijlocul tragerii
    [ "$PLAT" = true ] && seteaza pointerAccelerationProfileFlat true
    [ "$ADAPTIV" = true ] && seteaza pointerAccelerationProfileAdaptive true
    [ -n "$APP" ] && kill "$APP" 2>/dev/null
    [ -n "$TINTA" ] && kill "$TINTA" 2>/dev/null
    [ -n "$ALTA" ] && kill "$ALTA" 2>/dev/null
    wait 2>/dev/null
    for s in $(seq 0 20); do scripting unloadScript "bisonus-macro-ridicare-$s" >/dev/null 2>&1; done
    scripting unloadScript bisonus-macro >/dev/null 2>&1 # kill nu-l lasă pe aplicație să-l descarce
    scripting unloadScript bisonus-proba >/dev/null 2>&1
    sleep 0.5 # KWin scrie locul ținut minte la închiderea panoului: locul omului se reface după el
    if [ -n "$LOC" ]; then
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position "$LOC"
    else
        kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position --delete
    fi
    qdbus6 org.kde.KWin /KWin reconfigure
    goleste proba-rama
    true
}
trap curata EXIT
seteaza pointerAccelerationProfileFlat true
kwriteconfig6 --file "$REGULI" --group bisonus-macro --key position 40,40
qdbus6 org.kde.KWin /KWin reconfigure
sleep 1

# rezerva <mesaj>: o cădere care, cu rama, alege rezerva
rezerva() { echo "✗ $1"; [ "$RAMA" = 1 ] && exit 2; exit 1; }
# du <x> <y>: cursorul la punctul dat. ydotool 1.0.4: --absolute într-o singură mișcare ajunge greșit, deci colțul 0,0
# întâi, apoi mișcarea relativă.
du() { ydotool mousemove --absolute -x 0 -y 0; sleep 0.3; ydotool mousemove -x "$1" -y "$2"; sleep 0.3; }
clic() { du "$1" "$2"; ydotool click 0xC0; sleep 1.5; }
# trage <x> <y> <dx> <dy>: apasă la x,y, mută cu dx,dy și eliberează
trage() { du "$1" "$2"; ydotool click 0x40; sleep 0.3; ydotool mousemove -x "$3" -y "$4"; sleep 0.3; ydotool click 0x80; sleep 1.5; }
# bara_sus: citește locul panoului. FX FY FW FH: rama; CX CY CW CH: conținutul; SUS: înălțimea barei de sus (rama, sau
# bara de titlu a rezervei); B: marginea ramei; YB: mijlocul barei de sus; XI: centrul lui ✕; P: pasul butoanelor.
bara_sus() {
    read -r FX FY FW FH <<< "$(panou_cheie "$GEOM_RAMA")"
    read -r CX CY CW CH <<< "$(panou_cheie "$GEOM_CONTINUT")"
    [ -n "${CH:-}" ] || { echo "✗ KWin nu dă locul panoului: $D/tinta.log"; exit 1; }
    SUS=$((CY + TITLU_APP - FY))
    B=$((CX - FX))
    YB=$((FY + SUS / 2))
    XI=$((FX + FW - ${X_INCHIDE:-$((SUS / 2 + B))}))
    P=${PAS:-$SUS}
}
# captura <fișier> <x> <y> <lățime> <înălțime>: o bucată din ecran
captura() {
    spectacle -b -n -f -o "$PWD/$D/ecran.png"
    sleep 1
    python3 - "$PWD/$D/ecran.png" "$PWD/$D/$1" "$2" "$3" "$4" "$5" <<'PY'
import sys
from PIL import Image
x, y, w, h = (int(v) for v in sys.argv[3:7])
Image.open(sys.argv[1]).crop((x, y, x + w, y + h)).save(sys.argv[2])
PY
}
# la_fel <a> <b>: cele două capturi au aceiași pixeli
la_fel() {
    python3 - "$PWD/$D/$1" "$PWD/$D/$2" <<'PY'
import sys
from PIL import Image, ImageChops
a, b = (Image.open(p).convert("RGB") for p in sys.argv[1:3])
sys.exit(0 if a.size == b.size and ImageChops.difference(a, b).getbbox() is None else 1)
PY
}
# tava_activeaza: clicul pe iconița din tavă. KDE ține tava aplicației pe o conexiune a ei, fără numele
# org.kde.StatusNotifierItem-<pid>-1, deci o caut după pid și după obiectul /StatusNotifierItem.
tava_activeaza() {
    local n
    for n in $(busctl --user list --no-legend | awk -v p="$APP" '$2 == p && $1 ~ /^:/ {print $1}'); do
        if busctl --user tree "$n" 2>/dev/null | grep -q '/StatusNotifierItem'; then
            busctl --user call "$n" /StatusNotifierItem org.kde.StatusNotifierItem Activate ii 0 0 >/dev/null && return 0
        fi
    done
    return 1
}
salvata() { sed -n 's/^panou: mărime salvată \([0-9]*\)x\([0-9]*\)$/\1/p' "$D/aplicatia.log" | tail -1; }

# ținta pornește prima: ține numele win.bisonus.proba, prin care KWin răspunde probei
build/fereastra-tinta -1 fereastra-tinta 120000 > "$D/tinta.log" 2>&1 &
TINTA=$!
export KWIN_RECEPTOR="$D/tinta.log"
for _ in $(seq 50); do grep -qx activ "$D/tinta.log" && break; sleep 0.1; done
grep -qx activ "$D/tinta.log" || { echo "✗ fereastra țintă n-a primit focusul"; exit 1; }

[ -s "$ACORD" ] || echo "→ KDE arată fereastra de acord pentru tastatură: acceptă-o"
BISONUS_COMPONENTA=proba-rama XDG_CONFIG_HOME="$PWD/$D/config" build/bisonus-macro > "$D/aplicatia.log" 2>&1 &
APP=$!
for _ in $(seq $((ASTEPTARE * 10))); do grep -q 'tastatura EIS: gata' "$D/aplicatia.log" && break; sleep 0.1; done
grep -q 'tastatura EIS: gata' "$D/aplicatia.log" || { echo "✗ aplicația n-a primit tastatura EIS în $ASTEPTARE s: $D/aplicatia.log"; exit 1; }
JETON=$(sed -n 's/^restore_token=//p' "$D/config/bisonus-macrorc")
[ -n "$JETON" ] && echo "$JETON" > "$ACORD"
for _ in $(seq 50); do [ "$(scripting isScriptLoaded bisonus-macro)" = true ] && break; sleep 0.1; done
[ "$(scripting isScriptLoaded bisonus-macro)" = true ] || { echo "✗ aplicația n-a încărcat scriptul în KWin: $D/aplicatia.log"; exit 1; }
sleep 1
du 700 600 # cursorul stă departe de panou: capturile nu-l prind

# 1. titlurile și straturile
TOATE="$PANOU_JS raport(\"toate \" + JSON.stringify(workspace.windowList().filter(w => w.resourceClass === \"bisonus-macro\").map(w => [w.caption, w.caption.length])));"
activeaza fereastra-tinta
sleep 1
[ "$(panou_cheie 'panou.caption === titluDoi')" = true ] || { echo "✗ 1: cu ținta activă, KWin nu vede titlul al doilea. Ferestrele aplicației, cu titlul și lungimea: $(intreaba toate "$TOATE")"; exit 3; }
[ "$(panou_cheie panou.layer)" = 6 ] || { echo "✗ 1: cu ținta activă, panoul are stratul $(panou_cheie panou.layer), nu 6: regula nu prinde titlul al doilea"; exit 3; }
bara_sus
if [ "$RAMA" = 1 ]; then
    [ "$SUS" -ge 10 ] || rezerva "1: panoul n-are ramă: rama are $FX,$FY ${FW}x$FH, conținutul $CX,$CY ${CW}x$CH"
    captura titlu-2.png "$FX" "$FY" "$FW" "$SUS"
    captura lista.png "$CX" $((CY + 38)) "$CW" $((CH - 38 - 26))
else
    [ "$FX $FY $FW $FH" = "$CX $CY $CW $CH" ] || { echo "✗ 1: cu rezerva, panoul are ramă: $FX,$FY ${FW}x$FH față de $CX,$CY ${CW}x$CH"; exit 1; }
fi
build/fereastra-tinta -1 fereastra-alta 120000 > "$D/alta.log" 2>&1 &
ALTA=$!
for _ in $(seq 50); do grep -qx activ "$D/alta.log" && break; sleep 0.1; done
sleep 1
[ "$(panou_cheie 'panou.caption === titluUnu')" = true ] || { echo "✗ 1: în altă fereastră, panoul nu are titlul BisonusMacro: $(intreaba toate "$TOATE")"; exit 1; }
[ "$(panou_cheie panou.layer)" = 2 ] || { echo "✗ 1: în altă fereastră, panoul are stratul $(panou_cheie panou.layer), nu 2"; exit 1; }
if [ "$RAMA" = 1 ]; then
    captura titlu-1.png "$FX" "$FY" "$FW" "$SUS"
    cp "$D/titlu-1.png" "$D/rama.png" # pentru măsurarea butoanelor, dacă □ nu răspunde
    la_fel titlu-1.png titlu-2.png || { echo "✗ 1: rama arată altfel cu titlul al doilea: $D/titlu-1.png, $D/titlu-2.png"; exit 3; }
fi
echo "✓ 1: titlul al doilea are stratul 6, iar BisonusMacro are stratul 2$([ "$RAMA" = 1 ] && echo ', cu rama la fel în ambele')"

# 2. tragerea barei de sus, cu ținta activă și panoul fără focus
activeaza fereastra-tinta
sleep 1
bara_sus
X0=$FX
Y0=$FY
trage $((FX + B + 60)) "$YB" 100 50
bara_sus
[ $((FX - X0)) -ge 90 ] && [ $((FX - X0)) -le 110 ] && [ $((FY - Y0)) -ge 40 ] && [ $((FY - Y0)) -le 60 ] || rezerva "2: tragerea a dus panoul de la $X0,$Y0 la $FX,$FY, nu cu 100 px și 50 px"
[ "$(activa)" = fereastra-tinta ] || rezerva "2: după tragere, fereastra activă e $(activa), nu ținta"
echo "✓ 2: tragerea barei de sus a mutat panoul cu $((FX - X0)) px și $((FY - Y0)) px, iar ținta a rămas activă"

# 3. marginea din dreapta: în marginea ramei, în afara ei când rama n-are margine, sau la 3 px în panoul rezervei
L0=$(salvata)
[ -n "$L0" ] || L0=$CW
if [ "$RAMA" = 0 ]; then XM=$((CX + CW - 3)); elif [ "$B" -ge 2 ]; then XM=$((FX + FW - 1 - B / 2)); else XM=$((FX + FW + 2)); fi
trage "$XM" $((CY + TITLU_APP + 100)) 80 0
L=$(salvata)
[ -n "$L" ] && [ "$L" -ge $((L0 + 60)) ] || rezerva "3: după marginea trasă la x=$XM, lățimea ținută minte e „${L:-}”, nu cel puțin $((L0 + 60))"
echo "✓ 3: marginea din dreapta a lățit panoul de la $L0 px la $L px"

# 4. □ mărește, al doilea clic pune panoul la loc
bara_sus
W0=$FW
clic $((XI - P)) "$YB"
bara_sus
[ "$FW" -ge $((W0 + 200)) ] || rezerva "4: după □ la $((XI - P)),$YB, panoul are $FW px, nu cel puțin $((W0 + 200)). Măsoară butoanele pe $D/rama.png și pune X_INCHIDE și PAS"
clic $((XI - P)) "$YB"
bara_sus
[ "$FW" -le $((W0 + 4)) ] && [ "$FW" -ge $((W0 - 4)) ] || rezerva "4: după al doilea clic pe □, panoul are $FW px, nu $W0"
echo "✓ 4: □ a mărit panoul, iar al doilea clic l-a pus la loc"

# 5. – scoate panoul din KWin, clicul pe tavă îl aduce înapoi
clic $((XI - 2 * P)) "$YB"
M=$(panou_cheie panou.minimized)
if [ "$M" = true ]; then
    [ "$MINIMIZARE_PRINSA" = 1 ] && { echo "✗ 5: – a lăsat panoul minimizat: scriptul n-a prins minimizedChanged"; exit 4; }
elif [ "$M" != lipsa ]; then
    rezerva "5: – la $((XI - 2 * P)),$YB n-a răspuns: panoul e tot în KWin, neminimizat"
elif [ "$RAMA" = 1 ] && ! grep -q '^panou: minimizat, ascuns în tavă$' "$D/aplicatia.log"; then
    echo "✗ 5: panoul a plecat din KWin fără „panou: minimizat”: $D/aplicatia.log"; exit 1
fi
tava_activeaza || { echo "✗ 5: tava aplicației nu e pe D-Bus"; exit 1; }
sleep 1.5
[ "$(panou_cheie panou.minimized)" = false ] || { echo "✗ 5: după clicul pe tavă, panoul e „$(panou_cheie panou.minimized)”, nu arătat"; exit 1; }
echo "✓ 5: – a ascuns panoul$([ "$M" = true ] && echo ' prin minimizare'), iar clicul pe tavă l-a adus înapoi"

# 6. ✕ oprește aplicația
bara_sus
clic "$XI" "$YB"
for _ in $(seq 30); do kill -0 "$APP" 2>/dev/null || break; sleep 0.1; done
kill -0 "$APP" 2>/dev/null && rezerva "6: după ✕ la $XI,$YB, aplicația rulează"
wait "$APP" 2>/dev/null
APP=""
echo "✓ 6: ✕ a oprit aplicația"

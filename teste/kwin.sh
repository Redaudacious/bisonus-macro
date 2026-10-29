#!/bin/bash
# kwin.sh — ajutoarele probelor pentru KWin. Se încarcă cu `source teste/kwin.sh`, din rădăcina proiectului.
# Cere D-Bus-ul sesiunii. `uuid`, `intreaba` și ajutoarele de după el cer și KWIN_RECEPTOR: jurnalul ferestrei de probă
# care ține numele win.bisonus.proba, adică prima `build/fereastra-tinta` pornită.
KWIN_DOSAR=build/kwin
mkdir -p "$KWIN_DOSAR"

scripting() { qdbus6 org.kde.KWin /Scripting "org.kde.kwin.Scripting.$1" "${@:2}"; }

# ruleaza_script <nume> <cale .js>: descarcă scriptul cu același nume și așteaptă plecarea lui (necunoscuta 28),
# apoi îl încarcă și îl pornește. Tipărește ce întoarce loadScript: -1 e un refuz.
ruleaza_script() {
    scripting unloadScript "$1" >/dev/null
    for _ in $(seq 50); do [ "$(scripting isScriptLoaded "$1")" = false ] && break; sleep 0.02; done
    local id
    id=$(scripting loadScript "$PWD/$2" "$1")
    scripting start >/dev/null
    echo "$id"
}

# js <cod>: rulează codul în KWin, ca scriptul bisonus-proba, o singură dată
js() {
    printf '%s\n' "$1" > "$KWIN_DOSAR/proba.js"
    ruleaza_script bisonus-proba "$KWIN_DOSAR/proba.js" >/dev/null
}

# uuid <clasa> <titlu>: uuid-ul ferestrei, cu acolade, sau gol
uuid() {
    local n
    n=$(grep -c '^kwin: uuid ' "$KWIN_RECEPTOR")
    js "for (const w of workspace.windowList()) if (w.resourceClass === \"$1\" && w.caption === \"$2\") callDBus(\"win.bisonus.proba\", \"/BisonusMacro\", \"win.bisonus.BisonusMacro\", \"raport\", \"uuid \" + w.internalId.toString());"
    for _ in $(seq 40); do [ "$(grep -c '^kwin: uuid ' "$KWIN_RECEPTOR")" -gt "$n" ] && break; sleep 0.05; done
    grep '^kwin: uuid ' "$KWIN_RECEPTOR" | tail -1 | cut -d' ' -f3
}

# info <uuid> <cheie>: o cheie din getWindowInfo, de pildă minimized, layer, x, y, width, height
info() { qdbus6 org.kde.KWin /KWin org.kde.KWin.getWindowInfo "$1" | sed -n "s/^$2: //p"; }

# activeaza <clasa>: fereastra normală cu clasa dată devine activă, scoasă din minimizare
activeaza() {
    js "for (const w of workspace.windowList()) if (w.resourceClass === \"$1\" && w.normalWindow) workspace.activeWindow = w;"
}

# Varianta aleasă de prima sarcină a planului 17. RAMA=1: rama e a lui KWin. RAMA=0: rezerva, cu bara de titlu
# desenată de aplicație în conținut, de TITLU_APP px. MINIMIZARE_PRINSA=0: – lasă panoul minimizat, iar clicul pe
# tavă îl reface (rezerva necunoscutei 45).
RAMA=1
TITLU_APP=0
MINIMIZARE_PRINSA=1
# Al doilea titlu al panoului, scris ca text dintr-un literal JS (hotărârea 64): U+2060 urmat de numele aplicației
TITLU_DOI=$(printf '\xe2\x81\xa0BisonusMacro')

# PANOU_JS: începutul codului unei întrebări. `panou` e fereastra panoului, în oricare titlu, sau null; `raport`
# trimite un rând în jurnalul receptorului.
PANOU_JS='const titluUnu = "BisonusMacro"; const titluDoi = "'"$TITLU_DOI"'"; const panou = workspace.windowList().find(w => w.resourceClass === "bisonus-macro" && (w.caption === titluUnu || w.caption === titluDoi)) || null; function raport(t) { callDBus("win.bisonus.proba", "/BisonusMacro", "win.bisonus.BisonusMacro", "raport", t); }'

# intreaba <eticheta> <cod>: rulează codul, care trimite „<eticheta> …” cu raport, și tipărește restul rândului nou din
# jurnalul receptorului. Fără rând nou în 2 secunde, nu tipărește nimic.
intreaba() {
    local n
    n=$(grep -c "^kwin: $1 " "$KWIN_RECEPTOR")
    js "$2"
    for _ in $(seq 40); do [ "$(grep -c "^kwin: $1 " "$KWIN_RECEPTOR")" -gt "$n" ] && break; sleep 0.05; done
    [ "$(grep -c "^kwin: $1 " "$KWIN_RECEPTOR")" -gt "$n" ] && grep "^kwin: $1 " "$KWIN_RECEPTOR" | tail -1 | cut -d' ' -f3-
}

# panou_cheie <expresie JS>: valoarea expresiei pentru `panou`, sau „lipsa” cât KWin nu are panoul, de pildă ascuns în tavă
panou_cheie() { intreaba cheie "$PANOU_JS raport(\"cheie \" + (panou ? ($1) : \"lipsa\"));"; }

# Locul panoului, „x y lățime înălțime”: cu tot cu rama, apoi numai conținutul
GEOM_RAMA='[panou.frameGeometry.x, panou.frameGeometry.y, panou.frameGeometry.width, panou.frameGeometry.height].map(Math.round).join(" ")'
GEOM_CONTINUT='[panou.clientGeometry.x, panou.clientGeometry.y, panou.clientGeometry.width, panou.clientGeometry.height].map(Math.round).join(" ")'

# panou_colt: colțul din stânga sus al barei de unelte, „x y”. De aici probele numără bara, titlurile și rândurile.
panou_colt() { panou_cheie "[Math.round(panou.clientGeometry.x), Math.round(panou.clientGeometry.y) + $TITLU_APP].join(\" \")"; }

# activa: clasa ferestrei active, sau „nimic”
activa() { intreaba activa "$PANOU_JS raport(\"activa \" + (workspace.activeWindow ? workspace.activeWindow.resourceClass : \"nimic\"));"; }

#!/usr/bin/env bash
# Construiește pachetul .deb în debian:sid din 2026-08-03 (Qt 6.10.2, vezi comun.sh), cu CPack, și îl lasă în build-pachete/.
# Folosire: bash packaging/build-deb.sh, din orice folder.
set -euo pipefail
cd "$(dirname "$0")/.."
. packaging/comun.sh

mkdir -p build-pachete
rm -f build-pachete/bisonus-macro_*.deb
# Sursa intră numai de citit. Construcția stă în /tmp/b din container, ca build/ al laptopului să rămână neatins.
$CT run --rm -e APT_SNAPSHOT="$APT_SNAPSHOT" -v "$PWD":/src:ro -v "$PWD/build-pachete":/out "$IMG_DEB" bash -euc '
  export DEBIAN_FRONTEND=noninteractive
  eval "$APT_SNAPSHOT"
  apt-get update -qq
  apt-get install -y -qq --no-install-recommends \
    cmake g++ make extra-cmake-modules qt6-base-dev \
    libkf6globalaccel-dev libkf6xmlgui-dev libkf6config-dev libkf6dbusaddons-dev libkf6i18n-dev \
    libkf6colorscheme-dev libkf6statusnotifieritem-dev libkf6widgetsaddons-dev \
    libei-dev pkgconf gettext dpkg-dev file >/dev/null
  cmake -B /tmp/b -S /src -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build /tmp/b -j"$(nproc)" >/dev/null
  cmake --build /tmp/b --target package >/dev/null
  cp /tmp/b/bisonus-macro_*_amd64.deb /out/'
ls build-pachete/bisonus-macro_"$VERSIUNE"_amd64.deb

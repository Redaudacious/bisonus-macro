#!/usr/bin/env bash
# Construiește pachetul Arch în archlinux:latest, cu makepkg, din copia de lucru, și îl lasă în build-pachete/.
# Folosire: bash packaging/build-arch.sh, din orice folder.
set -euo pipefail
cd "$(dirname "$0")/.."
. packaging/comun.sh

v_pkg=$(sed -n 's/^pkgver=//p' packaging/arch/PKGBUILD)
if [ "$v_pkg" != "$VERSIUNE" ]; then
  echo "FAIL: packaging/arch/PKGBUILD has pkgver $v_pkg, CMakeLists.txt has $VERSIUNE. Make them equal."
  exit 1
fi

mkdir -p build-pachete
rm -f build-pachete/bisonus-macro-*.pkg.tar.zst
# Root instalează dependențele din PKGBUILD, fără podeaua de versiune. makepkg refuză root, deci construiește b.
# Arhiva copiei de lucru poartă numele din source=(), deci makepkg n-o descarcă. Ea e făcută ca în .copr/Makefile.
# OPTIONS din makepkg.conf are debug: modelul de la cp lasă afară pachetul bisonus-macro-debug.
$CT run --rm -v "$PWD":/src:ro -v "$PWD/build-pachete":/out -e V="$VERSIUNE" "$IMG_ARCH" bash -euc '
  . /src/packaging/arch/PKGBUILD
  pacman -Syu --noconfirm --needed base-devel "${depends[@]%%[<>=]*}" "${makedepends[@]}" >/dev/null
  useradd -m b
  mkdir /home/b/w
  cp /src/packaging/arch/PKGBUILD /home/b/w/
  tar -C /src --exclude=./.git --exclude=./build --exclude=./build-pachete \
      --transform "s,^\.,bisonus-macro-$V," -czf "/home/b/w/bisonus-macro-$V.tar.gz" .
  chown -R b /home/b/w
  su b -c "cd ~/w && makepkg --noconfirm" >/dev/null
  cp /home/b/w/bisonus-macro-"$V"-*-x86_64.pkg.tar.zst /out/'
ls build-pachete/bisonus-macro-"$VERSIUNE"-*-x86_64.pkg.tar.zst

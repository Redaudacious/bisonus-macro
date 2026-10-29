#!/usr/bin/env bash
# Pregătește clona AUR a pachetului: PKGBUILD cu suma arhivei de pe GitHub și .SRCINFO. Omul comite și împinge.
# Folosire: bash packaging/aur.sh <clona AUR>, după ce eticheta v<versiune> e pe GitHub și depozitul e public.
# Laptopul n-are makepkg, deci updpkgsums și makepkg --printsrcinfo rulează în archlinux:latest.
set -euo pipefail
[ -n "${1:-}" ] || { echo "Usage: bash packaging/aur.sh <AUR clone folder>"; exit 1; }
dest=$(realpath "$1")
cd "$(dirname "$0")/.."
. packaging/comun.sh

if [ ! -e "$dest/.git" ]; then
  echo "FAIL: $dest is not a git clone. Clone it first: git -c init.defaultBranch=master clone ssh://aur@aur.archlinux.org/bisonus-macro.git $dest"
  exit 1
fi
url="https://github.com/Redaudacious/bisonus-macro/archive/refs/tags/v$VERSIUNE.tar.gz"
if ! curl -fsIL -o /dev/null "$url"; then
  echo "FAIL: $url is not reachable. Push the tag v$VERSIUNE and make the repository public first."
  exit 1
fi

cp packaging/arch/PKGBUILD "$dest/PKGBUILD"
# makepkg refuză root, deci b face suma și .SRCINFO. Root din container copiază rezultatul înapoi în clona omului.
$CT run --rm -v "$dest":/aur "$IMG_ARCH" bash -euc '
  pacman -Syu --noconfirm --needed pacman-contrib >/dev/null
  useradd -m b
  cp /aur/PKGBUILD /home/b/
  chown b /home/b/PKGBUILD
  su b -c "cd ~ && updpkgsums && makepkg --printsrcinfo > .SRCINFO"
  cp /home/b/PKGBUILD /home/b/.SRCINFO /aur/'
grep -q "^sha256sums=('[0-9a-f]\{64\}')" "$dest/PKGBUILD" || { echo "FAIL: PKGBUILD in $dest has no sha256 sum"; exit 1; }
echo "PASS: $dest has PKGBUILD and .SRCINFO for $VERSIUNE. Commit and push them."

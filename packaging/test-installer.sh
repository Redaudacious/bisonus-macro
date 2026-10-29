#!/usr/bin/env bash
# Proba instalatorului, în containere (specul, „Proba instalatorului”).
# sudo e înlocuit cu o comandă care scrie ce ar fi rulat, iar kwin_wayland cu una care tipărește „kwin 6.7.4”.
set -euo pipefail
cd "$(dirname "$0")/.."
. packaging/comun.sh

t=$(mktemp -d)
c=""
cod=0
trap '[ -z "$c" ] || $CT rm -f "$c" >/dev/null; rm -rf "$t"' EXIT
pica() { echo "FAIL: $*"; exit 1; }

# porneste <imaginea>: containerul cu utilizatorul u, cu sudo și kwin_wayland de probă, cu curl și gpg pe Debian
porneste() {
  [ -z "$c" ] || $CT rm -f "$c" >/dev/null
  c=$($CT run -d -v "$PWD/install.sh":/install.sh:ro "$1" sleep 3600)
  $CT exec "$c" bash -euc '
    if command -v apt-get >/dev/null; then
      apt-get update -qq
      DEBIAN_FRONTEND=noninteractive apt-get install -y -qq curl gpg >/dev/null
    elif command -v pacman >/dev/null; then
      # pacman -Si din instalator cere baza depozitelor
      pacman -Sy --noconfirm >/dev/null
    else
      command -v useradd >/dev/null || dnf install -y -q shadow-utils >/dev/null
    fi
    useradd -m u
    printf "#!/bin/sh\necho \"\$*\" >> /tmp/sudo.log\n" > /usr/local/bin/sudo
    printf "#!/bin/sh\necho kwin 6.7.4\n" > /usr/local/bin/kwin_wayland
    chmod 755 /usr/local/bin/sudo /usr/local/bin/kwin_wayland'
}

# ruleaza <sesiunea> [argumentele instalatorului]: tipărește ieșirea și pune codul în $cod
ruleaza() {
  local ses=$1
  shift
  set +e
  $CT exec --user u -w /home/u -e PATH=/usr/local/bin:/usr/bin:/bin -e XDG_SESSION_TYPE="$ses" \
    -e XDG_CURRENT_DESKTOP=KDE -e KDE_SESSION_VERSION=6 "$c" bash /install.sh "$@" < /dev/null > "$t/iesire" 2>&1
  cod=$?
  set -e
  cat "$t/iesire"
}

# proba <imaginea> <sesiunea> <codul> <textul cerut> [argumentele]: instalatorul iese cu codul cerut, numește textul,
# nu schimbă niciun fișier și nu cheamă sudo. Probele 2, 3, 4 și 5.
proba() {
  local img=$1 ses=$2 cod_cerut=$3 text=$4
  shift 4
  porneste "$img"
  $CT exec "$c" bash -c "$LISTA" > "$t/inainte"
  ruleaza "$ses" "$@"
  [ "$cod" = "$cod_cerut" ] || pica "$img, session $ses: exit code $cod, expected $cod_cerut"
  grep -qF -- "$text" "$t/iesire" || pica "$img, session $ses: the output does not name $text"
  $CT exec "$c" bash -c "$LISTA" > "$t/dupa"
  diff -q "$t/inainte" "$t/dupa" >/dev/null || pica "$img: the installer changed files: $(diff "$t/inainte" "$t/dupa")"
  $CT exec "$c" test ! -e /tmp/sudo.log || pica "$img: the installer called sudo"
  echo "ok: $img, session $ses, exit $cod_cerut, names $text, no change, no sudo"
}

# 1. --check iese cu 0 pe sistemele primite. Pe Arch și pe CachyOS, el numește familia Arch și comanda AUR.
for img in "$IMG_SID" "$IMG_RPM" "$IMG_ARCH" "$IMG_CACHY"; do
  porneste "$img"
  ruleaza wayland --check
  [ "$cod" = 0 ] || pica "$img: --check exit code $cod, expected 0"
  case $img in
    "$IMG_ARCH" | "$IMG_CACHY")
      grep -qF '(Arch family)' "$t/iesire" || pica "$img: --check does not name the Arch family"
      grep -qF 'paru -S bisonus-macro' "$t/iesire" || pica "$img: --check does not name the AUR command"
      ;;
  esac
  echo "ok: $img, --check, exit 0"
done

# 2. versiunile care lipsesc: KF6 6.24 în ubuntu:26.04, Qt 6.8.2 în debian:trixie (măsurate pe 28 octombrie 2026)
proba docker.io/library/ubuntu:26.04 wayland 1 'KF6 6.24'
proba docker.io/library/debian:trixie wayland 1 'Qt 6.8.2'
# 3. sesiunea X11
proba "$IMG_SID" x11 1 'Wayland session'
# 5. pe CachyOS, instalarea spune comenzile AUR și pacman -U, iese cu 0 și nu schimbă nimic
proba "$IMG_CACHY" wayland 0 'paru -S bisonus-macro'
grep -qF 'sudo pacman -U' "$t/iesire" || pica "$IMG_CACHY: the output does not name sudo pacman -U"

echo "PASS: test-installer"

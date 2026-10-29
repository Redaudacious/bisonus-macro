#!/usr/bin/env bash
# Proba instalării și a dezinstalării, într-un container curat (specul, „Proba instalării și a dezinstalării”).
# Folosire: bash packaging/test-install.sh deb|deb-qt610|rpm|arch|cachyos
# deb: în sid de azi. deb-qt610: în sid din 2026-08-04, cu Qt 6.10.2, ca podeaua din Depends să fie probată pe cea mai veche Qt acceptată.
# Pachetul vine din build-pachete/, făcut de build-deb.sh, build-rpm.sh sau build-arch.sh. Modul cachyos probează
# pachetul Arch în imaginea CachyOS.
set -euo pipefail
cd "$(dirname "$0")/.."
. packaging/comun.sh

fel=${1:-}
snap=
case $fel in
  deb) fam=deb; img=$IMG_SID; pachet=build-pachete/bisonus-macro_${VERSIUNE}_amd64.deb ;;
  deb-qt610) fam=deb; snap=1; img=$IMG_DEB; pachet=build-pachete/bisonus-macro_${VERSIUNE}_amd64.deb ;;
  rpm) fam=rpm; img=$IMG_RPM; pachet=$(ls build-pachete/bisonus-macro-"$VERSIUNE"-*.x86_64.rpm 2>/dev/null | head -n1 || true) ;;
  arch | cachyos)
    fam=pacman
    if [ "$fel" = arch ]; then img=$IMG_ARCH; else img=$IMG_CACHY; fi
    pachet=$(ls build-pachete/bisonus-macro-"$VERSIUNE"-*-x86_64.pkg.tar.zst 2>/dev/null | head -n1 || true)
    ;;
  *) echo "Usage: bash packaging/test-install.sh deb|deb-qt610|rpm|arch|cachyos"; exit 1 ;;
esac
if [ -z "$pachet" ] || [ ! -f "$pachet" ]; then
  echo "FAIL: no $fel package for $VERSIUNE in build-pachete/. Run packaging/build-${fam/pacman/arch}.sh first."
  exit 1
fi
nume=$(basename "$pachet")

t=$(mktemp -d)
c=""
trap '[ -z "$c" ] || $CT rm -f "$c" >/dev/null; rm -rf "$t"' EXIT
pica() { echo "FAIL: $*"; exit 1; }
ex() { $CT exec "$c" bash -c "$1"; }
start=$(date +%s)

c=$($CT run -d -v "$PWD/build-pachete":/pachete:ro "$img" sleep 3600)

# 1. lista dinainte
case $fam in
  deb)
    [ -z "$snap" ] || ex "$APT_SNAPSHOT"
    ex 'apt-get update -qq'
    # Imaginea din cache are pachete mai vechi decât oglinda: instalarea ar aduce biblioteci noi (libpcre2-8) care rămân după scoatere. Le aducem la zi înaintea listei.
    ex 'DEBIAN_FRONTEND=noninteractive apt-get upgrade -y -qq >/dev/null'
    # systemd-sysv are „Protected: yes": apt nu-l scoate nici cu autoremove, iar dependențele pachetului îl aduc. Intră în starea de dinainte.
    ex 'DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends systemd-sysv >/dev/null'
    ;;
  rpm)
    # La fel pe Fedora: dnf nu scoate systemd, iar dependențele pachetului îl aduc, cu dbus și bibliotecile lui.
    ex 'dnf install -y -q --setopt=install_weak_deps=False systemd >/dev/null'
    # La fel pe Fedora: imaginea veche ar lăsa după scoatere bibliotecile aduse la zi de instalare.
    ex 'dnf upgrade -y -q >/dev/null'
    ex "rpm -qa --qf '%{NAME}\n' | sort > /tmp/pachete0"
    ;;
  pacman)
    # La fel pe Arch: imaginea veche ar lăsa după scoatere bibliotecile aduse la zi de instalare.
    ex 'pacman -Syu --noconfirm >/dev/null'
    # fontconfig (scriptul de instalare scrie /etc/fonts/conf.d), shared-mime-info (hook-ul update-mime-database scrie XML-uri în /usr/share/mime)
    # și vapoursynth (scriptul de instalare face libvsscript.so) lasă fișiere pe care scoaterea nu le șterge. Le aducem înaintea listei, ca la systemd-sysv pe .deb.
    ex 'pacman -S --needed --noconfirm fontconfig shared-mime-info vapoursynth >/dev/null'
    ;;
esac
ex "$LISTA" > "$t/inainte"

# 2. instalarea din fișier
case $fam in
  deb) ex "DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends /pachete/$nume" >/dev/null ;;
  rpm) ex "dnf install -y -q --setopt=install_weak_deps=False /pachete/$nume" >/dev/null ;;
  pacman) ex "pacman -U --noconfirm /pachete/$nume" >/dev/null ;;
esac
echo "installed: $nume"
if [ -n "$snap" ]; then
  qt=$(ex 'dpkg-query -W -f="\${Version}" libqt6core6t64')
  case $qt in 6.10.2*) echo "Qt: $qt" ;; *) pica "expected Qt 6.10.2, found $qt" ;; esac
fi

# 3. dependențele puse de mână
case $fam in
  deb)
    dep=$(ex 'dpkg-query -W -f="\${Depends}" bisonus-macro')
    for d in xdg-desktop-portal-kde kwin-wayland; do
      grep -qw -- "$d" <<<"$dep" || pica "Depends has no $d: $dep"
    done
    ;;
  rpm)
    dep=$(ex 'rpm -q --requires bisonus-macro')
    for d in xdg-desktop-portal-kde kwin; do
      grep -qx -- "$d" <<<"$dep" || pica "Requires has no $d"
    done
    ;;
  pacman)
    dep=$(ex 'pacman -Qi bisonus-macro' | sed -n 's/^Depends On *: //p')
    for d in xdg-desktop-portal-kde kwin 'qt6-base>=6.10.2'; do
      tr -s ' ' '\n' <<<"$dep" | grep -qxF -- "$d" || pica "Depends On has no $d: $dep"
    done
    ;;
esac
echo "dependencies: ok"

# 4. pornirea fără desktop, cu HOME în /tmp, ca rularea să nu lase /root/.config în lista de după. Înaintea lui 0.7.0, binarul n-are --version, deci se cer numai bibliotecile.
if versiune_ge "$VERSIUNE" 0.7.0; then
  ex 'HOME=/tmp/acasa-proba QT_QPA_PLATFORM=offscreen timeout 20 bisonus-macro --version' | grep -qF "$VERSIUNE" \
    || pica "bisonus-macro --version does not print $VERSIUNE"
  echo "--version: ok"
else
  ex '/lib64/ld-linux-x86-64.so.2 --list /usr/bin/bisonus-macro' > "$t/biblioteci"
  if grep -q 'not found' "$t/biblioteci"; then pica "missing libraries: $(grep 'not found' "$t/biblioteci")"; fi
  echo "libraries: ok"
fi

# fișierele pachetului, citite înaintea scoaterii
case $fam in
  deb) ex 'dpkg -L bisonus-macro' > "$t/fisiere" ;;
  rpm) ex 'rpm -ql bisonus-macro' > "$t/fisiere" ;;
  pacman) ex 'pacman -Qlq bisonus-macro' > "$t/fisiere" ;;
esac

# 5. scoaterea, cu dependențele aduse de pachet. purge scoate și pachetul; remove urmat de purge pică, fiindcă pachetul fără conffiles dispare din bază după remove.
# Fără cele două opțiuni, autoremove păstrează systemd și dbus, fiindcă pachetele de bază ale imaginii le recomandă.
case $fam in
  deb)
    ex 'export DEBIAN_FRONTEND=noninteractive; apt-get purge -y -qq bisonus-macro && apt-get autoremove --purge -y -qq -o APT::AutoRemove::RecommendsImportant=false -o APT::AutoRemove::SuggestsImportant=false' >/dev/null
    ;;
  rpm)
    # dnf păstrează pachetele recomandate de systemd, deci scoatem exact pachetele aduse de instalare
    ex "rpm -qa --qf '%{NAME}\n' | sort > /tmp/pachete1; dnf remove -y -q \$(comm -13 /tmp/pachete0 /tmp/pachete1) && dnf autoremove -y -q" >/dev/null
    ;;
  pacman)
    # -s scoate dependențele instalate numai pentru pachet, iar -n scoate și fișierele .pacsave
    ex 'pacman -Rns --noconfirm bisonus-macro' >/dev/null
    ;;
esac
echo "removed"

# 6. niciun fișier al pachetului nu mai e pe disc. Dosarele comune, ca /usr/bin, rămân.
$CT exec -i "$c" bash -c 'while read -r f; do if [ -f "$f" ] || [ -L "$f" ]; then echo "$f"; fi; done' \
  < "$t/fisiere" > "$t/ramase"
[ ! -s "$t/ramase" ] || pica "package files left on disk: $(tr '\n' ' ' < "$t/ramase")"
ramas=$(ex 'find / -xdev -path /pachete -prune -o -iname "*bisonus*" -print 2>/dev/null')
[ -z "$ramas" ] || pica "paths with bisonus left on disk: $ramas"

# 7. lista de după e lista dinainte, afară de căile din test-install-permise.txt
ex "$LISTA" > "$t/dupa"
{ diff "$t/inainte" "$t/dupa" || true; } | sed -n 's/^[<>] //p' \
  | { grep -vxF -f <(grep -v '^#' packaging/test-install-permise.txt) || true; } > "$t/diferenta"
if [ -s "$t/diferenta" ]; then
  { diff "$t/inainte" "$t/dupa" || true; } | grep '^[<>] '
  pica "the file list changed after removal"
fi

echo "time: $(( $(date +%s) - start )) s"
echo "PASS: test-install $fel"

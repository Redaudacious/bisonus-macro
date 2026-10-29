#!/usr/bin/env bash
# BisonusMacro installer. It checks the system first. When a check fails, it changes nothing.
#
#   bash install.sh              check, then add the package repository and install (on Arch: show the install commands)
#   bash install.sh --check      only check
#   bash install.sh --uninstall  remove the KDE settings of BisonusMacro, the package and the repository
set -euo pipefail

REPO_URL="https://redaudacious.github.io/bisonus-macro"
COPR_PROJECT="redaudacious/bisonus-macro"
# The fingerprint of the key that signs the APT repository. It is empty until the first release.
KEY_FPR="6D1C97D4EEDA19753C0CC004A41807E85CB09C2C"
QT_MIN=6.10.2
KF6_MIN=6.30
KWIN_MIN=6.7

say() { printf '%s\n' "$*"; }
ok() { say "  ✓ $1"; }
bad() { say "  ✗ $1"; say "    Fix: $2"; failed=1; }
warn() { say "  ! $1"; say "    $2"; }
# ge A B: version A is B or newer
ge() { [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]; }
# plain "4:6.7.4-2+b1" prints "6.7.4"
plain() { local v=${1#*:}; printf '%s' "${v%%[-+~]*}"; }
candidate() { apt-cache policy "$1" 2>/dev/null | sed -n 's/^ *Candidate: //p' | head -n1 || true; }
# pacman_version qt6-base prints "6.12.0-3", from the package database, without root
pacman_version() { pacman -Si "$1" 2>/dev/null | sed -n 's/^Version *: //p' | head -n1 || true; }
confirm() {
  printf 'Continue? [y/N] '
  read -r answer || answer=""
  case $answer in
    y | Y | yes) ;;
    *) say "Nothing was changed."; exit 0 ;;
  esac
}

mode=install
case "${1:-}" in
  "") ;;
  --check) mode=check ;;
  --uninstall) mode=uninstall ;;
  *) say "Usage: bash install.sh [--check | --uninstall]"; exit 1 ;;
esac

if [ "$(id -u)" -eq 0 ]; then
  say "Run this installer as your user, not as root. It calls sudo only for the steps that write."
  exit 1
fi

# os-release is read with sed, not run as a shell file
os_file=/etc/os-release
[ -r "$os_file" ] || os_file=/usr/lib/os-release
if [ ! -r "$os_file" ]; then
  say "Cannot read /etc/os-release or /usr/lib/os-release. Nothing was changed."
  exit 1
fi
os_value() { sed -n "s/^$1=//p" "$os_file" | head -n1 | tr -d "\"'"; }
ID=$(os_value ID)
ID_LIKE=$(os_value ID_LIKE)
VERSION_ID=$(os_value VERSION_ID)
NAME=$(os_value PRETTY_NAME)
case " $ID $ID_LIKE " in
  *" fedora "*) family=fedora ;;
  *" debian "* | *" ubuntu "*) family=debian ;;
  # Arch, CachyOS (ID_LIKE=arch), Manjaro (ID_LIKE=arch) și celelalte derivate cu arch în ID_LIKE
  *" arch "*) family=arch ;;
  *) family="" ;;
esac

check() {
  failed=0
  say "Checking your system. This step changes nothing."

  # 1. the distribution
  case $family in
    debian) ok "Distribution: $NAME (Debian family)" ;;
    fedora) ok "Distribution: $NAME (Fedora family)" ;;
    arch) ok "Distribution: $NAME (Arch family)" ;;
    *) bad "Distribution: $NAME is not supported." "BisonusMacro has packages for Debian sid and forky, their derivatives, Fedora 44 or newer, Arch Linux and its derivatives." ;;
  esac

  # 2. the versions in the repository of the distribution
  if [ "$family" = debian ]; then
    local qt kf tool
    qt=$(plain "$(candidate libqt6core6t64)")
    kf=$(plain "$(candidate libkf6globalaccel6)")
    if ge "${qt:-0}" "$QT_MIN"; then ok "Qt $qt"; else
      bad "Qt ${qt:-none} is in your distribution. BisonusMacro needs Qt $QT_MIN or newer." "Use Debian sid or forky, or a derivative with Qt $QT_MIN or newer."
    fi
    if ge "${kf:-0}" "$KF6_MIN"; then ok "KF6 $kf"; else
      bad "KF6 ${kf:-none} is in your distribution. BisonusMacro needs KF6 $KF6_MIN or newer." "Use Debian sid or forky, or a derivative with KF6 $KF6_MIN or newer."
    fi
    for tool in curl gpg; do
      command -v "$tool" >/dev/null || bad "The command $tool is missing." "sudo apt install $tool"
    done
  elif [ "$family" = fedora ]; then
    case $VERSION_ID in
      '' | *[!0-9]*) bad "Fedora version '$VERSION_ID' is not a number." "Use Fedora 44 or newer." ;;
      *)
        if [ "$VERSION_ID" -ge 44 ]; then ok "Fedora $VERSION_ID"; else
          bad "Fedora $VERSION_ID is too old. BisonusMacro needs Fedora 44 or newer." "Upgrade to Fedora 44 or newer."
        fi
        ;;
    esac
  elif [ "$family" = arch ]; then
    local qt kf
    qt=$(plain "$(pacman_version qt6-base)")
    kf=$(plain "$(pacman_version kglobalaccel)")
    if ge "${qt:-0}" "$QT_MIN"; then ok "Qt $qt"; else
      bad "Qt ${qt:-none} is in your package database. BisonusMacro needs Qt $QT_MIN or newer." "Update your system: sudo pacman -Syu"
    fi
    if ge "${kf:-0}" "$KF6_MIN"; then ok "KF6 $kf"; else
      bad "KF6 ${kf:-none} is in your package database. BisonusMacro needs KF6 $KF6_MIN or newer." "Update your system: sudo pacman -Syu"
    fi
  fi

  # 3. the session
  if [ "${XDG_SESSION_TYPE:-}" = wayland ]; then ok "Wayland session"; else
    bad "The session is ${XDG_SESSION_TYPE:-unknown}. BisonusMacro needs a Wayland session." "Log out. On the login screen, choose \"Plasma (Wayland)\"."
  fi
  case ":${XDG_CURRENT_DESKTOP:-}:" in
    *:KDE:*) ok "KDE desktop" ;;
    *) bad "The desktop is ${XDG_CURRENT_DESKTOP:-unknown}. BisonusMacro needs KDE Plasma." "Install KDE Plasma 6 and log in to it." ;;
  esac
  if [ "${KDE_SESSION_VERSION:-}" = 6 ]; then ok "Plasma 6"; else
    bad "KDE_SESSION_VERSION is ${KDE_SESSION_VERSION:-not set}. BisonusMacro needs Plasma 6." "Upgrade to KDE Plasma 6."
  fi

  # 4. KWin, the version that was tested
  local kwin=""
  if command -v kwin_wayland >/dev/null; then
    kwin=$(kwin_wayland --version 2>/dev/null | awk '{print $2; exit}') || kwin=""
  fi
  if [ -n "$kwin" ] && ge "$kwin" "$KWIN_MIN"; then ok "KWin $kwin"; else
    bad "KWin ${kwin:-was not found}. BisonusMacro needs KWin $KWIN_MIN or newer." "Upgrade KDE Plasma to 6.7 or newer."
  fi


  [ "$failed" = 0 ]
}

install_debian() {
  if [ -z "$KEY_FPR" ]; then
    say "This installer has no signing key yet. Install the .deb file from the Releases page. Nothing was changed."
    exit 1
  fi
  local tmp fpr
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' EXIT
  mkdir -m 700 "$tmp/gnupg"
  curl -fsSL --proto '=https' --tlsv1.2 "$REPO_URL/bisonus-macro.asc" -o "$tmp/bisonus-macro.asc"
  fpr=$(GNUPGHOME="$tmp/gnupg" gpg --batch --show-keys --with-colons "$tmp/bisonus-macro.asc" | awk -F: '/^fpr/ {print $10; exit}')
  if [ "$fpr" != "$KEY_FPR" ]; then
    say "The downloaded key has the fingerprint $fpr, not $KEY_FPR. Nothing was changed."
    exit 1
  fi
  printf 'Types: deb\nURIs: %s/\nSuites: ./\nSigned-By: /etc/apt/keyrings/bisonus-macro.asc\n' "$REPO_URL" > "$tmp/bisonus-macro.sources"
  sudo install -D -m 0644 "$tmp/bisonus-macro.asc" /etc/apt/keyrings/bisonus-macro.asc
  sudo install -m 0644 "$tmp/bisonus-macro.sources" /etc/apt/sources.list.d/bisonus-macro.sources
  sudo apt-get update
  sudo apt-get install -y bisonus-macro
}

install_fedora() {
  sudo dnf install -y 'dnf-command(copr)'
  sudo dnf copr enable -y "$COPR_PROJECT"
  sudo dnf install -y bisonus-macro
}

uninstall() {
  if [ -z "$family" ]; then
    say "Distribution $NAME is not supported. Nothing was changed."
    exit 1
  fi
  say "The installer will do these steps:"
  say "  1. bisonus-macro --cleanup: remove the KWin rules, the KWin scripts and the global shortcuts of BisonusMacro"
  if [ "$family" = arch ]; then
    say "  2. sudo: remove the package"
  else
    say "  2. sudo: remove the package, the package repository and its key"
  fi
  confirm
  local installed=""
  case $family in
    debian) installed=$(dpkg-query -W -f='${Version}' bisonus-macro 2>/dev/null) || installed="" ;;
    fedora) installed=$(rpm -q --qf '%{VERSION}' bisonus-macro 2>/dev/null) || installed="" ;;
    arch) installed=$(pacman -Q bisonus-macro 2>/dev/null | awk '{print $2}') || installed="" ;;
  esac
  # Versions before 0.7.0 have no --cleanup. Started with it, they would open the application.
  if [ -n "$installed" ] && ge "$(plain "$installed")" 0.7.0; then
    bisonus-macro --cleanup || warn "bisonus-macro --cleanup failed." "Use the manual steps in the README, section \"Uninstall\"."
  else
    warn "BisonusMacro ${installed:-is not installed}: this version has no --cleanup." "Use the manual steps in the README, section \"Uninstall\"."
  fi
  case $family in
    debian)
      [ -z "$installed" ] || sudo apt-get remove -y bisonus-macro
      sudo rm -f /etc/apt/sources.list.d/bisonus-macro.sources /etc/apt/keyrings/bisonus-macro.asc
      sudo apt-get update
      ;;
    fedora)
      [ -z "$installed" ] || sudo dnf remove -y bisonus-macro
      sudo dnf copr remove -y "$COPR_PROJECT" || true
      ;;
    arch)
      [ -z "$installed" ] || sudo pacman -R --noconfirm bisonus-macro
      ;;
  esac
  say "BisonusMacro is removed."
}

if [ "$mode" = uninstall ]; then
  uninstall
  exit 0
fi

if ! check; then
  say ""
  say "Nothing was changed."
  exit 1
fi
# Arch has no package repository of BisonusMacro: the installer shows the commands and changes nothing
if [ "$family" = arch ]; then
  say "All checks passed."
  say "BisonusMacro has no package repository for Arch Linux. Install it in one of these ways:"
  say "  - from the AUR, with an AUR helper: paru -S bisonus-macro (or: yay -S bisonus-macro)"
  say "  - from the AUR, by hand: git clone https://aur.archlinux.org/bisonus-macro.git && cd bisonus-macro && makepkg -si"
  say "  - from the Releases page: download bisonus-macro-<version>-1-x86_64.pkg.tar.zst, then run: sudo pacman -U ./bisonus-macro-<version>-1-x86_64.pkg.tar.zst"
  say "Nothing was changed."
  exit 0
fi
if [ "$mode" = check ]; then
  say "All checks passed."
  exit 0
fi

say "The installer will do these steps:"
if [ "$family" = debian ]; then
  say "  1. download the signing key from $REPO_URL and compare its fingerprint"
  say "  2. sudo: write /etc/apt/keyrings/bisonus-macro.asc and /etc/apt/sources.list.d/bisonus-macro.sources"
  say "  3. sudo apt-get update"
  say "  4. sudo apt-get install bisonus-macro"
  confirm
  install_debian
else
  say "  1. sudo dnf install 'dnf-command(copr)'"
  say "  2. sudo dnf copr enable $COPR_PROJECT"
  say "  3. sudo dnf install bisonus-macro"
  confirm
  install_fedora
fi
say "BisonusMacro is installed. Start it from the application menu."
say "At the first start, KDE asks once to allow keyboard control. Allow it."

#!/usr/bin/env bash
# Proba depozitului APT (specul, „Depozitul APT”): apt update aduce versiunea nouă, iar un Packages schimbat e refuzat.
# Folosire: bash packaging/test-apt-repo.sh, după packaging/build-deb.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
. packaging/comun.sh

deb=build-pachete/bisonus-macro_${VERSIUNE}_amd64.deb
[ -f "$deb" ] || { echo "FAIL: $deb is missing. Run packaging/build-deb.sh first."; exit 1; }

t=$(mktemp -d)
c=""
srv=""
export GNUPGHOME="$t/gpg"
# 7. cheia de probă pleacă odată cu folderul temporar
trap '[ -z "$c" ] || $CT rm -f "$c" >/dev/null; [ -z "$srv" ] || kill "$srv" 2>/dev/null; gpgconf --kill gpg-agent 2>/dev/null; rm -rf "$t"' EXIT
pica() { echo "FAIL: $*"; exit 1; }
ex() { $CT exec "$c" bash -c "$1"; }
versiune() { ex "dpkg-query -W -f='\${Version}' bisonus-macro"; }

# 1. cheia de probă
mkdir -m 700 "$GNUPGHOME"
gpg --batch --pinentry-mode loopback --passphrase '' --quick-gen-key 'BisonusMacro test key <test@invalid>' ed25519 sign never 2>/dev/null
fpr=$(gpg --batch --with-colons --list-secret-keys | awk -F: '/^fpr/ {print $10; exit}')

# 2. depozitul cu prima versiune, servit pe laptop
mkdir "$t/debs"
cp "$deb" "$t/debs/"
bash packaging/apt-repo.sh "$t/debs" "$t/site" "$fpr"
port=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
python3 -m http.server -b 127.0.0.1 -d "$t/site" "$port" >/dev/null 2>&1 &
srv=$!
sleep 1

# 3. sursa, apt-get update și instalarea, în debian:sid
c=$($CT run -d --network host "$IMG_SID" sleep 3600)
ex 'mkdir -p /etc/apt/keyrings'
$CT cp "$t/site/bisonus-macro.asc" "$c":/etc/apt/keyrings/bisonus-macro.asc
ex 'chmod 644 /etc/apt/keyrings/bisonus-macro.asc'
ex "printf 'Types: deb\nURIs: http://127.0.0.1:$port/\nSuites: ./\nSigned-By: /etc/apt/keyrings/bisonus-macro.asc\n' > /etc/apt/sources.list.d/bisonus-macro.sources"
ex 'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends bisonus-macro >/dev/null'
v1=$(versiune)
[ "$v1" = "$VERSIUNE" ] || pica "installed $v1, expected $VERSIUNE"
echo "installed from the repository: $v1"

# 4. versiunea +proba1, publicată peste prima
dpkg-deb -R "$deb" "$t/x"
sed -i "s/^Version: .*/Version: ${VERSIUNE}+proba1/" "$t/x/DEBIAN/control"
rm "$t/debs"/*.deb
dpkg-deb -b --root-owner-group "$t/x" "$t/debs/bisonus-macro_${VERSIUNE}+proba1_amd64.deb" >/dev/null
rm -rf "$t/site"
bash packaging/apt-repo.sh "$t/debs" "$t/site" "$fpr"

# 5. apt-get update și apt-get upgrade aduc versiunea nouă
ex 'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get upgrade -y -qq >/dev/null'
v2=$(versiune)
[ "$v2" = "${VERSIUNE}+proba1" ] || pica "after upgrade $v2, expected ${VERSIUNE}+proba1"
echo "upgraded: $v2"

# 6. un octet schimbat în Packages: apt-get update trebuie să refuze depozitul
printf 'X' | dd of="$t/site/Packages" bs=1 count=1 conv=notrunc 2>/dev/null
ex 'rm -f /var/lib/apt/lists/127.0.0.1*'
if ex 'apt-get update -qq' > "$t/update" 2>&1; then
  pica "apt-get update accepted a changed Packages file"
fi
grep -m1 -i 'mismatch\|hash\|sum' "$t/update" || true
echo "changed Packages: refused"

echo "PASS: test-apt-repo"

#!/usr/bin/env bash
# Construiește pachetul .rpm în fedora:44, prin .copr/Makefile, și îl lasă în build-pachete/.
# Folosire: bash packaging/build-rpm.sh, din orice folder.
set -euo pipefail
cd "$(dirname "$0")/.."
. packaging/comun.sh

v_spec=$(sed -n 's/^Version: *//p' packaging/bisonus-macro.spec)
if [ "$v_spec" != "$VERSIUNE" ]; then
  echo "FAIL: packaging/bisonus-macro.spec has Version $v_spec, CMakeLists.txt has $VERSIUNE. Make them equal."
  exit 1
fi

mkdir -p build-pachete
rm -f build-pachete/bisonus-macro-*.rpm
$CT run --rm -v "$PWD":/src:ro -v "$PWD/build-pachete":/out "$IMG_RPM" bash -euc '
  dnf install -y -q make rpm-build tar gzip "dnf-command(builddep)" >/dev/null
  make -f /src/.copr/Makefile srpm outdir=/tmp/srpm spec=/src/packaging/bisonus-macro.spec >/dev/null
  dnf builddep -y -q /tmp/srpm/bisonus-macro-*.src.rpm >/dev/null
  rpmbuild --rebuild --define "_rpmdir /tmp/rpm" /tmp/srpm/bisonus-macro-*.src.rpm >/dev/null
  cp /tmp/rpm/x86_64/bisonus-macro-[0-9]*.x86_64.rpm /out/'
ls build-pachete/bisonus-macro-"$VERSIUNE"-*.x86_64.rpm

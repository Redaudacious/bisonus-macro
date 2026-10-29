#!/usr/bin/env bash
# Tipărește secțiunea unei versiuni din CHANGELOG.md, fără titlul ei: notele Release-ului.
# Folosire: bash packaging/note-versiune.sh <versiune, cu sau fără v> [CHANGELOG.md]
set -euo pipefail
versiune=${1:?versiunea lipsește}
versiune=${versiune#v}
awk -v v="$versiune" '
  /^## / { if (activ) exit; activ = ($2 == v); next }
  activ { print }
' "${2:-CHANGELOG.md}" | sed -e '/./,$!d'

# Ce folosesc toate scripturile din packaging/. Se încarcă cu `. packaging/comun.sh`, din rădăcina proiectului.

# Unealta containerelor: podman, sau docker când podman lipsește. Workflow-ul pune CT=docker.
CT=${CT:-$(command -v podman >/dev/null && echo podman || echo docker)}
# Podeaua Qt a pachetului .deb e versiunea Qt din containerul de construcție. Qt 6.10.2 a fost în sid între 2026-08-09 și
# 2026-08-20. Imaginea datată și arhiva snapshot.debian.org de a doua zi dau
# aceeași stare a lui sid, deci construcția cere Qt 6.10.2 în Depends, nu 6.11. KF6 din arhivă e 6.28: podeaua lui KF6 în
# Depends vine din fișierele de simboluri și nu depinde de versiunea de construcție.
# Comanda care arată versiunea: curl -s https://snapshot.debian.org/mr/binary/libqt6core6t64/6.10.2+dfsg-15/binfiles
IMG_DEB=docker.io/library/debian:sid-20260803
SNAPSHOT_DEB=20260804T000000Z
APT_SNAPSHOT="rm -f /etc/apt/sources.list.d/*; echo 'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/$SNAPSHOT_DEB/ sid main' > /etc/apt/sources.list"
# sid de azi, pentru proba de instalare obișnuită
IMG_SID=docker.io/library/debian:sid
IMG_RPM=registry.fedoraproject.org/fedora:44
IMG_ARCH=docker.io/library/archlinux:latest
IMG_CACHY=docker.io/cachyos/cachyos:latest
VERSIUNE=$(sed -n 's/^project(bisonus-macro VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
# Fișierele sistemului, sortate, fără dosarele care se schimbă singure (specul, „Proba instalării”, pasul 1)
LISTA='find / -xdev \( -path /proc -o -path /sys -o -path /dev -o -path /run -o -path /tmp -o -path /var/lib -o -path /var/cache -o -path /var/log \) -prune -o -print | sort'

# versiune_ge A B: iese cu 0 când versiunea A e cel puțin B
versiune_ge() { [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]; }

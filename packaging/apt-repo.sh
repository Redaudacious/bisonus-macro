#!/usr/bin/env bash
# Face depozitul APT plat dintr-un folder de .deb-uri și îl semnează, după wiki-ul Debian, „DebianRepository/Format”.
# Folosire: bash packaging/apt-repo.sh <folderul .deb-urilor> <folderul depozitului> <amprenta cheii>
# Cheia stă în GNUPGHOME. APT_KEY_PASSPHRASE e parola ei, dacă are una.
set -euo pipefail
debs=${1:?Usage: apt-repo.sh <debs folder> <repository folder> <key fingerprint>}
out=${2:?Usage: apt-repo.sh <debs folder> <repository folder> <key fingerprint>}
key=${3:?Usage: apt-repo.sh <debs folder> <repository folder> <key fingerprint>}

mkdir -p "$out"
if [ -n "$(ls -A "$out")" ]; then echo "FAIL: $out is not empty."; exit 1; fi
cp "$debs"/*.deb "$out"/
cd "$out"

# Numai Packages, fără Packages.gz: apt ia fișierul semnat, iar proba octetului schimbat ține.
apt-ftparchive packages . > Packages
# Release nu se pune în folder cât apt-ftparchive îl scrie, altfel s-ar număra pe sine.
r=$(mktemp)
apt-ftparchive -o APT::FTPArchive::Release::Origin=BisonusMacro -o APT::FTPArchive::Release::Label=BisonusMacro release . > "$r"
mv "$r" Release
chmod 644 Release

g=(gpg --batch --yes --pinentry-mode loopback --passphrase "${APT_KEY_PASSPHRASE:-}" --local-user "$key")
"${g[@]}" --clearsign -o InRelease Release
"${g[@]}" --armor --detach-sign -o Release.gpg Release
gpg --batch --armor --export "$key" > bisonus-macro.asc
echo "repository: $out"

# COPR îl cheamă cu metoda make_srpm: make -f .copr/Makefile srpm outdir=<dosar> spec=<fișier .spec>.
# build-rpm.sh îl cheamă la fel, ca laptopul și COPR să facă același pachet.
top := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
spec ?= $(top)/packaging/bisonus-macro.spec
version = $(shell sed -n 's/^Version: *//p' $(spec))

srpm:
	command -v rpmbuild >/dev/null && command -v tar >/dev/null || dnf install -y rpm-build tar
	mkdir -p $(outdir)/sursa
	tar -C $(top) --exclude=./.git --exclude=./build --exclude=./build-pachete \
	    --transform 's,^\.,bisonus-macro-$(version),' -czf $(outdir)/sursa/bisonus-macro-$(version).tar.gz .
	rpmbuild -bs --define '_sourcedir $(outdir)/sursa' --define '_srcrpmdir $(outdir)' $(spec)

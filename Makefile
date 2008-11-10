# Hacky makefile to compile everything and run the tests in some kind of sane order.
# V=--verbose for verbose tests.

# This can be overridden on cmdline to generate pages elsewhere.
WEBDIR=~/www/html/ccan/

ALL=$(patsubst ccan/%/test, %, $(wildcard ccan/*/test))
ALL_DIRS=$(patsubst %, ccan/%, $(ALL))
ALL_DEPENDS=$(patsubst %, ccan/%/.depends, $(ALL))
ALL_PAGES=$(patsubst ccan/%, $(WEBDIR)/info/%.html, $(ALL_DIRS))
DIRECT_TARBALLS=$(patsubst ccan/%, $(WEBDIR)/tarballs/%.tar.bz2, $(ALL_DIRS))
DEPEND_TARBALLS=$(patsubst ccan/%, $(WEBDIR)/tarballs/with-deps/%.tar.bz2, $(ALL_DIRS))
WEB_SUBDIRS=$(WEBDIR)/tarballs $(WEBDIR)/tarballs/with-deps $(WEBDIR)/info

include Makefile-ccan

check: $(ALL_DIRS:%=test-%)

distclean: clean
	rm -f $(ALL_DEPENDS)
	rm -rf $(WEBDIR)

webpages: $(WEB_SUBDIRS) $(WEBDIR)/junkcode $(ALL_PAGES) $(WEBDIR)/list.html $(WEBDIR)/index.html $(WEBDIR)/upload.html $(WEBDIR)/uploader.php $(WEBDIR)/example-config.h $(WEBDIR)/ccan.jpg $(DIRECT_TARBALLS) $(DEPEND_TARBALLS) $(WEBDIR)/ccan.tar.bz2 $(WEBDIR)/Makefile-ccan

$(WEB_SUBDIRS):
	mkdir -p $@

$(WEBDIR)/junkcode:
	cp -a junkcode $@

# Override implicit attempt to link directory.
$(ALL_DIRS):
	@touch $@

$(WEBDIR)/ccan.tar.bz2:
	tar cvfj $@ `bzr ls --versioned --kind=file ccan`

$(ALL_PAGES): tools/doc_extract web/staticmoduleinfo.php

$(WEBDIR)/list.html: web/staticall.php tools/doc_extract $(DIRECT_TARBALLS) $(DEPEND_TARBALLS) $(WEBDIR)/ccan.tar.bz2
	php5 web/staticall.php ccan/ $(WEBDIR) > $@

$(WEBDIR)/upload.html: web/staticupload.php
	php5 web/staticupload.php > $@

# cpp inserts gratuitous linebreaks at start of file, makes for php problems.
$(WEBDIR)/uploader.php: web/uploader.php.cpp
	cpp -w -C -P $< | grep . > $@

$(WEBDIR)/index.html: web/staticindex.php
	php5 web/staticindex.php > $@

$(WEBDIR)/example-config.h: config.h
	cp $< $@

$(WEBDIR)/Makefile-ccan: Makefile-ccan
	cp $< $@

$(WEBDIR)/ccan.jpg: web/ccan.jpg
	cp $< $@

$(WEBDIR)/info/%.html: ccan/% ccan/%/test $(WEBDIR)/tarballs/%.tar.bz2 $(WEBDIR)/tarballs/with-deps/%.tar.bz2
	URLPREFIX=../ php5 web/staticmoduleinfo.php ccan/$* > $@

$(WEBDIR)/tarballs/%.tar.bz2: ccan/% ccan/%/test
	tar -c -v -j -f $@ `bzr ls --versioned --kind=file ccan/$*`

$(WEBDIR)/tarballs/with-deps/%.tar.bz2: ccan/% ccan/%/test tools/ccan_depends
	tar cvfj $@ $$(echo ccan/$* $$(tools/ccan_depends ccan/$*) | xargs -n 1 bzr ls --versioned --kind=file)

$(ALL_DEPENDS): %/.depends: tools/ccan_depends
	tools/ccan_depends $* > $@ || ( rm -f $@; exit 1 )

test-ccan/%: tools/run_tests ccan/%.o
	@echo Testing $*...
	@if tools/run_tests $(V) ccan/$* | grep ^'not ok'; then exit 1; else exit 0; fi

ccanlint: tools/ccanlint/ccanlint

clean: tools-clean
	$(RM) `find . -name '*.o'` `find . -name '.depends'` `find . -name '*.a'`  `find . -name _info`
	$(RM) inter-depends lib-depends test-depends

inter-depends: $(ALL_DEPENDS)
	for f in $(ALL_DEPENDS); do echo test-ccan/`basename \`dirname $$f\``: `sed -n 's,ccan/\(.*\),ccan/\1.o,p' < $$f`; done > $@

test-depends: $(ALL_DEPENDS)
	for f in $(ALL_DEPENDS); do echo test-ccan/`basename \`dirname $$f\``: `sed -n 's,ccan/\(.*\),test-ccan/\1,p' < $$f`; done > $@

include tools/Makefile
-include inter-depends
-include test-depends

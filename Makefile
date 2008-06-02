# Hacky makefile to compile everything and run the tests in some kind of sane order.
# V=--verbose for verbose tests.

CFLAGS=-O3 -Wall -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -Iccan -I.

ALL=$(patsubst ccan/%/test, ccan/%, $(wildcard ccan/*/test))
ALL_DEPENDS=$(patsubst %, %/.depends, $(ALL))

test-all: $(ALL_DEPENDS)
	$(MAKE) `for f in $(ALL); do echo test-$$f test-$$f; while read d; do echo test-$$d test-$$f; done < $$f/.depends; done | tsort`

distclean: clean
	rm -f */_info
	rm -f $(ALL_DEPENDS)

$(ALL_DEPENDS): %/.depends: %/_info
	@$< depends > $@ || ( rm -f $@; exit 1 )

test-ccan/%: tools/run_tests
	@echo Testing $*...
	@if tools/run_tests $(V) ccan/$* | grep ^'not ok'; then exit 1; else exit 0; fi

ccanlint: tools/ccanlint/ccanlint

clean: tools-clean
	rm -f `find . -name '*.o'` `find . -name '.depends'`

include tools/Makefile

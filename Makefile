# Hacky makefile to compile everything and run the tests in some kind of sane order.
# V=--verbose for verbose tests.

CFLAGS=-O3 -Wall -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -I.

ALL=$(patsubst %/test, %, $(wildcard */test))
ALL_DEPENDS=$(patsubst %, %/.depends, $(ALL))

test-all: $(ALL_DEPENDS)
	$(MAKE) `for f in $(ALL); do echo test-$$f test-$$f; while read d; do echo test-$$d test-$$f; done < $$f/.depends; done | tsort`

distclean: clean
	rm -f */_info
	rm -f $(ALL_DEPENDS)

$(ALL_DEPENDS): %/.depends: %/_info
	@$< depends > $@ || ( rm -f $@; exit 1 )

test-%: ccan_tools/run_tests
	@echo Testing $*...
	@if ccan_tools/run_tests $(V) $* | grep ^'not ok'; then exit 1; else exit 0; fi

clean: ccan_tools-clean
	rm -f `find . -name '*.o'`

include ccan_tools/Makefile

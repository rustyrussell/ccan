CFLAGS=-O3 -Wall -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -I.

ALL=$(patsubst %/test, %, $(wildcard */test))
ALL_DEPENDS=$(patsubst %, %/.depends, $(ALL))

default: test-all

test-all: $(ALL_DEPENDS)
	@$(MAKE) `for f in $(ALL); do echo test-$$f test-$$f; while read d; do echo test-$$d test-$$f; done < $$f/.depends; done | tsort`

$(ALL_DEPENDS): %/.depends: %/_info
	@$< depends > $@ || ( rm -f $@; exit 1 )

test-%: FORCE run_tests
	@echo Testing $*...
	@if ./run_tests $* | grep ^'not ok'; then exit 1; else exit 0; fi

FORCE:

run_tests: run_tests.o tap/tap.o talloc/talloc.o 

clean:
	rm -f run_tests run_tests.o

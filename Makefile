# Hacky makefile to compile everything and run the tests in some kind
# of sane order.

# Main targets:
# 
# check: run tests on all ccan modules (use 'make check V=--verbose' for more)
#        Includes building libccan.a.
# tools: build useful tools in tools/ dir.
#        Especially tools/ccanlint/ccanlint and tools/namespacize.
# distclean: destroy everything back to pristine state

ALL=$(patsubst ccan/%/test, %, $(wildcard ccan/*/test))
ALL_DIRS=$(patsubst %, ccan/%, $(ALL))
ALL_DEPENDS=$(patsubst %, ccan/%/.depends, $(ALL))

include Makefile-ccan

check: $(ALL_DIRS:%=test-%)
	echo $(ALL_DIRS)

distclean: clean
	rm -f $(ALL_DEPENDS)

# Override implicit attempt to link directory.
$(ALL_DIRS):
	@touch $@

$(ALL_DEPENDS): %/.depends: tools/ccan_depends
	tools/ccan_depends $* > $@ || ( rm -f $@; exit 1 )

test-ccan/%: tools/run_tests libccan.a(%.o)
	@echo Testing $*...
	@if tools/run_tests $(V) ccan/$* | grep ^'not ok'; then exit 1; else exit 0; fi

# Some don't have object files.
test-ccan/%:: tools/run_tests
	@echo Testing $*...
	@if tools/run_tests $(V) ccan/$* | grep ^'not ok'; then exit 1; else exit 0; fi

clean: tools-clean
	$(RM) `find . -name '*.o'` `find . -name '.depends'` `find . -name '*.a'`  `find . -name _info`
	$(RM) inter-depends lib-depends test-depends

# Only list a dependency if there are object files to build.
inter-depends: $(ALL_DEPENDS)
	for f in $(ALL_DEPENDS); do echo test-ccan/$$(basename $$(dirname $$f) ): $$(for dir in $$(cat $$f); do [ "$$(echo $$dir/[a-z]*.c)" = "$$dir/[a-z]*.c" ] || echo libccan.a\("$$(basename $$dir)".o\); done); done > $@

test-depends: $(ALL_DEPENDS)
	for f in $(ALL_DEPENDS); do echo test-ccan/`basename \`dirname $$f\``: `sed -n 's,ccan/\(.*\),test-ccan/\1,p' < $$f`; done > $@

include tools/Makefile
-include inter-depends
-include test-depends
-include Makefile-web

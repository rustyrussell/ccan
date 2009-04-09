# Hacky makefile to compile everything and run the tests in some kind
# of sane order.

# Main targets:
# 
# check: run tests on all ccan modules (use 'make check V=--verbose' for more)
#        Includes building libccan.a.
# libccan.a: A library with all the ccan modules in it.
# tools: build useful tools in tools/ dir.
#        Especially tools/ccanlint/ccanlint and tools/namespacize.
# distclean: destroy everything back to pristine state

# Anything with an _info.c file is a module.
ALL=$(patsubst ccan/%/_info.c, %, $(wildcard ccan/*/_info.c))
ALL_DEPENDS=$(patsubst %, ccan/%/.depends, $(ALL))
# Not all modules have tests.
ALL_TESTS=$(patsubst ccan/%/test/, %, $(wildcard ccan/*/test/))

default: libccan.a

include Makefile-ccan

check: $(ALL_TESTS:%=check-%)

distclean: clean
	rm -f $(ALL_DEPENDS)

$(ALL_DEPENDS): %/.depends: %/_info.c tools/ccan_depends
	@tools/ccan_depends $* > $@ || ( rm -f $@; exit 1 )

# Actual dependencies are created in inter-depends
check-%: tools/run_tests ccan/%/_info
	@echo Testing $*...
	@if tools/run_tests $(V) $$(for f in `ccan/$*/_info libs`; do echo --lib=$$f; done) `[ ! -f ccan/$*.o ] || echo --apiobj=ccan/$*.o` ccan/$* $(filter-out ccan/$*.o, $(filter %.o, $^)) | grep ^'not ok'; then exit 1; else exit 0; fi

ccan/%/_info: ccan/%/_info.c
	$(CC) $(CFLAGS) -o $@ $<

libccan.a(%.o): ccan/%.o
	$(AR) r $@ $<

clean: tools-clean
	$(RM) `find . -name '*.o'` `find . -name '.depends'` `find . -name '*.a'`  `find . -name _info` `find . -name '*.d'`
	$(RM) inter-depends lib-depends test-depends

# Creates a dependency from the tests to the object files which it needs.
inter-depends: $(ALL_DEPENDS) Makefile
	@for f in $(ALL_DEPENDS); do echo check-$$(basename $$(dirname $$f) ): $$(for dir in $$(cat $$f) $$(dirname $$f); do [ "$$(echo $$dir/[a-z]*.c)" = "$$dir/[a-z]*.c" ] || echo ccan/"$$(basename $$dir)".o; done); done > $@

# Creates dependencies between tests, so if foo depends on bar, bar is tested
# first 
test-depends: $(ALL_DEPENDS) Makefile
	@for f in $(ALL_DEPENDS); do echo check-`basename \`dirname $$f\``: `sed -n 's,ccan/\(.*\),check-\1,p' < $$f`; done > $@

include tools/Makefile
-include inter-depends
-include test-depends
-include Makefile-web

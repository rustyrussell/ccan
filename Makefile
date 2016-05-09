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

# Where make scores puts the results
SCOREDIR=scores/$(shell whoami)/$(shell uname -s)-$(shell uname -m)-$(CC)-$(shell git describe --always --dirty)
CCANLINT=tools/ccanlint/ccanlint --deps-fail-ignore
CCANLINT_FAST=$(CCANLINT) -x tests_pass_valgrind -x tests_compile_coverage

default: all_info libccan.a

ALL_DEPENDS=$(patsubst %, ccan/%/.depends, $(MODS))

# By default, we skip modules with external deps (or plaform specific)
MODS_EXCLUDE:=altstack generator jmap jset nfs ogg_to_pcm tal/talloc wwviaudio
# This randomly fails, and reliably fails under Jenkins :(
MODS_FLAKY:=altstack
MODS_RELIABLE=$(filter-out $(MODS_FLAKY),$(MODS))

include Makefile-ccan

fastcheck: $(MODS_RELIABLE:%=summary-fastcheck/%)

check: $(MODS_RELIABLE:%=summary-check/%)

distclean: clean
	rm -f $(ALL_DEPENDS)

scores: $(SCOREDIR)/SUMMARY

$(SCOREDIR)/SUMMARY: $(MODS:%=$(SCOREDIR)/%.score)
	git describe --always > $@
	uname -a >> $@
	$(CC) -v >> $@
	cat $^ | grep 'Total score:' >> $@

$(SCOREDIR)/%.score: ccan/%/_info tools/ccanlint/ccanlint $(OBJFILES)
	mkdir -p `dirname $@`
	$(CCANLINT) -v -s ccan/$* > $@ || true

$(ALL_DEPENDS): %/.depends: %/_info tools/ccan_depends
	tools/ccan_depends $* > $@ || ( rm -f $@; exit 1 )

# Actual dependencies are created in inter-depends
check/%: tools/ccanlint/ccanlint
	$(CCANLINT) ccan/$*

fastcheck/%: tools/ccanlint/ccanlint
	$(CCANLINT_FAST) ccan/$*

# Doesn't test dependencies, doesn't print verbose fail results.
summary-check/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT) -s ccan/$*

summary-fastcheck/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT_FAST) -s ccan/$*

ccan/%/info: ccan/%/_info config.h
	$(CC) $(CCAN_CFLAGS) -I. -o $@ -x c $<

all_info: $(MODS:%=ccan/%/info)

clean: tools-clean
	rm -f `find * -name '*.o'` `find * -name '.depends'` `find * -name '*.a'`  `find * -name info` `find * -name '*.d'` `find ccan -name '*-Makefile'`
	rm -f config.h
	rm -f inter-depends lib-depends test-depends

# Creates a dependency from the tests to the object files which it needs.
inter-depends: $(ALL_DEPENDS) Makefile
	for f in $(ALL_DEPENDS); do echo check-$$(basename $$(dirname $$f) ): $$(for dir in $$(cat $$f) $$(dirname $$f); do [ "$$(echo $$dir/*.c)" = "$$dir/*.c" ] || echo ccan/"$$(basename $$dir)".o; done); done > $@

# Creates dependencies between tests, so if foo depends on bar, bar is tested
# first 
test-depends: $(ALL_DEPENDS) Makefile
	for f in $(ALL_DEPENDS); do echo check/`basename \`dirname $$f\``: `sed -n 's,ccan/\(.*\),check/\1,p' < $$f`; done > $@

TAGS: FORCE
	find * -name '*.[ch]' | xargs etags

FORCE:

# Ensure we don't end up with empty file if configurator fails!
config.h: tools/configurator/configurator Makefile Makefile-ccan
	tools/configurator/configurator $(CC) $(CCAN_CFLAGS) > $@.tmp && mv $@.tmp $@

include tools/Makefile
-include inter-depends
-include test-depends
-include Makefile-web

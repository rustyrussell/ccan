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

default: libccan.a

ALL_DEPENDS=$(patsubst %, ccan/%/.depends, $(MODS))

# By default, we skip modules with external deps.
MODS_EXCLUDE:=jmap jset nfs ogg_to_pcm tal/talloc wwviaudio

include Makefile-ccan

fastcheck: $(MODS:%=summary-fastcheck-%)

check: $(MODS:%=summary-check-%)

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
check-%: tools/ccanlint/ccanlint
	$(CCANLINT) ccan/$*

fastcheck-%: tools/ccanlint/ccanlint
	$(CCANLINT_FAST) ccan/$*

# Doesn't test dependencies, doesn't print verbose fail results.
summary-check-%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT) -s ccan/$*

summary-fastcheck-%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT_FAST) -s ccan/$*

# FIXME: Horrible hacks because % doesn't match /
summary-check-antithread/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT) -s ccan/antithread/$*

summary-fastcheck-antithread/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT_FAST) -s ccan/antithread/$*

summary-check-tal/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT) -s ccan/tal/$*

summary-fastcheck-tal/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT_FAST) -s ccan/tal/$*

summary-check-str/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT) -s ccan/str/$*

summary-fastcheck-str/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT_FAST) -s ccan/str/$*

summary-check-crypto/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT) -s ccan/crypto/$*

summary-fastcheck-crypto/%: tools/ccanlint/ccanlint $(OBJFILES)
	$(CCANLINT_FAST) -s ccan/crypto/$*

ccan/%/info: ccan/%/_info
	$(CC) $(CCAN_CFLAGS) -o $@ -x c $<

clean: tools-clean
	$(RM) `find * -name '*.o'` `find * -name '.depends'` `find * -name '*.a'`  `find * -name info` `find * -name '*.d'`
	$(RM) inter-depends lib-depends test-depends ccan/*-Makefile

# Creates a dependency from the tests to the object files which it needs.
inter-depends: $(ALL_DEPENDS) Makefile
	for f in $(ALL_DEPENDS); do echo check-$$(basename $$(dirname $$f) ): $$(for dir in $$(cat $$f) $$(dirname $$f); do [ "$$(echo $$dir/*.c)" = "$$dir/*.c" ] || echo ccan/"$$(basename $$dir)".o; done); done > $@

# Creates dependencies between tests, so if foo depends on bar, bar is tested
# first 
test-depends: $(ALL_DEPENDS) Makefile
	for f in $(ALL_DEPENDS); do echo check-`basename \`dirname $$f\``: `sed -n 's,ccan/\(.*\),check-\1,p' < $$f`; done > $@

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

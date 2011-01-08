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

# Trying to build the whole repo is usually a lose; there will be some
# dependencies you don't have.
EXCLUDE=wwviaudio ogg_to_pcm jmap jbitset nfs

# Where make scores puts the results
SCOREDIR=scores/$(shell whoami)/$(shell uname -s)-$(shell uname -m)-$(CC)-$(shell git describe --always --dirty)

ALL=$(filter-out $(EXCLUDE), $(REALLY_ALL))

ALL_DEPENDS=$(patsubst %, ccan/%/.depends, $(REALLY_ALL))

# Not all modules have tests.
ALL_TESTS=$(patsubst ccan/%/test/, %, $(foreach dir, $(ALL), $(wildcard ccan/$(dir)/test/)))

# Here's my rough logarithmic timeout graph for my laptop:
#
# 302                                                  -     
#    |                                                  / --*  
#    |                                                 /       
#    |                                                 /       
#    |                                                /        
#    |Execution Time, seconds                        /        
#    |                                               /         
#    |                                          ---//          
#    |                                         /               
#    |                                       //                
#    |                    ---\            ---                  
#    |                 ---    \\    ------                     
#    |-----------------         \---                           
# 19 +------------------------------------------------------+--
#    0           Timeout (ms, logarithmic)               262144
#
# 140
#    |                                                         
#    |------------                                             
#    |            ---                                          
#    |               ---------                                 
#    |                        -------                          
#    |                               --\                       
#    |                                  \\-                    
#    | Tests skipped                       --\                 
#    |                                        \                
#    |                                         \\              
#    |                                           \\\           
#    |                                              \          
#    |                                               \----     
#  --+0---------------------------------------------------==+--
#    0           Timeout (ms, logarithmic)               262144
#
# On my laptop, this runs 574 tests in 40 seconds, vs. a full check which
# runs 676 tests in 260 seconds.
FASTTIMEOUT=750

default: libccan.a

include Makefile-ccan

fastcheck: $(ALL_TESTS:%=summary-fastcheck-%)

check: $(ALL_TESTS:%=summary-check-%)

distclean: clean
	rm -f $(ALL_DEPENDS)

scores: $(SCOREDIR)/SUMMARY

$(SCOREDIR)/SUMMARY: $(patsubst ccan/%/_info, $(SCOREDIR)/score-%, $(wildcard ccan/*/_info))
	git describe --always > $@
	uname -a >> $@
	$(CC) -v >> $@
	cat $^ | grep 'Total score:' >> $@

$(SCOREDIR)/score-%: ccan/%/_info tools/ccanlint/ccanlint $(OBJFILES)
	mkdir -p $(SCOREDIR)
	tools/ccanlint/ccanlint -v -s -d ccan/$* > $@ || true

$(ALL_DEPENDS): %/.depends: %/_info tools/ccan_depends
	tools/ccan_depends $* > $@ || ( rm -f $@; exit 1 )

# Actual dependencies are created in inter-depends
check-%: tools/ccanlint/ccanlint
	tools/ccanlint/ccanlint -d ccan/$*

fastcheck-%: tools/ccanlint/ccanlint
	tools/ccanlint/ccanlint -t $(FASTTIMEOUT) -d ccan/$*

# Doesn't test dependencies, doesn't print verbose fail results.
summary-check-%: tools/ccanlint/ccanlint $(OBJFILES)
	tools/ccanlint/ccanlint -s -d ccan/$*

summary-fastcheck-%: tools/ccanlint/ccanlint $(OBJFILES)
	tools/ccanlint/ccanlint -t $(FASTTIMEOUT) -s -d ccan/$*

ccan/%/info: ccan/%/_info
	$(CC) $(CCAN_CFLAGS) -o $@ -x c $<

libccan.a(%.o): ccan/%.o
	$(AR) r $@ $<

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

config.h: tools/configurator/configurator Makefile Makefile-ccan
	@tools/configurator/configurator $(CC) $(CCAN_CFLAGS) > config.h

include tools/Makefile
-include inter-depends
-include test-depends
-include Makefile-web

# Makefile for CCAN

# 'make quiet=1' builds silently
QUIETEN.1 := @
PRE := $(QUIETEN.$(quiet))

all::

# Our flags for building
WARN_CFLAGS := -Wall -Wstrict-prototypes -Wold-style-definition -Wundef \
 -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith -Wwrite-strings
DEP_CFLAGS = -MMD -MP -MF$(@:%=%.d) -MT$@
CCAN_CFLAGS := -g3 -ggdb $(WARN_CFLAGS) -DCCAN_STR_DEBUG=1 -I. $(CFLAGS)
CFLAGS_FORCE_C_SOURCE := -x c

# Anything with an _info file is a module ...
INFO_SRCS := $(wildcard ccan/*/_info ccan/*/*/_info)
ALL_INFOS := $(INFO_SRCS:%_info=%info)
ALL_MODULES := $(ALL_INFOS:%/info=%)

# ... Except stuff that needs external dependencies, which we exclude
EXCLUDE := altstack jmap jset nfs ogg_to_pcm tal/talloc wwviaudio
MODULES:= $(filter-out $(EXCLUDE:%=ccan/%), $(ALL_MODULES))

# Sources are C files in each module, objects the resulting .o files
SRCS := $(wildcard $(MODULES:%=%/*.c))
OBJS := $(SRCS:%.c=%.o)
DEPS := $(OBJS:%=%.d)

# We build all object files using our CCAN_CFLAGS, after config.h
%.o : %.c config.h
	$(PRE)$(CC) $(CCAN_CFLAGS) $(DEP_CFLAGS) -c $< -o $@

# _info files are compiled into executables and don't need dependencies
%info : %_info config.h
	$(PRE)$(CC) $(CCAN_CFLAGS) -I. -o $@ $(CFLAGS_FORCE_C_SOURCE) $<

# config.h is built by configurator which has no ccan dependencies
CONFIGURATOR := tools/configurator/configurator
$(CONFIGURATOR): $(CONFIGURATOR).c
	$(PRE)$(CC) $(CCAN_CFLAGS) $(DEP_CFLAGS) $< -o $@
config.h: $(CONFIGURATOR) Makefile
	$(PRE)$(CONFIGURATOR) $(CC) $(CCAN_CFLAGS) >$@.tmp && mv $@.tmp $@

# Tools
TOOLS := tools/ccan_depends tools/doc_extract tools/namespacize tools/modfiles
TOOLS_SRCS := $(filter-out $(TOOLS:%=%.c), $(wildcard tools/*.c))
TOOLS_DEPS := $(TOOLS_SRCS:%.c=%.d) $(TOOLS:%=%.d)
TOOLS_CCAN_MODULES := asort err foreach hash htable list noerr opt rbuf \
    read_write_all str take tal tal/grab_file tal/link tal/path tal/str time
TOOLS_CCAN_SRCS := $(wildcard $(TOOLS_CCAN_MODULES:%=ccan/%/*.c))
TOOLS_OBJS := $(TOOLS_SRCS:%.c=%.o) $(TOOLS_CCAN_SRCS:%.c=%.o)
tools/% : tools/%.c $(TOOLS_OBJS)
	$(PRE)$(CC) $(CCAN_CFLAGS) $(DEP_CFLAGS) $< $(TOOLS_OBJS) -lm -o $@

# ccanlint
LINT := tools/ccanlint/ccanlint
LINT_OPTS.ok := -s
LINT_OPTS.fast-ok := -s -x tests_pass_valgrind -x tests_compile_coverage
LINT_SRCS := $(filter-out $(LINT).c, $(wildcard tools/ccanlint/*.c tools/ccanlint/tests/*.c))
LINT_DEPS := $(LINT_SRCS:%.c=%.d) $(LINT).d
LINT_CCAN_MODULES := autodata dgraph ilog lbalance ptr_valid strmap
LINT_CCAN_SRCS := $(wildcard $(LINT_CCAN_MODULES:%=ccan/%/*.c))
LINT_OBJS := $(LINT_SRCS:%.c=%.o) $(LINT_CCAN_SRCS:%.c=%.o) $(TOOLS_OBJS)
ifneq ($(GCOV),)
LINT_GCOV = --gcov="$(GCOV)"
endif
$(LINT): $(LINT).c $(LINT_OBJS)
	$(PRE)$(CC) $(CCAN_CFLAGS) $(DEP_CFLAGS) $(LINT).c $(LINT_OBJS) -lm -o $@

# We generate dependencies for tests into a .d file
%/.d: %/info tools/gen_deps.sh tools/ccan_depends
	$(PRE)tools/gen_deps.sh $* > $@ || rm -f $@
TEST_DEPS := $(MODULES:%=%/.d)

# We produce .ok files when the tests succeed
%.ok: $(LINT) %info
	$(PRE)$(LINT) $(LINT_OPTS.ok) --deps-fail-ignore $(LINT_GCOV) $(LINTFLAGS) $(dir $*) && touch $@

%.fast-ok: $(LINT) %info
	$(PRE)$(LINT) $(LINT_OPTS.fast-ok) --deps-fail-ignore $(LINT_GCOV) $(LINTFLAGS) $(dir $*) && touch $@

check: $(MODULES:%=%/.ok)
fastcheck: $(MODULES:%=%/.fast-ok)

ifeq ($(strip $(filter clean config.h, $(MAKECMDGOALS))),)
-include $(DEPS) $(LINT_DEPS) $(TOOLS_DEPS) $(TEST_DEPS)
endif

# Default target: object files, info files and tools
all:: $(OBJS) $(ALL_INFOS) $(CONFIGURATOR) $(LINT) $(TOOLS)

.PHONY: clean TAGS
clean:
	$(PRE)find . -name "*.d" -o -name "*.o" -o -name "*.ok" | xargs -n 256 rm -f
	$(PRE)rm -f $(CONFIGURATOR) $(LINT) $(TOOLS) TAGS config.h config.h.d $(ALL_INFOS)

# 'make TAGS' builds etags
TAGS:
	$(PRE)find * -name '*.[ch]' | xargs etags

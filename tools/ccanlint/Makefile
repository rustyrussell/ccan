TEST_CFILES := $(wildcard tools/ccanlint/compulsory_tests/*.c) \
	$(wildcard tools/ccanlint/tests/*.c)
TEST_OBJS := $(TEST_CFILES:.c=.o)

CORE_OBJS := tools/ccanlint/ccanlint.o \
	tools/ccanlint/file_analysis.o \
	tools/doc_extract-core.o \
	ccan/str_talloc/str_talloc.o ccan/grab_file/grab_file.o \
	ccan/talloc/talloc.o ccan/noerr/noerr.o

OBJS := $(CORE_OBJS) $(TEST_OBJS)

tools/ccanlint/generated-init-tests: $(OBJS)
	cat $(OBJS:.o=.c) | sed -n 's/^struct ccanlint \([A-Za-z0-9_]*\) = {/{ extern struct ccanlint \1; list_add(\&tests, \&\1.list); }/p' >$@

tools/ccanlint/ccanlint.o: tools/ccanlint/generated-init-tests

tools/ccanlint/ccanlint: $(OBJS)

ccanlint-clean:
	$(RM) tools/ccanlint/generated-init-tests
	$(RM) tools/ccanlint/ccanlint


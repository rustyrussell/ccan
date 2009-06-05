TEST_CFILES := $(wildcard tools/ccanlint/compulsory_tests/*.c) \
	$(wildcard tools/ccanlint/tests/*.c)
TEST_OBJS := $(TEST_CFILES:.c=.o)

CORE_OBJS := tools/ccanlint/ccanlint.o \
	tools/ccanlint/file_analysis.o \
	tools/doc_extract-core.o \
	ccan/str_talloc/str_talloc.o ccan/grab_file/grab_file.o \
	ccan/talloc/talloc.o ccan/noerr/noerr.o

OBJS := $(CORE_OBJS) $(TEST_OBJS)

# FIXME: write a trivial C program to do this
tools/ccanlint/generated-init-tests: $(TEST_CFILES)
	cat $(TEST_CFILES) | grep ^REGISTER_TEST > $@

$(TEST_OBJS): tools/ccanlint/generated-init-tests

tools/ccanlint/ccanlint: $(OBJS)

ccanlint-clean:
	$(RM) tools/ccanlint/generated-init-tests
	$(RM) tools/ccanlint/ccanlint


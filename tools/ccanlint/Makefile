COMPULSORY_TEST_CFILES := $(wildcard tools/ccanlint/compulsory_tests/*.c)
NORMAL_TEST_CFILES := $(wildcard tools/ccanlint/tests/*.c)
TEST_OBJS := $(NORMAL_TEST_CFILES:.c=.o) $(COMPULSORY_TEST_CFILES:.c=.o)

CORE_OBJS := tools/ccanlint/ccanlint.o \
	tools/ccanlint/file_analysis.o \
	tools/doc_extract-core.o \
	tools/depends.o \
	tools/tools.o \
	tools/compile.o \
	ccan/str_talloc/str_talloc.o ccan/grab_file/grab_file.o \
	ccan/btree/btree.o \
	ccan/talloc/talloc.o ccan/noerr/noerr.o \
	ccan/read_write_all/read_write_all.o \
	ccan/opt/opt.o ccan/opt/usage.o ccan/opt/helpers.o

OBJS := $(CORE_OBJS) $(TEST_OBJS)

# FIXME: write a trivial C program to do this
tools/ccanlint/generated-normal-tests: $(NORMAL_TEST_CFILES)
	cat $^ | grep ^REGISTER_TEST > $@
tools/ccanlint/generated-compulsory-tests: $(COMPULSORY_TEST_CFILES)
	cat $^ | grep ^REGISTER_TEST > $@

$(TEST_OBJS): tools/ccanlint/generated-normal-tests tools/ccanlint/generated-compulsory-tests

# Otherwise, ccanlint.c et al. may fail to build
$(CORE_OBJS): tools/ccanlint/generated-normal-tests tools/ccanlint/generated-compulsory-tests

tools/ccanlint/ccanlint: $(OBJS)

ccanlint-clean:
	$(RM) tools/ccanlint/generated-compulsory-tests
	$(RM) tools/ccanlint/generated-normal-tests
	$(RM) tools/ccanlint/ccanlint


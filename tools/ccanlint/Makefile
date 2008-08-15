OBJS := tools/ccanlint/no_info.o \
	tools/ccanlint/has_main_header.o \
	tools/ccanlint/has_tests.o \
	tools/ccanlint/trailing_whitespace.o \
	tools/ccanlint/idempotent.o \

FUTURE:=tools/ccanlint/if_have_not_ifdef.o \
	tools/ccanlint/needs_depends.o \
	tools/ccanlint/has_info_documentation.o \
	tools/ccanlint/has_header_documentation.o \
	tools/ccanlint/has_tests.o \
	tools/ccanlint/builds_ok.o \
	tools/ccanlint/builds_ok_all_have_variants.o \
	tools/ccanlint/run_tests.o \
	tools/ccanlint/test_coverage.o \

tools/ccanlint/generated-init-tests: $(OBJS)
	cat $(OBJS:.o=.c) | sed -n 's/^struct ccanlint \([A-Za-z0-9_]*\) = {/{ extern struct ccanlint \1; list_add(\&tests, \&\1.list); }/p' >$@

tools/ccanlint/ccanlint.o: tools/ccanlint/generated-init-tests

tools/ccanlint/ccanlint: \
	$(OBJS)			\
	tools/ccanlint/ccanlint.o \
	tools/ccanlint/file_analysis.o \
	ccan/string/string.o ccan/talloc/talloc.o ccan/noerr/noerr.o

ccanlint-clean:
	$(RM) tools/ccanlint/generated-init-tests
	$(RM) tools/ccanlint/ccanlint


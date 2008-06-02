OBJS := ccan_tools/ccanlint/no_info.o \
	ccan_tools/ccanlint/has_main_header.o \
	ccan_tools/ccanlint/has_tests.o \
	ccan_tools/ccanlint/trailing_whitespace.o \
	ccan_tools/ccanlint/idempotent.o \

FUTURE:=ccan_tools/ccanlint/if_have_not_ifdef.o \
	ccan_tools/ccanlint/needs_depends.o \
	ccan_tools/ccanlint/has_info_documentation.o \
	ccan_tools/ccanlint/has_header_documentation.o \
	ccan_tools/ccanlint/has_tests.o \
	ccan_tools/ccanlint/builds_ok.o \
	ccan_tools/ccanlint/builds_ok_all_have_variants.o \
	ccan_tools/ccanlint/run_tests.o \
	ccan_tools/ccanlint/test_coverage.o \

ccan_tools/ccanlint/generated-init-tests: $(OBJS)
	cat $(OBJS:.o=.c) | sed -n 's/^struct ccanlint \([A-Za-z0-9_]*\) = {/{ extern struct ccanlint \1; list_add(\&tests, \&\1.list); }/p' >$@

ccan_tools/ccanlint/ccanlint.o: ccan_tools/ccanlint/generated-init-tests

ccan_tools/ccanlint/ccanlint: \
	$(OBJS)			\
	ccan_tools/ccanlint/ccanlint.o \
	ccan_tools/ccanlint/get_file_lines.o \
	ccan_tools/ccanlint/file_analysis.o \
	talloc/talloc.o noerr/noerr.o

ccanlint-clean:
	$(RM) ccan_tools/ccanlint/generated-init-tests


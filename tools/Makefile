ALL_TOOLS = tools/configurator/configurator tools/ccan_depends tools/doc_extract tools/namespacize tools/ccanlint/ccanlint

DEP_OBJS = ccan/grab_file/grab_file.o \
	ccan/noerr/noerr.o \
	ccan/read_write_all/read_write_all.o \
	ccan/str/debug.o \
	ccan/str/str.o \
	ccan/str_talloc/str_talloc.o \
	ccan/talloc/talloc.o \
	tools/compile.o \
	tools/depends.o \
	tools/tools.o

.PHONY: tools
tools: $(ALL_TOOLS)

tools/ccan_depends: tools/ccan_depends.o $(DEP_OBJS)

tools/doc_extract: tools/doc_extract.o tools/doc_extract-core.o $(DEP_OBJS)

tools/namespacize: tools/namespacize.o $(DEP_OBJS)

tools/namespacize.o tools/depends.o: tools/tools.h

tools/configurator/configurator: tools/configurator/configurator.c

tools-clean: ccanlint-clean
	$(RM) $(ALL_TOOLS)

include tools/ccanlint/Makefile

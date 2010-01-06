ALL_TOOLS = tools/ccan_depends tools/doc_extract tools/namespacize tools/ccanlint/ccanlint

DEP_OBJS = tools/depends.o tools/compile.o tools/tools.o ccan/str_talloc/str_talloc.o ccan/grab_file/grab_file.o ccan/talloc/talloc.o ccan/noerr/noerr.o ccan/read_write_all/read_write_all.o

.PHONY: tools
tools: $(ALL_TOOLS)

tools/ccan_depends: tools/ccan_depends.o $(DEP_OBJS)

tools/doc_extract: tools/doc_extract.o tools/doc_extract-core.o $(DEP_OBJS)

tools/namespacize: tools/namespacize.o $(DEP_OBJS)

tools/namespacize.o tools/depends.o: tools/tools.h

tools-clean: ccanlint-clean
	$(RM) $(ALL_TOOLS)

include tools/ccanlint/Makefile

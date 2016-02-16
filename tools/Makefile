ALL_TOOLS = tools/configurator/configurator tools/ccan_depends tools/doc_extract tools/namespacize tools/ccanlint/ccanlint tools/modfiles
LDLIBS = -lrt
DEP_OBJS = ccan/err/err.o \
	ccan/foreach/foreach.o \
	ccan/hash/hash.o \
	ccan/htable/htable.o \
	ccan/list/list.o \
	ccan/noerr/noerr.o \
	ccan/opt/opt.o \
	ccan/opt/helpers.o \
	ccan/opt/parse.o \
	ccan/opt/usage.o \
	ccan/rbuf/rbuf.o \
	ccan/read_write_all/read_write_all.o \
	ccan/str/debug.o \
	ccan/str/str.o \
	ccan/take/take.o \
	ccan/tal/tal.o \
	ccan/tal/grab_file/grab_file.o \
	ccan/tal/link/link.o \
	ccan/tal/path/path.o \
	ccan/tal/str/str.o \
	ccan/time/time.o \
	tools/read_config_header.o \
	tools/ccan_dir.o \
	tools/compile.o \
	tools/depends.o \
	tools/tools.o

.PHONY: tools
tools: $(ALL_TOOLS)

tools/ccan_depends.o: config.h

tools/ccan_depends: tools/ccan_depends.o $(DEP_OBJS)

tools/doc_extract: tools/doc_extract.o tools/doc_extract-core.o $(DEP_OBJS)

tools/namespacize: tools/namespacize.o $(DEP_OBJS)

tools/namespacize.o tools/depends.o: tools/tools.h

tools/configurator/configurator: tools/configurator/configurator.c

tools/modfiles: tools/modfiles.o tools/manifest.o $(DEP_OBJS)

tools-clean: ccanlint-clean
	rm -f $(ALL_TOOLS)

include tools/ccanlint/Makefile

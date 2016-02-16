TEST_CFILES := $(wildcard tools/ccanlint/tests/*.c)
TEST_OBJS := $(TEST_CFILES:.c=.o)

CORE_OBJS := \
	ccan/asort/asort.o \
	ccan/autodata/autodata.o \
	ccan/dgraph/dgraph.o \
	ccan/foreach/foreach.o \
	ccan/hash/hash.o \
	ccan/htable/htable.o \
	ccan/ilog/ilog.o \
	ccan/lbalance/lbalance.o \
	ccan/list/list.o \
	ccan/noerr/noerr.o \
	ccan/opt/helpers.o \
	ccan/opt/opt.o \
	ccan/opt/parse.o \
	ccan/opt/usage.o \
	ccan/ptr_valid/ptr_valid.o \
	ccan/rbuf/rbuf.o \
	ccan/read_write_all/read_write_all.o \
	ccan/str/str.o ccan/str/debug.o \
	ccan/strmap/strmap.o \
	ccan/take/take.o \
	ccan/tal/tal.o \
	ccan/tal/grab_file/grab_file.o \
	ccan/tal/link/link.o \
	ccan/tal/path/path.o \
	ccan/tal/str/str.o \
	ccan/time/time.o \
	tools/ccanlint/async.o \
	tools/ccanlint/ccanlint.o \
	tools/ccanlint/file_analysis.o \
	tools/ccanlint/licenses.o \
	tools/ccan_dir.o \
	tools/compile.o \
	tools/depends.o \
	tools/doc_extract-core.o \
	tools/manifest.o \
	tools/read_config_header.o \
	tools/tools.o

OBJS := $(CORE_OBJS) $(TEST_OBJS)

$(CORE_OBJS): config.h

tools/ccanlint/ccanlint: $(OBJS)

ccanlint-clean:
	rm -f tools/ccanlint/ccanlint

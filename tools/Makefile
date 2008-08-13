tools/ccan_depends: tools/ccan_depends.o tools/depends.o ccan/string/string.o ccan/talloc/talloc.o ccan/noerr/noerr.o

tools/run_tests: tools/run_tests.o tools/depends.o ccan/tap/tap.o ccan/string/string.o ccan/noerr/noerr.o ccan/talloc/talloc.o

tools/doc_extract: tools/doc_extract.o ccan/string/string.o ccan/noerr/noerr.o ccan/talloc/talloc.o

tools/namespacize: tools/namespacize.o tools/depends.o ccan/string/string.o ccan/noerr/noerr.o ccan/talloc/talloc.o

tools/create_dep_tar: tools/create_dep_tar.o tools/depends.o ccan/string/string.o ccan/noerr/noerr.o ccan/talloc/talloc.o tools/_infotojson/sqlite3_database.o tools/_infotojson/utils.o
	$(CC) $(LDFLAGS) -o $@ $^ -lsqlite3

tools/run_tests.o tools/namespacize.o tools/depends.o: tools/tools.h

tools-clean: ccanlint-clean
	rm -f tools/ccan_depends tools/run_tests tools/doc_extract tools/namespacize

include tools/ccanlint/Makefile
include tools/_infotojson/Makefile

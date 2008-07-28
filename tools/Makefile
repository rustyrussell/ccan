tools/ccan_depends: tools/ccan_depends.o tools/depends.o tools/grab_file.o ccan/string/string.o ccan/talloc/talloc.o

tools/run_tests: tools/run_tests.o tools/depends.o tools/grab_file.o ccan/tap/tap.o ccan/string/string.o ccan/talloc/talloc.o

tools/doc_extract: tools/doc_extract.o ccan/string/string.o ccan/talloc/talloc.o

tools/namespacize: tools/namespacize.o tools/grab_file.o tools/depends.o ccan/string/string.o ccan/talloc/talloc.o

tools/run_tests.o tools/namespacize.o tools/grab_file.o tools/depends.o: tools/tools.h

tools-clean: ccanlint-clean
	rm -f tools/ccan_depends tools/run_tests tools/doc_extract tools/namespacize

include tools/ccanlint/Makefile
include tools/_infotojson/Makefile

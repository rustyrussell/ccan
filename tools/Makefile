tools/ccan_depends: tools/ccan_depends.o tools/depends.o tools/split.o tools/grab_file.o ccan/talloc/talloc.o

tools/run_tests: tools/run_tests.o tools/depends.o tools/split.o tools/grab_file.o ccan/tap/tap.o ccan/talloc/talloc.o

tools/doc_extract: tools/doc_extract.c ccan/talloc/talloc.o

tools/namespacize: tools/namespacize.c tools/split.o tools/grab_file.o tools/depends.o ccan/talloc/talloc.o

tools/run_tests.o tools/namespacize.o tools/split.o tools/grab_file.o tools/depends.o: tools/tools.h

tools-clean: ccanlint-clean
	rm -f run_tests doc_extract namespacize

include tools/ccanlint/Makefile

ccan_tools/run_tests: ccan_tools/run_tests.o tap/tap.o talloc/talloc.o 

ccan_tools/doc_extract: ccan_tools/doc_extract.c talloc/talloc.o

ccan_tools/namespacize: ccan_tools/namespacize.c talloc/talloc.o

ccan_tools-clean: ccanlint-clean
	rm -f run_tests doc_extract

include ccan_tools/ccanlint/Makefile

tools/run_tests: tools/run_tests.o ccan/tap/tap.o ccan/talloc/talloc.o 

tools/doc_extract: tools/doc_extract.c ccan/talloc/talloc.o

tools/namespacize: tools/namespacize.c ccan/talloc/talloc.o

tools-clean: ccanlint-clean
	rm -f run_tests doc_extract namespacize

include tools/ccanlint/Makefile

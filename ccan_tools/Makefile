ccan_tools/run_tests: ccan_tools/run_tests.o ccan_tools/libtap/src/tap.o ccan_tools/talloc/talloc.o 

ccan_tools/doc_extract: ccan_tools/doc_extract.c ccan_tools/talloc/talloc.o

ccan_tools/libtap/src/tap.o:
	cd ccan_tools/libtap && ./configure && make

ccan_tools/talloc/talloc.o:
	cd ccan_tools/talloc && ./configure && make

ccan_tools-clean:
	rm -f run_tests doc_extract
	@cd ccan_tools/libtap && make clean
	@cd ccan_tools/talloc && make clean

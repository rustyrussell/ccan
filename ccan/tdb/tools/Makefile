LDLIBS:=-lccan
CFLAGS:=-I../../.. -Wall -O3 #-g -pg
LDFLAGS:=-L../../..

default: replay_trace tdbtorture tdbdump

replay_trace: replay_trace.c keywords.c
	$(LINK.c) $< $(LOADLIBES) $(LDLIBS) -o $@

keywords.c: keywords.gperf
	gperf $< > $@

check: replay_trace
	@rm -f *.reduced_trace
	@set -e; for f in tests/*.trace.tar.bz2; do		\
		tar xvfj $$f;					\
		./replay_trace replay.tdb *.reduced_trace;	\
		rm -f *.reduced_trace;				\
	done

# Usage: make mytest.trace.tar.bz2 TRACEFILES=*.trace
%.trace.tar.bz2: $(patsubst %.trace,%.reduced_trace,$(wildcard $(TRACEFILES)))
	tar cvfj $@ $^

%.reduced_trace: %.trace
	@sed 's/\(^[0-9]* traverse\) .*/\1fn/' < $^ > $@

clean:
	rm -f replay_trace tdbtorture tdbdump *.o

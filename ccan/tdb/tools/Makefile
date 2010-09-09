LDLIBS:=../../tdb.o ../../hash.o
CFLAGS:=-I../../.. -Wall -O3 #-g -pg
LDFLAGS:=-L../../..

default: replay_trace tdbtorture tdbdump tdbtool starvation

benchmark: replay_trace
	@trap "rm -f /tmp/trace.$$$$" 0; for f in benchmarks/*.rz; do if runzip -k $$f -o /tmp/trace.$$$$ && echo -n "$$f": && ./replay_trace --quiet -n 5 replay.tdb /tmp/trace.$$$$ && rm /tmp/trace.$$$$; then rm -f /tmp/trace.$$$$; else exit 1; fi; done

REPLAY_LIBS=$(LDLIBS) ../../str_talloc.o ../../grab_file.o  ../../talloc.o ../../noerr.o
replay_trace: replay_trace.c keywords.c $(REPLAY_LIBS)
	$(LINK.c) $< $(LOADLIBES) $(REPLAY_LIBS) -o $@

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
	rm -f replay_trace tdbtorture tdbdump tdbtool *.o

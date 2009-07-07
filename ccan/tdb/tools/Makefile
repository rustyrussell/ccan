LDLIBS:=-lccan
CFLAGS:=-I../../.. -Wall -O3 #-g -pg
LDFLAGS:=-L../../..

default: replay_trace tdbtorture tdbdump

replay_trace: replay_trace.c keywords.c
	$(LINK.c) $< $(LOADLIBES) $(LDLIBS) -o $@

keywords.c: keywords.gperf
	gperf $< > $@

clean:
	rm -f replay_trace tdbtorture tdbdump *.o

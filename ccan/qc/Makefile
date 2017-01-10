all: example

example: test/example.c qc.c qc.h
	gcc -o example test/example.c qc.c qc.h -lgc -Wall

clean:
	-rm example

#include "../qc.h"
#include <gc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void gen_odd(blob data) {
	int i;
	gen_int(&i);

	if (i % 2 == 0) {
		i++;
	}

	qc_return(int, i);
}

bool is_odd(blob data) {
	int n = qc_args(int, 0, int);

	return n % 2 == 1;
}

bool both_less_than(blob data) {
	int a = qc_args(int, 0, int);
	int b = qc_args(int, 1, int);
	int c = qc_args(int, 2, int);

	return a < c && b < c;
}

void gen_digit_char(blob data) {
	int i;
	gen_int(&i);

	i %= 10;

	char c = '0' + (char) i;

	qc_return(char, c);
}

void gen_digit(blob data) {
	int i;
	gen_int(&i);

	i %= 10;

	qc_return(int, i);
}

bool does_not_parse_to(blob data) {
	char c = qc_args(char, 0, int);
	int i = qc_args(int, 1, int);

	return (c - '0') != i;
}

bool does_not_have_an_h(blob data) {
	char* s = qc_args(char*, 0, char*);

	return strchr(s, 'h') == NULL;
}

int main() {
	qc_init();

	gen gs[] = { gen_odd };

	print ps[] = { print_int };

	// Are all odd numbers odd?
	for_all(is_odd, 1, gs, ps, int);

	gen gs2[] = { gen_int };
	print ps2[] = { print_int };

	// Are all integers odd?
	for_all(is_odd, 1, gs2, ps2, int);

	gen gs3[] = { gen_int, gen_int, gen_int };
	print ps3[] = { print_int, print_int, print_int };

	// Are any two integers less than a third integer?
	for_all(both_less_than, 3, gs3, ps3, int);

	gen gs4[] = { gen_digit_char, gen_digit };
	print ps4[] = { print_char, print_int };

	// Do any characters parse to a matching integer?
	for_all(does_not_parse_to, 2, gs4, ps4, int);

	gen gs5[] = { gen_string };
	print ps5[] = { print_string };

	// Do any string pairs match?
	for_all(does_not_have_an_h, 1, gs5, ps5, char*);

	return 0;
}

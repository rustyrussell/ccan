#include <ccan/deque/deque.h>
#include <ccan/tap/tap.h>

int main(void)
{
	char t, *save;

	plan_tests(64);

	DEQ_WRAP(char) *a;
	ok1(deq_new(a, 4, DEQ_SHRINK_AT_20PCT) == 0);
	ok1(a->v && a->deq.head == 0 && a->deq.tail == 0 && a->deq.len == 0 && a->deq.cap == 4 &&
	    a->deq.min == 4 && a->deq.esz == 1 && a->deq.shrink == DEQ_SHRINK_AT_20PCT);
	save = a->v;
	memset(a->v, 0, a->deq.cap);
	ok1(deq_len(a) == 0 && deq_cap(a) == 4);

	ok1(deq_push(a, 'a') == 1);
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 1 && a->deq.len == 1 && a->deq.cap == 4);
	ok1(a->v[0] == 'a');
	ok1(t = '~' && deq_first(a, &t) == 1 && t == 'a');
	ok1(t = '~' && deq_last(a, &t)  == 1 && t == 'a');
	ok1(deq_len(a) == 1 && deq_cap(a) == 4);

	ok1(t = '~' && deq_pop(a, &t) == 1 && t == 'a');
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 0 && a->deq.len == 0 && a->deq.cap == 4);

	ok1(deq_unshift(a, 'a') == 1);
	ok1(a->v == save && a->deq.head == 3 && a->deq.tail == 0 && a->deq.len == 1 && a->deq.cap == 4);
	ok1(a->v[3] == 'a');
	ok1(t = '~' && deq_first(a, &t) == 1 && t == 'a');
	ok1(t = '~' && deq_last(a, &t)  == 1 && t == 'a');

	ok1(t = '~' && deq_shift(a, &t) == 1 && t == 'a');
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 0 && a->deq.len == 0 && a->deq.cap == 4);

	memset(a->v, 0, a->deq.cap);
	deq_push(a, 'a');
	deq_push(a, 'b');
	deq_push(a, 'c');
	deq_push(a, 'd');
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 0 && a->deq.len == 4 && a->deq.cap == 4);
	ok1(strncmp("abcd", a->v + a->deq.head, a->deq.len) == 0);
	ok1(t = '~' && deq_first(a, &t) == 1 && t == 'a');
	ok1(t = '~' && deq_last(a, &t)  == 1 && t == 'd');

	deq_push(a, 'e');
	ok1(a->v != save);
	save = a->v;
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 5 && a->deq.len == 5 && a->deq.cap == 8);
	ok1(strncmp("abcde", a->v + a->deq.head, a->deq.len) == 0);
	ok1(t = '~' && deq_first(a, &t) == 1 && t == 'a');
	ok1(t = '~' && deq_last(a, &t)  == 1 && t == 'e');
	ok1(deq_len(a) == 5 && deq_cap(a) == 8);


	deq_push(a, 'f');
	deq_push(a, 'g');
	deq_push(a, 'h');
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 0 && a->deq.len == 8 && a->deq.cap == 8);
	ok1(strncmp("abcdefgh", a->v + a->deq.head, a->deq.len) == 0);

	ok1(t = '~' && deq_shift(a, &t) == 1 && t == 'a');
	ok1(t = '~' && deq_shift(a, &t) == 1 && t == 'b');
	ok1(t = '~' && deq_shift(a, &t) == 1 && t == 'c');
	ok1(t = '~' && deq_shift(a, &t) == 1 && t == 'd');
	ok1(a->v == save && a->deq.head == 4 && a->deq.tail == 0 && a->deq.len == 4 && a->deq.cap == 8);
	ok1(strncmp("efgh", a->v + a->deq.head, a->deq.len) == 0);
	ok1(t = '~' && deq_first(a, &t) == 1 && t == 'e');
	ok1(t = '~' && deq_last(a, &t)  == 1 && t == 'h');

	deq_push(a, 'i');
	deq_push(a, 'j');
	deq_push(a, 'k');
	deq_push(a, 'l');
	ok1(a->v == save && a->deq.head == 4 && a->deq.tail == 4 && a->deq.len == 8 && a->deq.cap == 8);
	ok1(strncmp("ijklefgh", a->v, a->deq.len) == 0);

	deq_push(a, 'm');
	ok1(a->v != save);
	save = a->v;
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 9 && a->deq.len == 9 && a->deq.cap == 16);
	ok1(strncmp("efghijklm", a->v + a->deq.head, a->deq.len) == 0);

	int i;
	for (i = 9, t = '!'; i <= 128; i++, t = (t == '~' ? '!' : t + 1))
		deq_push(a, t);

	save = a->v;
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 129 && a->deq.len == 129 && a->deq.cap == 256);
	int j;
	for(j = 0; i > 52; i--, j++)
		deq_shift(a, &t);
	ok1(a->v == save && a->deq.head == j && a->deq.tail == 129 && a->deq.len == 52 && a->deq.cap == 256);
	deq_shift(a, &t);
	ok1(a->v != save);
	save = a->v;
	ok1(a->v == save && a->deq.head == 1 && a->deq.tail == 52 && a->deq.len == 51 && a->deq.cap == 128);
	ok1(strncmp("fghijklmnopqrstuvwxyz{|}~!\"#$%&'()*+,-./0123456789:", a->v + a->deq.head, a->deq.len) == 0);

	a->deq.shrink = DEQ_SHRINK_IF_EMPTY;
	for(i = a->deq.len; i > 1; i--)
		deq_shift(a, &t);
	ok1(a->v == save && a->deq.head == 51 && a->deq.tail == 52 && a->deq.len == 1 && a->deq.cap == 128);
	deq_shift(a, &t);
	ok1(a->v != save);
	save = a->v;
	ok1(a->v == save && a->deq.head == 1 && a->deq.tail == 1 && a->deq.len == 0 && a->deq.cap == a->deq.min);

	deq_reset(a);
	ok1(!a->v);
	ok1(deq_unshift(a, 'a') == 1);
	save = a->v;
	memset(a->v, 0, a->deq.cap - 1);
	ok1(t = '~' && deq_pop(a, &t) == 1 && t == 'a');
	ok1(a->v == save && a->deq.head == 3 && a->deq.tail == 3 && a->deq.len == 0 && a->deq.cap == 4);

	deq_reset(a);
	deq_push(a, 'A');
	save = a->v;
	deq_unshift(a, 'B');
	deq_push(a, 'C');
	deq_unshift(a, 'D');
	ok1(strncmp("ACDB", a->v, 4) == 0);
	ok1(a->v == save && a->deq.head == 2 && a->deq.tail == 2 && a->deq.len == 4 && a->deq.cap == 4);
	ok1(t = '~' && deq_pop(a, &t) == 1 && t == 'C');
	ok1(t = '~' && deq_shift(a, &t) == 1 && t == 'D');
	ok1(t = '~' && deq_shift(a, &t) == 1 && t == 'B');
	ok1(t = '~' && deq_pop(a, &t) == 1 && t == 'A');

	ok1(deq_pop(a, &t) == 0);
	ok1(deq_shift(a, &t) == 0);

	deq_free(a);
	ok1(!a);

	return exit_status();
}

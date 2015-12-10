#include <ccan/xstring/xstring.h>
/* Include the C files directly. */
#include <ccan/xstring/xstring.c>
#include <ccan/tap/tap.h>

int main(void)
{
	plan_tests(70);

	{ //12
		char buf[4] = { 'a', 'a', 'a', 'a' };
		xstring a;

		ok1(xstrInit(&a, buf, sizeof(buf), 0) == 0);
		ok1(a.str == buf && a.len == 0 && a.cap == sizeof(buf) && a.truncated == 0 && a.str[0] == '\0');

		buf[0] = 'a';
		ok1(xstrInit(&a, buf, sizeof(buf), 1) == -1);
		ok1(a.str == buf && a.len == 3 && a.cap == sizeof(buf) && a.truncated == -1 && strcmp(a.str, "aaa") == 0);

		ok1(xstrInit(&a, buf, sizeof(buf), 1) == 0);
		ok1(a.str == buf && a.len == 3 && a.cap == sizeof(buf) && a.truncated == 0 && strcmp(a.str, "aaa") == 0);

		xstrClear(&a);
		ok1(xstrAddChar(&a, '1') == 0 && a.truncated == 0 && strcmp(a.str, "1") == 0);
		ok1(xstrAddChar(&a, '2') == 0 && a.truncated == 0 && strcmp(a.str, "12") == 0);
		ok1(xstrAddChar(&a, '3') == 0 && a.truncated == 0 && strcmp(a.str, "123") == 0);
		ok1(xstrAddChar(&a, '\0') == 0 && a.truncated == 0 && strcmp(a.str, "123") == 0);
		ok1(xstrAddChar(&a, '4') == -1 && a.truncated == -1 && strcmp(a.str, "123") == 0);
		ok1(xstrAddChar(&a, '5') == -1 && a.truncated == -1 && strcmp(a.str, "123") == 0);
	}

	{ //21
		char buf[10];
		xstring _x, *x = &_x;

		ok1(xstrInit(x, buf, sizeof(buf), 0) == 0);
		ok1(x->str == buf && x->len == 0 && x->cap == sizeof(buf) && x->truncated == 0 && *x->str == '\0');

		ok1(xstrAdd(x, "") == 0 && x->len == 0 && x->truncated == 0 && *x->str == '\0');
		ok1(xstrAdd(x, NULL) == 0 && x->len == 0 && x->truncated == 0 && *x->str == '\0');

		ok1(xstrAdd(x, "foo") == 0 && x->len == 3 && x->truncated == 0 && strcmp(x->str, "foo") == 0);
		ok1(xstrAdd(x, "bar") == 0 && x->len == 6 && x->truncated == 0 && strcmp(x->str, "foobar") == 0);
		ok1(xstrAdd(x, "baz") == 0 && x->len == 9 && x->truncated == 0 && strcmp(x->str, "foobarbaz") == 0);
		ok1(xstrAdd(x, "oof") == -1 && x->len == 9 && x->truncated == -1 && strcmp(x->str, "foobarbaz") == 0);
		ok1(xstrAdd(x, "rab") == -1 && x->len == 9 && x->truncated == -1 && strcmp(x->str, "foobarbaz") == 0);

		xstrClear(x);
		ok1(x->str == buf && x->len == 0 && x->cap == sizeof(buf) && x->truncated == 0 && *x->str == '\0');
		ok1(xstrAdd(x, "foobarbazoof") == -1 && x->len == 9 && x->truncated == -1 && strcmp(x->str, "foobarbaz") == 0);

		xstrClear(x);
		ok1(xstrAdd(x, "foo") == 0 && x->len == 3 && x->truncated == 0 && strcmp(x->str, "foo") == 0);
		ok1(xstrAddT(x, "foobarbazoof") == -1 && x->len == 3 && x->truncated == -1 && strcmp(x->str, "foo") == 0);
		xstrClear(x);
		ok1(xstrAddT(&_x, "foobarbazoof") == -1 && x->len == 0 && x->truncated == -1 && *x->str == '\0');

		xstrClear(x);
		ok1(xstrCat(x, "foo", "bar", "baz", NULL) == 0 && x->len == 9 && x->truncated == 0 && strcmp(x->str, "foobarbaz") == 0);
		xstrClear(x);
		ok1(xstrCat(x, "foo", "bar", "baz", "oof", NULL) == -1 && x->len == 9 && x->truncated == -1 && strcmp(x->str, "foobarbaz") == 0);
		xstrClear(x);
		ok1(xstrCatT(x, "foo", "bar", "baz", "oof", NULL) == -1 && x->len == 0 && x->truncated == -1 && *x->str == '\0');
		xstrClear(x);
		ok1(xstrCatT(&_x, "foo", "bar", "baz", "oof", NULL) == -1 && x->len == 0 && x->truncated == -1 && *x->str == '\0');

		xstrClear(x);
		ok1(xstrJoin(x, ",", "fo", "ba", "ba", NULL) == 0 && x->len == 8 && x->truncated == 0 && strcmp(x->str, "fo,ba,ba") == 0);
		xstrClear(x);
		ok1(xstrJoin(x, ",", "foobarbaz", "oof", NULL) == -1 && x->len == 9 && x->truncated == -1 && strcmp(x->str, "foobarbaz") == 0);
		xstrClear(x);
		ok1(xstrJoin(x, ",", "foobarba", "oof", NULL) == -1 && x->len == 9 && x->truncated == -1 && strcmp(x->str, "foobarba,") == 0);
	}

	{ //9
		char buf[10];
		xstring _x, *x = &_x;

		ok1(xstrInit(x, buf, sizeof(buf), 0) == 0);
		ok1(xstrAddSub(x, NULL, 0) == 0 && x->len == 0 && x->truncated == 0 && *x->str == '\0');
		ok1(xstrAddSub(x, "foo", 0) == 0 && x->len == 0 && x->truncated == 0 && *x->str == '\0');
		ok1(xstrAddSub(x, "foobar", 3) == 0 && x->len == 3 && x->truncated == 0 && strcmp(x->str, "foo") == 0);
		ok1(xstrAddSub(x, "foobarbaz", 7) == -1 && x->len == 3 && x->truncated == -1 && strcmp(x->str, "foo") == 0);

		xstrClear(x);
		ok1(xstrAddSubs(x, "aa", 1, "bbb", 2, NULL) == 0 && x->len == 3 && x->truncated == 0 && strcmp(x->str, "abb") == 0);
		ok1(xstrAddSubs(x, "cccccccc", 7, NULL) == -1 && x->len == 3 && x->truncated == -1 && strcmp(x->str, "abb") == 0);
		xstrClear(x);
		ok1(xstrAddSubsT(x, "aa", 1, "bbb", 2, NULL) == 0 && x->len == 3 && x->truncated == 0 && strcmp(x->str, "abb") == 0);
		ok1(xstrAddSubsT(x, "cccccccc", 7, NULL) == -1 && x->len == 3 && x->truncated == -1 && strcmp(x->str, "abb") == 0);
	}

	{ //28
		char a[10], b[10];
		xstring _x, *x = &_x;
		xstring _y, *y = &_y;

		xstrInit(x, a, sizeof(a), 0);
		xstrInit(y, b, sizeof(b), 0);
		xstrAdd(x, "foobarbaz");
		xstrAdd(y, "foobarbaz");
		ok1(xstrEq3(x, y) == 1);
		ok1(xstrEq(x, y) == 1);
		ok1(xstrContains3(x, y, 1) == 1);
		ok1(xstrContains(x, y, 1) == 1);
		xstrAddChar(x, 'x');
		ok1(xstrEq3(x, y) == -1);
		ok1(xstrEq(x, y) == 0);
		ok1(xstrContains3(x, y, 1) == 1);
		ok1(xstrContains(x, y, 1) == 1);
		xstrClear(x);
		xstrAdd(x, "foobarbaz");
		xstrAddChar(y, 'x');
		ok1(xstrEq3(x, y) == -1);
		ok1(xstrEq(x, y) == 0);
		ok1(xstrContains3(x, y, 1) == -1);
		ok1(xstrContains(x, y, 1) == 0);
		xstrClear(y);
		xstrAdd(y, "Foobarbaz");
		ok1(xstrEq3(x, y) == 0);
		ok1(xstrEq(x, y) == 0);

		xstrClear(x);
		xstrClear(y);
		xstrAdd(x, "foo");
		xstrAdd(y, "foobarbaz");
		ok1(xstrContains3(x, y, 1) == 0);
		ok1(xstrContains(x, y, 1) == 0);
		ok1(xstrContains3(y, x, 1) == 1);
		ok1(xstrContains(y, x, 1) == 1);
		ok1(xstrContains3(y, x, 0) == 1);
		ok1(xstrContains(y, x, 0) == 1);
		ok1(xstrContains3(y, x, -1) == 0);
		ok1(xstrContains(y, x, -1) == 0);
		xstrClear(x);
		xstrAdd(x, "baz");
		ok1(xstrContains3(y, x, -1) == 1);
		ok1(xstrContains(y, x, -1) == 1);
		ok1(xstrContains3(y, x, 0) == 1);
		ok1(xstrContains(y, x, 0) == 1);
		ok1(xstrContains3(y, x, 1) == 0);
		ok1(xstrContains(y, x, 1) == 0);
	}

	return exit_status();
}

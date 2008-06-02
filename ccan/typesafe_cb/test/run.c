#include "typesafe_cb/typesafe_cb.h"
#include <string.h>
#include "tap/tap.h"

static char dummy = 0;

/* The example usage. */
static void _set_some_value(void *val)
{
	ok1(val == &dummy);
}

#define set_some_value(expr)						\
	_set_some_value(cast_if_type((expr), unsigned long, void *))

static void _callback_onearg(void (*fn)(void *arg), void *arg)
{
	fn(arg);
}

static void _callback_preargs(void (*fn)(int a, int b, void *arg), void *arg)
{
	fn(1, 2, arg);
}

static void _callback_postargs(void (*fn)(void *arg, int a, int b), void *arg)
{
	fn(arg, 1, 2);
}

#define callback_onearg(cb, arg)					\
	_callback_onearg(typesafe_cb(void, (cb), (arg)), (arg))

#define callback_preargs(cb, arg)					\
	_callback_preargs(typesafe_cb_preargs(void, (cb), (arg), int, int), (arg))

#define callback_postargs(cb, arg)					\
	_callback_postargs(typesafe_cb_postargs(void, (cb), (arg), int, int), (arg))

static void my_callback_onearg(char *p)
{
	ok1(strcmp(p, "hello world") == 0);
}

static void my_callback_onearg_const(const char *p)
{
	ok1(strcmp(p, "hello world") == 0);
}

static void my_callback_onearg_volatile(volatile char *p)
{
	ok1(strcmp((char *)p, "hello world") == 0);
}

static void my_callback_preargs(int a, int b, char *p)
{
	ok1(a == 1);
	ok1(b == 2);
	ok1(strcmp(p, "hello world") == 0);
}

static void my_callback_preargs_const(int a, int b, const char *p)
{
	ok1(a == 1);
	ok1(b == 2);
	ok1(strcmp(p, "hello world") == 0);
}

static void my_callback_preargs_volatile(int a, int b, volatile char *p)
{
	ok1(a == 1);
	ok1(b == 2);
	ok1(strcmp((char *)p, "hello world") == 0);
}

static void my_callback_postargs(char *p, int a, int b)
{
	ok1(a == 1);
	ok1(b == 2);
	ok1(strcmp(p, "hello world") == 0);
}

static void my_callback_postargs_const(const char *p, int a, int b)
{
	ok1(a == 1);
	ok1(b == 2);
	ok1(strcmp(p, "hello world") == 0);
}

static void my_callback_postargs_volatile(volatile char *p, int a, int b)
{
	ok1(a == 1);
	ok1(b == 2);
	ok1(strcmp((char *)p, "hello world") == 0);
}

/* This is simply a compile test; we promised cast_if_type can be in a
 * static initializer. */
struct callback_onearg
{
	void (*fn)(void *arg);
	void *arg;
};

struct callback_onearg cb_onearg
= { typesafe_cb(void, my_callback_onearg, "hello world"), "hello world" };

struct callback_preargs
{
	void (*fn)(int a, int b, void *arg);
	void *arg;
};

struct callback_preargs cb_preargs
= { typesafe_cb_preargs(void, my_callback_preargs, "hi", int, int), "hi" };

struct callback_postargs
{
	void (*fn)(void *arg, int a, int b);
	void *arg;
};

struct callback_postargs cb_postargs
= { typesafe_cb_postargs(void, my_callback_postargs, "hi", int, int), "hi" };

int main(int argc, char *argv[])
{
	void *p = &dummy;
	unsigned long l = (unsigned long)p;

	plan_tests(2 + 3 + 9 + 9);
	set_some_value(p);
	set_some_value(l);

	callback_onearg(my_callback_onearg, "hello world");
	callback_onearg(my_callback_onearg_const, "hello world");
	callback_onearg(my_callback_onearg_volatile, "hello world");

	callback_preargs(my_callback_preargs, "hello world");
	callback_preargs(my_callback_preargs_const, "hello world");
	callback_preargs(my_callback_preargs_volatile, "hello world");

	callback_postargs(my_callback_postargs, "hello world");
	callback_postargs(my_callback_postargs_const, "hello world");
	callback_postargs(my_callback_postargs_volatile, "hello world");

	return exit_status();
}

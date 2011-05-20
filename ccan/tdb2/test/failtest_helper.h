#ifndef TDB2_TEST_FAILTEST_HELPER_H
#define TDB2_TEST_FAILTEST_HELPER_H
#include <ccan/failtest/failtest.h>
#include <stdbool.h>

/* FIXME: Check these! */
#define INITIAL_TDB_MALLOC	"open.c", 338, FAILTEST_MALLOC
#define URANDOM_OPEN		"open.c", 45, FAILTEST_OPEN
#define URANDOM_READ		"open.c", 25, FAILTEST_READ

bool exit_check_log(struct failtest_call *history, unsigned num);
bool failmatch(const struct failtest_call *call,
	       const char *file, int line, enum failtest_call_type type);
enum failtest_result
block_repeat_failures(struct failtest_call *history, unsigned num);

#endif /* TDB2_TEST_LOGGING_H */

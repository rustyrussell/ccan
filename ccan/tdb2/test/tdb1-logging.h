#ifndef TDB_TEST_LOGGING_H
#define TDB_TEST_LOGGING_H
#include <ccan/tdb2/tdb1.h>
#include <stdbool.h>

extern bool suppress_logging;
extern const char *log_prefix;
extern struct tdb1_logging_context taplogctx;

#endif /* TDB_TEST_LOGGING_H */

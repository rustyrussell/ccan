#ifndef TDB_TEST_LOGGING_H
#define TDB_TEST_LOGGING_H
#include <ccan/tdb/tdb.h>
#include <stdbool.h>

extern bool suppress_logging;
extern const char *log_prefix;
extern struct tdb_logging_context taplogctx;

#endif /* TDB_TEST_LOGGING_H */

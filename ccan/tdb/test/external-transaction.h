#ifndef TDB_TEST_EXTERNAL_TRANSACTION_H
#define TDB_TEST_EXTERNAL_TRANSACTION_H
#include <stdbool.h>

/* Do this before doing any tdb stuff.  Return handle, or -1. */
struct agent *prepare_external_agent(void);

/* Ask the external agent to try to do a transaction. */
bool external_agent_transaction(struct agent *handle, const char *tdbname);

#endif /* TDB_TEST_EXTERNAL_TRANSACTION_H */

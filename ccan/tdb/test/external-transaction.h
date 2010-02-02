#ifndef TDB_TEST_EXTERNAL_TRANSACTION_H
#define TDB_TEST_EXTERNAL_TRANSACTION_H
#include <stdbool.h>

enum operation {
	OPEN,
	OPEN_WITH_CLEAR_IF_FIRST,
	TRANSACTION,
};

/* Do this before doing any tdb stuff.  Return handle, or -1. */
struct agent *prepare_external_agent(void);

/* Ask the external agent to try to do an operation. */
bool external_agent_operation(struct agent *handle,
			      enum operation op,
			      const char *tdbname);

/* Ask... */
void external_agent_operation_start(struct agent *agent,
				    enum operation op, const char *tdbname);

/* See if they've done it. */
bool external_agent_operation_check(struct agent *agent, bool block, int *res);
#endif /* TDB_TEST_EXTERNAL_TRANSACTION_H */

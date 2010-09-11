#ifndef TDB_TEST_EXTERNAL_AGENT_H
#define TDB_TEST_EXTERNAL_AGENT_H

/* For locking tests, we need a different process to try things at
 * various times. */
enum operation {
	OPEN,
	OPEN_WITH_CLEAR_IF_FIRST,
	TRANSACTION_START,
	FETCH,
	STORE,
	TRANSACTION_COMMIT,
	CHECK,
	NEEDS_RECOVERY,
	CLOSE,
};

/* Do this before doing any tdb stuff.  Return handle, or -1. */
struct agent *prepare_external_agent(void);

enum agent_return {
	SUCCESS,
	WOULD_HAVE_BLOCKED,
	AGENT_DIED,
	FAILED, /* For fetch, or NEEDS_RECOVERY */
	OTHER_FAILURE,
};

/* Ask the external agent to try to do an operation. 
 * name == tdb name for OPEN/OPEN_WITH_CLEAR_IF_FIRST,
 * record name for FETCH/STORE (store stores name as data too)
 */
enum agent_return external_agent_operation(struct agent *handle,
					   enum operation op,
					   const char *name);

/* Mapping enum -> string. */
const char *agent_return_name(enum agent_return ret);
const char *operation_name(enum operation op);

#endif /* TDB_TEST_EXTERNAL_AGENT_H */

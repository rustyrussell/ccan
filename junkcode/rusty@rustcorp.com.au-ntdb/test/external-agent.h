#ifndef NTDB_TEST_EXTERNAL_AGENT_H
#define NTDB_TEST_EXTERNAL_AGENT_H

/* For locking tests, we need a different process to try things at
 * various times. */
enum operation {
	OPEN,
	OPEN_WITH_HOOK,
	FETCH,
	STORE,
	TRANSACTION_START,
	TRANSACTION_COMMIT,
	NEEDS_RECOVERY,
	CHECK,
	SEND_SIGNAL,
	CLOSE,
};

/* Do this before doing any ntdb stuff.  Return handle, or -1. */
struct agent *prepare_external_agent(void);

enum agent_return {
	SUCCESS,
	WOULD_HAVE_BLOCKED,
	AGENT_DIED,
	FAILED, /* For fetch, or NEEDS_RECOVERY */
	OTHER_FAILURE,
};

/* Ask the external agent to try to do an operation.
 * name == ntdb name for OPEN/OPEN_WITH_CLEAR_IF_FIRST,
 * <key>=<data> for FETCH/STORE.
 */
enum agent_return external_agent_operation(struct agent *handle,
					   enum operation op,
					   const char *name);

/* Hook into free() on ntdb_data in external agent. */
extern void (*external_agent_free)(void *);

/* Mapping enum -> string. */
const char *agent_return_name(enum agent_return ret);
const char *operation_name(enum operation op);

void free_external_agent(struct agent *agent);

/* Internal use: */
struct ntdb_context;
enum agent_return external_agent_needs_rec(struct ntdb_context *ntdb);

#endif /* NTDB_TEST_EXTERNAL_AGENT_H */

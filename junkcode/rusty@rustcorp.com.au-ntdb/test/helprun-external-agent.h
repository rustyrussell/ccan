#include "external-agent.h"
#include "../private.h"

enum agent_return external_agent_needs_rec(struct ntdb_context *ntdb)
{
	return ntdb_needs_recovery(ntdb) ? SUCCESS : FAILED;
}

#include "external-agent.h"

/* This isn't possible with via the ntdb API, but this makes it link. */
enum agent_return external_agent_needs_rec(struct ntdb_context *ntdb)
{
	return FAILED;
}

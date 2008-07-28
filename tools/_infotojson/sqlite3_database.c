/* SQLite3 database backend. */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>
#include "database.h"
#include "utils.h"

/* sqlite3_busy_timeout sleeps for a *second*.  What a piece of shit. */
static int busy(void *unused __attribute__((unused)), int count)
{
	usleep(50000);

	/* If we've been stuck for 1000 iterations (at least 50
	 * seconds), give up. */
	return (count < 1000);
}

void *db_open(const char *file)
{
	sqlite3 *handle;

	int err = sqlite3_open(file, &handle);
	if (err != SQLITE_OK)
		printf("Error %i from sqlite3_open of db '%s'\n", err, file);
	sqlite3_busy_handler(handle, busy, NULL);

	return handle;
}

static int query_cb(void *data, int num, char**vals,
		    char**names __attribute__((unused)))
{
	int i;
	struct db_query *query = data;
	query->rows = realloc_array(query->rows, query->num_rows+1);
	query->rows[query->num_rows] = new_array(char *, num);
	for (i = 0; i < num; i++) {
		/* We don't count rows with NULL results
		 * (eg. count(*),player where count turns out to be
		 * zero. */
		if (!vals[i])
			return 0;
		query->rows[query->num_rows][i] = strdup(vals[i]);
	}
	query->num_rows++;
	return 0;
} 

/* Runs query (SELECT).  Fails if > 1 row returned.  Fills in columns. */
struct db_query *db_query(void *h, const char *query)
{
	struct db_query *ret;
	char *err;

	ret = (struct db_query*) palloc(sizeof(struct db_query));
	ret->rows = NULL;
	ret->num_rows = 0;
	if (sqlite3_exec(h, query, query_cb, ret, &err) != SQLITE_OK)
		printf("Failed sqlite3 query '%s': %s", query, err);
	return ret;
}

/* Runs command (CREATE TABLE/INSERT) */
void db_command(void *h, const char *command)
{
	char *err;

	if (sqlite3_exec(h, command, NULL, NULL, &err) != SQLITE_OK)
		printf("Failed sqlite3 command '%s': %s", command, err);
}

/* Closes database (only called when everything OK). */
void db_close(void *h)
{
	sqlite3_close(h);
}


/* Simple SQL-style database ops.  Currently implemented for sqlite3. */
#include <stdbool.h>

/* Returns handle to the database.. */
void *db_open(const char *file);

/* Runs query (SELECT).  Fills in columns. */
struct db_query
{
	unsigned int num_rows;
	char ***rows;
};

struct db_query *db_query(void *h, const char *query);

/* Runs command (CREATE TABLE/INSERT) */
void db_command(void *h, const char *command);

/* Closes database (only called when everything OK). */
void db_close(void *h);

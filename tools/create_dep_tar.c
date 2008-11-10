#include "tools.h"
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "ccan/grab_file/grab_file.h"
#include "ccan/str_talloc/str_talloc.h"
#include "ccan/talloc/talloc.h"
#include "tools/_infotojson/database.h"

#define TAR_CMD "tar cvvf "

/* get dependents of the module from db */
static char**
get_dependents(const char *dir, const char *db)
{
	char 		*query, *module, **dependents;
	sqlite3 	*handle;
	int 		i;
	struct db_query *q;

	module = strrchr(dir, '/');
	module++;

	/* getting dependents from db */
	handle = db_open(db);
	query = talloc_asprintf(NULL, "select module from search where depends LIKE \"%%%s%%\";", module);
	q = db_query(handle, query);
	db_close(handle);

	if (q->num_rows == 0)
		return 0;
	else {	
		/* getting query results and returning */
		dependents = talloc_array(NULL, char *, q->num_rows + 1);
		for (i = 0; i < q->num_rows; i++)
			dependents[i] = talloc_asprintf(dependents, "ccan/%s", q->rows[i][0]);
		dependents[q->num_rows] = NULL;
		return dependents;
	}
}

/* create tar ball of dependencies */
static void
create_tar(char **deps, const char *dir, const char *targetdir)
{
	FILE 	*p;
	char 	*cmd_args, *cmd, *module, *buffer;

	/* getting module name*/
	module = strrchr(dir, '/');
	module++;
	
	if (deps != NULL) {
		cmd_args = strjoin(NULL, deps, " ");	
		cmd = talloc_asprintf(NULL, TAR_CMD "%s/%s_with_deps.tar %s %s", targetdir, module, cmd_args, dir);
	} else 
		cmd = talloc_asprintf(NULL, TAR_CMD "%s/%s.tar %s", targetdir, module, dir);
			
	p = popen(cmd, "r");
	if (!p)
		err(1, "Executing '%s'", cmd);

	buffer = grab_fd(NULL, fileno(p), NULL);
	if (!buffer)
		err(1, "Reading from '%s'", cmd);
	pclose(p);
}

int main(int argc, char *argv[])
{
	char 	**deps, **dependents;
	int 	i;

	if (argc != 4)
		errx(1, "Usage: create_dep_tar <dir> <targetdir> <db>\n"
		        "Create tar of all the ccan dependencies");

	/* creating tar of the module */
	create_tar(NULL, argv[1], argv[2]);
	printf("creating tar ball of \"%s\"\n", argv[1]);	

	/* creating tar of the module dependencies */
	deps = get_deps(talloc_autofree_context(), argv[1]);
	if (deps != NULL)
		create_tar(deps, argv[1], argv[2]);
	talloc_free(deps);

	/* creating/updating tar of the module dependents */
	dependents = get_dependents(argv[1], argv[3]);
	if (dependents != NULL)
		for (i = 0; dependents[i]; i++) {	
			printf("creating tar ball of \"%s\"\n", dependents[i]);	
			deps = get_deps(NULL, dependents[i]);
			if (deps != NULL)
				create_tar(deps, dependents[i], argv[2]);			
			talloc_free(deps);
		}

	talloc_free(dependents);
	return 0;
}

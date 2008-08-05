#include "tools.h"
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ccan/string/string.h"
#include "ccan/talloc/talloc.h"

#define TAR_CMD "tar cvvf "

static void create_tar(char **deps, const char *dir)
{
	FILE *p;
	char *cmd_args, *cmd, *module, *buffer;

	/* getting module name*/
	module = strrchr(dir, '/');
	module++;
	
	cmd_args = strjoin(NULL, deps, " ");	
	cmd = talloc_asprintf(NULL, TAR_CMD "%s/%s_dep.tar %s", dir, module, cmd_args);
		
	p = popen(cmd, "r");
	if (!p)
		err(1, "Executing '%s'", cmd);

	buffer = grab_fd(NULL, fileno(p));
	if (!buffer)
		err(1, "Reading from '%s'", cmd);
	pclose(p);
}

int main(int argc, char *argv[])
{
	char **deps;

	if (argc != 2)
		errx(1, "Usage: create_dep_tar <dir>\n"
		        "Create tar of all the ccan dependencies");

	deps = get_deps(NULL, argv[1]);
	if(deps != NULL)
		create_tar(deps, argv[1]);
	return 0;
}

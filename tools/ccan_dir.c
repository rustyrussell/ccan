#include <ccan/err/err.h>
#include <ccan/tal/path/path.h>
#include "tools.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* Walk up to find /ccan/ => ccan directory. */
static unsigned int ccan_dir_prefix(const char *fulldir)
{
	unsigned int i;

	assert(fulldir[0] == '/');
	for (i = strlen(fulldir) - 1; i > 0; i--) {
		if (strncmp(fulldir+i, "/ccan", 5) != 0)
			continue;
		if (fulldir[i+5] != '\0' && fulldir[i+5] != '/')
			continue;
		return i + 1;
	}
	return 0;
}

const char *find_ccan_dir(const char *base)
{
	static char *ccan_dir;

	if (!ccan_dir) {
		if (base[0] != '/') {
			const char *tmpctx = path_cwd(NULL);
			find_ccan_dir(path_join(tmpctx, tmpctx, base));
			tal_free(tmpctx);
		} else {
			unsigned int prefix = ccan_dir_prefix(base);
			if (prefix)
				ccan_dir = tal_strndup(NULL, base, prefix);
		}
	}
	return ccan_dir;
}

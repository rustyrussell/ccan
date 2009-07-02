#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Returns the analog of "cd path" from a directory "cwd".
 * The root is defined as empty path (instead of "/")
 * An attempt to go past the root (with "..") leaves the path at root.
 * The cwd is not expanded.
 */
char *pathexpand(const char *cwd, const char *path)
{
	static const char SEP[] = "/";
	if (!*path) /* empty path -> "." (don't move) */
		path = ".";
	if (!*cwd || *SEP == *path) /* no cwd, or path begins with "/" */
		cwd = "";

	while (*cwd && *SEP == *cwd)
		++cwd;

	size_t len = strlen(cwd);
	char *out = malloc(len + 1 + strlen(path) + 1);
	char *p = strcpy(out, cwd) + len;

	for (; *path; ++path)
	{
		char *pl;
		if (p > out && p[-1] != *SEP)
			*p++ = *SEP;
		pl = p;
		while (*path && *SEP != *path)
			*p++ = *path++;
		*p = '\0';
		/* ..."//"... */
		if (p == pl)
			; /* just ignore */
		/* ..."/./"...  */
		else if ( p - pl == 1 && '.' == *pl )
			--p; /* just ignore */
		/* ..."/../"...  */
		else if ( p - pl == 2 && '.' == pl[0] && '.' == pl[1] )
		{
			/* drop the last element of the resulting path */
			if (pl > out && --pl > out)
				for (--pl; pl > out && *SEP != *pl; --pl)
					;
			p = pl > out ? ++pl: out;
		}
		/* ..."/path/"...  */
		else if (*path)
			*p++ = *path; /* just add the separator */

		if (!*path)
			break;
	}
	if (p > out+1 && *SEP == p[-1])
		--p;
	*p = '\0';
	return out;
}

#ifdef CHECK_PATHEXPAND
static void check(const char *cwd, const char *path, const char *good)
{
	static int n = 0;
	printf("%-2d: %10s$ cd %s", ++n, cwd, path);
	char *t = pathexpand(cwd, path);
	if ( strcmp(t, good) )
		printf(" ____________________failed(%s)\n", t);
	else
		printf(" \033[32m%s\033[0m\n", t);
	free(t);
}

int main(int argc, char **argv)
{
	/* 1 */ check("/onelevel", "aa", "onelevel/aa");
	/* 2 */ check("/", "..", "");
	/* 3 */ check("/", "../..", "");
	/* 4 */ check("/one", "aa/../bb", "one/bb");
	/* 5 */ check("/one/two", "aa//bb", "one/two/aa/bb");
	/* 6 */ check("", "/aa//bb", "aa/bb");
	/* 7 */ check("/one/two", "", "one/two");
	/* 8 */ check("/one/two", "aa/..bb/x/../cc/", "one/two/aa/..bb/cc");
	/* 9 */ check("/one/two", "aa/x/././cc////", "one/two/aa/x/cc");
	/* 10 */ check("/one/two", "../../../../aa", "aa");
	/* 11 */ check("one/", "../one/two", "one/two");
	/* 12 */ check("", "../../two", "two");
	/* 13 */ check("a/b/c", "../../two", "a/two");
	/* 14 */ check("a/b/", "../two", "a/two");
	/* 15 */ check("///", "../two", "two");
	return 0;
}
#endif


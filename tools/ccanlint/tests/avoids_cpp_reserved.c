#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <ctype.h>

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static struct ccan_file *main_header(struct manifest *m)
{
	struct ccan_file *f;

	list_for_each(&m->h_files, f, list) {
		if (strstarts(f->name, m->basename)
		    && strlen(f->name) == strlen(m->basename) + 2)
			return f;
	}
	/* Should not happen: we depend on main_header_compiles */
	abort();
}

static void check_headers_no_cpp(struct manifest *m,
				 unsigned int *timeleft, struct score *score)
{
	char *contents;
	char *tmpsrc, *tmpobj, *cmdout;
	int fd;
	struct ccan_file *mainh = main_header(m);

	tmpsrc = temp_file(m, "-included.c", mainh->fullname);
	tmpobj = temp_file(m, ".o", tmpsrc);

	/* We don't fail you for this. */
	score->pass = true;
	fd = open(tmpsrc, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		err(1, "Creating temporary file %s", tmpsrc);

	contents = tal_fmt(tmpsrc,
		   "#define alignas #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define class #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define constexpr #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define const_cast #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define decltype #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define delete #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define dynamic_cast #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define explicit #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define false #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define friend #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define mutable #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define namespace #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define new #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define nullptr #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define operator #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define public #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define private #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define protected #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define reinterpret_cast #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define static_assert #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define static_cast #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define template #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define this #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define thread_local #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define throw #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define true #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define try #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define typeid #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define typename #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define using #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#define virtual #DONT_USE_CPLUSPLUS_RESERVED_NAMES\n"
		   "#include <ccan/%s/%s.h>\n",
				   m->modname, m->basename);
	if (write(fd, contents, strlen(contents)) != strlen(contents))
		err(1, "writing to temporary file %s", tmpsrc);
	close(fd);

	if (compile_object(score, tmpsrc, ccan_dir, compiler, cflags,
			   tmpobj, &cmdout)) {
		score->score = score->total;
	} else {
		score->error = tal_fmt(score,
				       "Main header file with C++ names:\n%s",
				       cmdout);
	}
}

struct ccanlint avoids_cpp_reserved = {
	.key = "avoids_cpp_reserved",
	.name = "Modules main header compiles without C++ reserved words",
	.check = check_headers_no_cpp,
	.can_run = can_build,
	.needs = "main_header_compiles"
};

REGISTER_TEST(avoids_cpp_reserved);

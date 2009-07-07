#include <ccan/tdb/tdb.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/hash/hash.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/str/str.h>
#include <ccan/list/list.h>
#include <err.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

/* Avoid mod by zero */
static unsigned int total_keys = 1;

/* #define DEBUG_DEPS 1 */

struct pipe {
	int fd[2];
};
static struct pipe *pipes;

static void __attribute__((noreturn)) fail(const char *filename,
					   unsigned int line,
					   const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%u: FAIL: ", filename, line);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}
	
/* Try or die. */
#define try(expr, expect)						\
	do {								\
		int ret = (expr);					\
		if (ret != (expect))					\
			fail(filename, i+1, STRINGIFY(expr) "= %i", ret); \
	} while (0)

/* Try or imitate results. */
#define unreliable(expr, expect, force, undo)				\
	do {								\
		int ret = expr;						\
		if (ret != expect) {					\
			fprintf(stderr, "%s:%u: %s gave %i not %i",	\
			      filename, i+1, STRINGIFY(expr), ret, expect); \
			if (expect == 0)				\
				force;					\
			else						\
				undo;					\
		}							\
	} while (0)

static bool key_eq(TDB_DATA a, TDB_DATA b)
{
	if (a.dsize != b.dsize)
		return false;
	return memcmp(a.dptr, b.dptr, a.dsize) == 0;
}

/* This is based on the hash algorithm from gdbm */
static unsigned int hash_key(TDB_DATA *key)
{
	uint32_t value;	/* Used to compute the hash value.  */
	uint32_t   i;	/* Used to cycle through random values. */

	/* Set the initial value from the key size. */
	for (value = 0x238F13AF ^ key->dsize, i=0; i < key->dsize; i++)
		value = (value + (key->dptr[i] << (i*5 % 24)));

	return (1103515243 * value + 12345);  
}

enum op_type {
	OP_TDB_LOCKALL,
	OP_TDB_LOCKALL_MARK,
	OP_TDB_LOCKALL_UNMARK,
	OP_TDB_LOCKALL_NONBLOCK,
	OP_TDB_UNLOCKALL,
	OP_TDB_LOCKALL_READ,
	OP_TDB_LOCKALL_READ_NONBLOCK,
	OP_TDB_UNLOCKALL_READ,
	OP_TDB_CHAINLOCK,
	OP_TDB_CHAINLOCK_NONBLOCK,
	OP_TDB_CHAINLOCK_MARK,
	OP_TDB_CHAINLOCK_UNMARK,
	OP_TDB_CHAINUNLOCK,
	OP_TDB_CHAINLOCK_READ,
	OP_TDB_CHAINUNLOCK_READ,
	OP_TDB_PARSE_RECORD,
	OP_TDB_EXISTS,
	OP_TDB_STORE,
	OP_TDB_APPEND,
	OP_TDB_GET_SEQNUM,
	OP_TDB_WIPE_ALL,
	OP_TDB_TRANSACTION_START,
	OP_TDB_TRANSACTION_CANCEL,
	OP_TDB_TRANSACTION_COMMIT,
	OP_TDB_TRAVERSE_READ_START,
	OP_TDB_TRAVERSE_START,
	OP_TDB_TRAVERSE_END,
	OP_TDB_TRAVERSE,
	OP_TDB_FIRSTKEY,
	OP_TDB_NEXTKEY,
	OP_TDB_FETCH,
	OP_TDB_DELETE,
};

struct op {
	unsigned int serial;
	enum op_type op;
	TDB_DATA key;
	TDB_DATA data;
	int ret;
	/* Who is waiting for us? */
	struct list_head post;
	/* How many are we waiting for? */
	unsigned int pre;

	union {
		int flag; /* open and store */
		struct traverse *trav; /* traverse start */
		TDB_DATA post_append; /* append */
	};
};

static unsigned char hex_char(const char *filename, unsigned int line, char c)
{
	c = toupper(c);
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= '0' && c <= '9')
		return c - '0';
	fail(filename, line, "invalid hex character '%c'", c);
}

/* TDB data is <size>:<%02x>* */
static TDB_DATA make_tdb_data(const void *ctx,
			      const char *filename, unsigned int line,
			      const char *word)
{
	TDB_DATA data;
	unsigned int i;
	const char *p;

	if (streq(word, "NULL"))
		return tdb_null;

	data.dsize = atoi(word);
	data.dptr = talloc_array(ctx, unsigned char, data.dsize);
	p = strchr(word, ':');
	if (!p)
		fail(filename, line, "invalid tdb data '%s'", word);
	p++;
	for (i = 0; i < data.dsize; i++)
		data.dptr[i] = hex_char(filename, line, p[i*2])*16
			+ hex_char(filename, line, p[i*2+1]);

	return data;
}

static void add_op(const char *filename, struct op **op, unsigned int i,
		   unsigned int serial, enum op_type type)
{
	struct op *new;
	*op = talloc_realloc(NULL, *op, struct op, i+1);
	new = (*op) + i;
	new->op = type;
	new->serial = serial;
	new->pre = 0;
	new->ret = 0;
}

static void op_add_nothing(const char *filename,
			   struct op op[], unsigned int op_num, char *words[])
{
	if (words[2])
		fail(filename, op_num+1, "Expected no arguments");
	op[op_num].key = tdb_null;
}

static void op_add_key(const char *filename,
		       struct op op[], unsigned int op_num, char *words[])
{
	if (words[2] == NULL || words[3])
		fail(filename, op_num+1, "Expected just a key");

	op[op_num].key = make_tdb_data(op, filename, op_num+1, words[2]);
	if (op[op_num].op != OP_TDB_TRAVERSE)
		total_keys++;
}

static void op_add_key_ret(const char *filename,
			   struct op op[], unsigned int op_num, char *words[])
{
	if (!words[2] || !words[3] || !words[4] || words[5]
	    || !streq(words[3], "="))
		fail(filename, op_num+1, "Expected <key> = <ret>");
	op[op_num].ret = atoi(words[4]);
	op[op_num].key = make_tdb_data(op, filename, op_num+1, words[2]);
	/* May only be a unique key if it fails */
	if (op[op_num].ret != 0)
		total_keys++;
}

static void op_add_key_data(const char *filename,
			    struct op op[], unsigned int op_num, char *words[])
{
	if (!words[2] || !words[3] || !words[4] || words[5]
	    || !streq(words[3], "="))
		fail(filename, op_num+1, "Expected <key> = <data>");
	op[op_num].key = make_tdb_data(op, filename, op_num+1, words[2]);
	op[op_num].data = make_tdb_data(op, filename, op_num+1, words[4]);
	/* May only be a unique key if it fails */
	if (!op[op_num].data.dptr)
		total_keys++;
}

/* <serial> tdb_store <rec> <rec> <flag> = <ret> */
static void op_add_store(const char *filename,
			 struct op op[], unsigned int op_num, char *words[])
{
	if (!words[2] || !words[3] || !words[4] || !words[5] || !words[6]
	    || words[7] || !streq(words[5], "="))
		fail(filename, op_num+1, "Expect <key> <data> <flag> = <ret>");

	op[op_num].flag = strtoul(words[4], NULL, 0);
	op[op_num].ret = atoi(words[6]);
	op[op_num].key = make_tdb_data(op, filename, op_num+1, words[2]);
	op[op_num].data = make_tdb_data(op, filename, op_num+1, words[3]);
	total_keys++;
}

/* <serial> tdb_append <rec> <rec> = <rec> */
static void op_add_append(const char *filename,
			  struct op op[], unsigned int op_num, char *words[])
{
	if (!words[2] || !words[3] || !words[4] || !words[5] || words[6]
	    || !streq(words[4], "="))
		fail(filename, op_num+1, "Expect <key> <data> = <rec>");

	op[op_num].key = make_tdb_data(op, filename, op_num+1, words[2]);
	op[op_num].data = make_tdb_data(op, filename, op_num+1, words[3]);
	op[op_num].post_append
		= make_tdb_data(op, filename, op_num+1, words[5]);
	total_keys++;
}

/* <serial> tdb_get_seqnum = <ret> */
static void op_add_seqnum(const char *filename,
			  struct op op[], unsigned int op_num, char *words[])
{
	if (!words[2] || !words[3] || words[4] || !streq(words[2], "="))
		fail(filename, op_num+1, "Expect = <ret>");

	op[op_num].key = tdb_null;
	op[op_num].ret = atoi(words[3]);
}

static void op_add_traverse(const char *filename,
			    struct op op[], unsigned int op_num, char *words[])
{
	if (words[2])
		fail(filename, op_num+1, "Expect no arguments");

	op[op_num].key = tdb_null;
	op[op_num].trav = NULL;
}

struct traverse_hash {
	TDB_DATA key;
	unsigned int index;
};

/* A traverse is a hash of keys, each one associated with ops. */
struct traverse {
	/* How many traversal callouts should I do? */
	unsigned int num;

	/* Where is traversal end op? */
	unsigned int end;

	/* For trivial traversals. */
	struct traverse_hash *hash;
};

/* A trivial traversal is one which doesn't terminate early and only
 * plays with its own record.  We can reliably replay these even if
 * traverse order changes. */
static bool is_trivial_traverse(struct op op[], unsigned int end)
{
#if 0
	unsigned int i;
	TDB_DATA cur = tdb_null;

	if (op[end].ret != 0)
		return false;

	for (i = 0; i < end; i++) {
		if (!op[i].key.dptr)
			continue;
		if (op[i].op == OP_TDB_TRAVERSE)
			cur = op[i].key;
		if (!key_eq(cur, op[i].key))
			return false;
	}
	return true;
#endif
	/* With multiple things happening at once, no traverse is trivial. */
	return false;
}

static void op_analyze_traverse(const char *filename,
				struct op op[], unsigned int op_num,
				char *words[])
{
	int i;
	struct traverse *trav = talloc(op, struct traverse);

	/* = %u means traverse function terminated. */
	if (words[2]) {
		if (!streq(words[2], "=") || !words[3] || words[4])
			fail(filename, op_num+1, "expect = <num>");
		op[op_num].ret = atoi(words[3]);
	} else
		op[op_num].ret = 0;

	trav->num = 0;
	trav->end = op_num;
	for (i = op_num-1; i >= 0; i--) {
		if (op[i].op == OP_TDB_TRAVERSE)
			trav->num++;
		if (op[i].op != OP_TDB_TRAVERSE_READ_START
		    && op[i].op != OP_TDB_TRAVERSE_START)
			continue;
		if (op[i].trav)
			continue;
		break;
	}

	if (i < 0)
		fail(filename, op_num+1, "no traversal start found");

	op[i].trav = trav;

	if (is_trivial_traverse(op+i, op_num-i)) {
		/* Fill in a plentiful hash table. */
		op[i].trav->hash = talloc_zero_array(op[i].trav,
						     struct traverse_hash,
						     trav->num * 2);
		for (; i < op_num; i++) {
			unsigned int h;
			if (op[i].op != OP_TDB_TRAVERSE)
				continue;
			h = hash_key(&op[i].key) % (trav->num * 2);
			while (trav->hash[h].index)
				h = (h + 1) % (trav->num * 2);
			trav->hash[h].index = i+1;
			trav->hash[h].key = op[i].key;
		}
	} else
		trav->hash = NULL;
}

/* Keep -Wmissing-declarations happy: */
const struct op_table *
find_keyword (register const char *str, register unsigned int len);

#include "keywords.c"

static int get_len(TDB_DATA key, TDB_DATA data, void *private_data)
{
	return data.dsize;
}

static unsigned run_ops(struct tdb_context *tdb,
			int pre_fd,
			const char *filename,
			struct op op[],
			unsigned int start, unsigned int stop);

struct traverse_info {
	struct op *op;
	const char *filename;
	int pre_fd;
	unsigned int start;
	unsigned int i;
};

/* Trivial case: do whatever they did for this key. */
static int trivial_traverse(struct tdb_context *tdb,
			    TDB_DATA key, TDB_DATA data,
			    void *_tinfo)
{
	struct traverse_info *tinfo = _tinfo;
	struct traverse *trav = tinfo->op[tinfo->start].trav;
	unsigned int h = hash_key(&key) % (trav->num * 2);

	while (trav->hash[h].index) {
		if (key_eq(trav->hash[h].key, key)) {
			run_ops(tdb, tinfo->pre_fd, tinfo->filename, tinfo->op,
				trav->hash[h].index, trav->end);
			tinfo->i++;
			return 0;
		}
		h = (h + 1) % (trav->num * 2);
	}
	fail(tinfo->filename, tinfo->start + 1, "unexpected traverse key");
}

/* More complex.  Just do whatever's they did at the n'th entry. */
static int nontrivial_traverse(struct tdb_context *tdb,
			       TDB_DATA key, TDB_DATA data,
			       void *_tinfo)
{
	struct traverse_info *tinfo = _tinfo;
	struct traverse *trav = tinfo->op[tinfo->start].trav;

	if (tinfo->i == trav->end) {
		/* This can happen if traverse expects to be empty. */
		if (tinfo->start + 1 == trav->end)
			return 1;
		fail(tinfo->filename, tinfo->start + 1,
		     "traverse did not terminate");
	}

	if (tinfo->op[tinfo->i].op != OP_TDB_TRAVERSE)
		fail(tinfo->filename, tinfo->start + 1,
		     "%s:%u:traverse terminated early");

	/* Run any normal ops. */
	tinfo->i = run_ops(tdb, tinfo->pre_fd, tinfo->filename, tinfo->op,
			   tinfo->i+1, trav->end);

	if (tinfo->i == trav->end)
		return 1;

	return 0;
}

static unsigned op_traverse(struct tdb_context *tdb,
			    int pre_fd,
			    const char *filename,
			    int (*traversefn)(struct tdb_context *,
					      tdb_traverse_func, void *),
			    struct op op[],
			    unsigned int start)
{
	struct traverse *trav = op[start].trav;
	struct traverse_info tinfo = { op, filename, pre_fd, start, start+1 };

	/* Trivial case. */
	if (trav->hash) {
		int ret = traversefn(tdb, trivial_traverse, &tinfo);
		if (ret != trav->num)
			fail(filename, start+1, "short traversal %i", ret);
		return trav->end;
	}

	traversefn(tdb, nontrivial_traverse, &tinfo);

	/* Traversing in wrong order can have strange effects: eg. if
	 * original traverse went A (delete A), B, we might do B
	 * (delete A).  So if we have ops left over, we do it now. */
	while (tinfo.i != trav->end) {
		if (op[tinfo.i].op == OP_TDB_TRAVERSE)
			tinfo.i++;
		else
			tinfo.i = run_ops(tdb, pre_fd, filename, op,
					  tinfo.i, trav->end);
	}
	return trav->end;
}

struct depend {
	/* We can have more than one */
	struct list_node list;
	unsigned int file;
	unsigned int op;
};

static void do_pre(const char *filename, int pre_fd,
		   struct op op[], unsigned int i)
{
	while (op[i].pre != 0) {
		unsigned int opnum;

#if DEBUG_DEPS
		printf("%s:%u:waiting for pre\n", filename, i+1);
#endif
		if (read(pre_fd, &opnum, sizeof(opnum)) != sizeof(opnum))
			errx(1, "Reading from pipe");

#if DEBUG_DEPS
		printf("%s:%u:got pre %u\n",
		       filename, i+1, opnum);
#endif
		/* This could be any op, not just this one. */
		if (op[opnum].pre == 0)
			errx(1, "Got unexpected notification for op line %u",
			     opnum + 1);
		op[opnum].pre--;
	}
}

static void do_post(const char *filename, const struct op op[], unsigned int i)
{
	struct depend *dep;

	list_for_each(&op[i].post, dep, list) {
#if DEBUG_DEPS
		printf("%s:%u:sending %u to file %u\n", filename, i+1,
		       dep->op, dep->file);
#endif
		if (write(pipes[dep->file].fd[1], &dep->op, sizeof(dep->op))
		    != sizeof(dep->op))
			err(1, "Failed to tell file %u", dep->file);
	}
}

static __attribute__((noinline))
unsigned run_ops(struct tdb_context *tdb,
		 int pre_fd,
		 const char *filename,
		 struct op op[], unsigned int start, unsigned int stop)
{
	unsigned int i;

	for (i = start; i < stop; i++) {
		do_pre(filename, pre_fd, op, i);

		switch (op[i].op) {
		case OP_TDB_LOCKALL:
			try(tdb_lockall(tdb), op[i].ret);
			break;
		case OP_TDB_LOCKALL_MARK:
			try(tdb_lockall_mark(tdb), op[i].ret);
			break;
		case OP_TDB_LOCKALL_UNMARK:
			try(tdb_lockall_unmark(tdb), op[i].ret);
			break;
		case OP_TDB_LOCKALL_NONBLOCK:
			unreliable(tdb_lockall_nonblock(tdb), op[i].ret,
				   tdb_lockall(tdb), tdb_unlockall(tdb));
			break;
		case OP_TDB_UNLOCKALL:
			try(tdb_unlockall(tdb), op[i].ret);
			break;
		case OP_TDB_LOCKALL_READ:
			try(tdb_lockall_read(tdb), op[i].ret);
			break;
		case OP_TDB_LOCKALL_READ_NONBLOCK:
			unreliable(tdb_lockall_read_nonblock(tdb), op[i].ret,
				   tdb_lockall_read(tdb),
				   tdb_unlockall_read(tdb));
			break;
		case OP_TDB_UNLOCKALL_READ:
			try(tdb_unlockall_read(tdb), op[i].ret);
			break;
		case OP_TDB_CHAINLOCK:
			try(tdb_chainlock(tdb, op[i].key), op[i].ret);
			break;
		case OP_TDB_CHAINLOCK_NONBLOCK:
			unreliable(tdb_chainlock_nonblock(tdb, op[i].key),
				   op[i].ret,
				   tdb_chainlock(tdb, op[i].key),
				   tdb_chainunlock(tdb, op[i].key));
			break;
		case OP_TDB_CHAINLOCK_MARK:
			try(tdb_chainlock_mark(tdb, op[i].key), op[i].ret);
			break;
		case OP_TDB_CHAINLOCK_UNMARK:
			try(tdb_chainlock_unmark(tdb, op[i].key), op[i].ret);
			break;
		case OP_TDB_CHAINUNLOCK:
			try(tdb_chainunlock(tdb, op[i].key), op[i].ret);
			break;
		case OP_TDB_CHAINLOCK_READ:
			try(tdb_chainlock_read(tdb, op[i].key), op[i].ret);
			break;
		case OP_TDB_CHAINUNLOCK_READ:
			try(tdb_chainunlock_read(tdb, op[i].key), op[i].ret);
			break;
		case OP_TDB_PARSE_RECORD:
			try(tdb_parse_record(tdb, op[i].key, get_len, NULL),
			    op[i].ret);
			break;
		case OP_TDB_EXISTS:
			try(tdb_exists(tdb, op[i].key), op[i].ret);
			break;
		case OP_TDB_STORE:
			try(tdb_store(tdb, op[i].key, op[i].data, op[i].flag),
			    op[i].ret < 0 ? op[i].ret : 0);
			break;
		case OP_TDB_APPEND:
			try(tdb_append(tdb, op[i].key, op[i].data),
			    op[i].ret < 0 ? op[i].ret : 0);
			break;
		case OP_TDB_GET_SEQNUM:
			try(tdb_get_seqnum(tdb), op[i].ret);
			break;
		case OP_TDB_WIPE_ALL:
			try(tdb_wipe_all(tdb), op[i].ret);
			break;
		case OP_TDB_TRANSACTION_START:
			try(tdb_transaction_start(tdb), op[i].ret);
			break;
		case OP_TDB_TRANSACTION_CANCEL:
			try(tdb_transaction_cancel(tdb), op[i].ret);
			break;
		case OP_TDB_TRANSACTION_COMMIT:
			try(tdb_transaction_commit(tdb), op[i].ret);
			break;
		case OP_TDB_TRAVERSE_READ_START:
			i = op_traverse(tdb, pre_fd, filename,
					tdb_traverse_read, op, i);
			break;
		case OP_TDB_TRAVERSE_START:
			i = op_traverse(tdb, pre_fd, filename,
					tdb_traverse, op, i);
			break;
		case OP_TDB_TRAVERSE:
			/* Terminate: we're in a traverse, and we've
			 * done our ops. */
			return i;
		case OP_TDB_TRAVERSE_END:
			fail(filename, i+1, "unepxected end traverse");
		/* FIXME: These must be treated like traverse. */
		case OP_TDB_FIRSTKEY:
			if (!key_eq(tdb_firstkey(tdb), op[i].data))
				fail(filename, i+1, "bad firstkey");
			break;
		case OP_TDB_NEXTKEY:
			if (!key_eq(tdb_nextkey(tdb, op[i].key), op[i].data))
				fail(filename, i+1, "bad nextkey");
			break;
		case OP_TDB_FETCH: {
			TDB_DATA f = tdb_fetch(tdb, op[i].key);
			if (!key_eq(f, op[i].data))
				fail(filename, i+1, "bad fetch %u", f.dsize);
			break;
		}
		case OP_TDB_DELETE:
			try(tdb_delete(tdb, op[i].key), op[i].ret);
			break;
		}
		do_post(filename, op, i);
	}
	return i;
}

static struct op *load_tracefile(const char *filename, unsigned int *num,
				 unsigned int *hashsize,
				 unsigned int *tdb_flags,
				 unsigned int *open_flags)
{
	unsigned int i;
	struct op *op = talloc_array(NULL, struct op, 1);
	char **words;
	char **lines;
	char *file;

	file = grab_file(NULL, filename, NULL);
	if (!file)
		err(1, "Reading %s", filename);

	lines = strsplit(file, file, "\n", NULL);
	if (!lines[0])
		errx(1, "%s is empty", filename);

	words = strsplit(lines, lines[0], " ", NULL);
	if (!streq(words[1], "tdb_open"))
		fail(filename, 1, "does not start with tdb_open");

	*hashsize = atoi(words[2]);
	*tdb_flags = strtoul(words[3], NULL, 0);
	*open_flags = strtoul(words[4], NULL, 0);

	for (i = 1; lines[i]; i++) {
		const struct op_table *opt;

		words = strsplit(lines, lines[i], " ", NULL);
		if (!words[0] || !words[1])
			fail(filename, i+1, "Expected serial number and op");
	       
		opt = find_keyword(words[1], strlen(words[1]));
		if (!opt) {
			if (streq(words[1], "tdb_close")) {
				if (lines[i+1])
					fail(filename, i+2,
					     "lines after tdb_close");
				*num = i;
				talloc_free(lines);
				return op;
			}
			fail(filename, i+1, "Unknown operation '%s'", words[1]);
		}

		add_op(filename, &op, i, atoi(words[0]), opt->type);
		opt->enhance_op(filename, op, i, words);
	}

	fprintf(stderr, "%s:%u:last operation is not tdb_close: incomplete?",
	      filename, i);
	talloc_free(lines);
	*num = i - 1;
	return op;
}

/* We remember all the keys we've ever seen, and who has them. */
struct key_user {
	unsigned int file;
	unsigned int op_num;
};

struct keyinfo {
	TDB_DATA key;
	unsigned int num_users;
	struct key_user *user;
};

static bool changes_db(const struct op *op)
{
	if (op->ret != 0)
		return false;

	return op->op == OP_TDB_STORE
		|| op->op == OP_TDB_APPEND
		|| op->op == OP_TDB_WIPE_ALL
		|| op->op == OP_TDB_TRANSACTION_COMMIT
		|| op->op == OP_TDB_DELETE;
}

static struct keyinfo *hash_ops(struct op *op[], unsigned int num_ops[],
				unsigned int num)
{
	unsigned int i, j, h;
	struct keyinfo *hash;

	/* Gcc nexted function extension.  How cool is this? */
	int compare_user_serial(const void *_a, const void *_b)
	{
		const struct key_user *a = _a, *b = _b;
		int ret = op[a->file][a->op_num].serial
			- op[b->file][b->op_num].serial;

		/* Fetches don't inc serial, so we put changes first. */
		if (ret == 0) {
			if (changes_db(&op[a->file][a->op_num])
			    && !changes_db(&op[b->file][b->op_num]))
				return -1;
			if (changes_db(&op[b->file][b->op_num])
			    && !changes_db(&op[a->file][a->op_num]))
				return 1;
		}
		return ret;
	}

	hash = talloc_zero_array(op[0], struct keyinfo, total_keys*2);
	for (i = 0; i < num; i++) {
		for (j = 1; j < num_ops[i]; j++) {
			/* We can't do this on allocation, due to realloc. */
			list_head_init(&op[i][j].post);

			if (!op[i][j].key.dptr)
				continue;

			/* We don't wait for traverse keys */
			/* FIXME: We should, for trivial traversals. */
			if (op[i][j].op == OP_TDB_TRAVERSE)
				continue;

			h = hash_key(&op[i][j].key) % (total_keys * 2);
			while (!key_eq(hash[h].key, op[i][j].key)) {
				if (!hash[h].key.dptr) {
					hash[h].key = op[i][j].key;
					break;
				}
				h = (h + 1) % (total_keys * 2);
			}
			/* Might as well save some memory if we can. */
			if (op[i][j].key.dptr != hash[h].key.dptr) {
				talloc_free(op[i][j].key.dptr);
				op[i][j].key.dptr = hash[h].key.dptr;
			}
			hash[h].user = talloc_realloc(hash, hash[h].user,
						     struct key_user,
						     hash[h].num_users+1);
			hash[h].user[hash[h].num_users].op_num = j;
			hash[h].user[hash[h].num_users].file = i;
			hash[h].num_users++;
		}
	}

	/* Now sort into seqnum order. */
	for (h = 0; h < total_keys * 2; h++)
		qsort(hash[h].user, hash[h].num_users, sizeof(hash[h].user[0]),
		      compare_user_serial);

	return hash;
}

static void add_dependency(void *ctx,
			   struct op *op[],
			   unsigned int needs_file,
			   unsigned int needs_opnum,
			   unsigned int satisfies_file,
			   unsigned int satisfies_opnum)
{
	struct depend *post;

	post = talloc(ctx, struct depend);
	post->file = needs_file;
	post->op = needs_opnum;
	list_add(&op[satisfies_file][satisfies_opnum].post, &post->list);

	op[needs_file][needs_opnum].pre++;
}

static void derive_dependencies(char *filename[],
				struct op *op[], unsigned int num_ops[],
				unsigned int num)
{
	struct keyinfo *hash;
	unsigned int i;

	/* Create hash table for faster key lookup. */
	hash = hash_ops(op, num_ops, num);

	/* We make the naive assumption that two ops on the same key
	 * have to be ordered; it's overkill. */
	for (i = 0; i < total_keys * 2; i++) {
		unsigned int j;

		for (j = 1; j < hash[i].num_users; j++) {
			/* We don't depend on ourselves. */
			if (hash[i].user[j].file == hash[i].user[j-1].file)
				continue;
#if DEBUG_DEPS
			printf("%s:%u: depends on %s:%u\n",
			       filename[hash[i].user[j].file],
			       hash[i].user[j].op_num+1,
			       filename[hash[i].user[j-1].file],
			       hash[i].user[j-1].op_num+1);
#endif
			add_dependency(hash, op,
				       hash[i].user[j].file,
				       hash[i].user[j].op_num,
				       hash[i].user[j-1].file,
				       hash[i].user[j-1].op_num);
		}
	}
}

int main(int argc, char *argv[])
{
	struct timeval start, end;
	unsigned int i, num_ops[argc], hashsize[argc], tdb_flags[argc], open_flags[argc];
	struct op *op[argc];
	int fds[2];
	char c;

	if (argc < 3)
		errx(1, "Usage: %s <tdbfile> <tracefile>...", argv[0]);

	pipes = talloc_array(NULL, struct pipe, argc - 2);
	for (i = 0; i < argc - 2; i++) {
		op[i] = load_tracefile(argv[2+i], &num_ops[i], &hashsize[i],
				       &tdb_flags[i], &open_flags[i]);
		if (pipe(pipes[i].fd) != 0)
			err(1, "creating pipe");
	}

	derive_dependencies(argv+2, op, num_ops, i);

	/* Don't fork for single arg case: simple debugging. */
	if (argc == 3) {
		struct tdb_context *tdb;
		tdb = tdb_open_ex(argv[1], hashsize[0], tdb_flags[0],
				  open_flags[0], 0600,
				  NULL, hash_key);
		run_ops(tdb, pipes[0].fd[0], argv[2],
			op[0], 1, num_ops[0]);
		exit(0);
	}

	if (pipe(fds) != 0)
		err(1, "creating pipe");

	for (i = 0; i < argc - 2; i++) {
		struct tdb_context *tdb;

		switch (fork()) {
		case -1:
			err(1, "fork failed");
		case 0:
			close(fds[1]);
			tdb = tdb_open_ex(argv[1], hashsize[i], tdb_flags[i],
					  open_flags[i], 0600,
					  NULL, hash_key);
			if (!tdb)
				err(1, "Opening tdb %s", argv[1]);

			/* This catches parent exiting. */
			if (read(fds[0], &c, 1) != 1)
				exit(1);
			run_ops(tdb, pipes[i].fd[0], argv[2+i],
				op[i], 1, num_ops[i]);
			exit(0);
		default:
			break;
		}
	}

	/* Let everything settle. */
	sleep(1);

	gettimeofday(&start, NULL);
	/* Tell them all to go!  Any write of sufficient length will do. */
	if (write(fds[1], hashsize, i) != i)
		err(1, "Writing to wakeup pipe");

	for (i = 0; i < argc - 2; i++) {
		int status;
		wait(&status);
		if (!WIFEXITED(status))
			errx(1, "Child died with signal");
		if (WEXITSTATUS(status) != 0)
			errx(1, "Child died with error code");
	}
	gettimeofday(&end, NULL);

	end.tv_sec -= start.tv_sec;
	printf("Time replaying: %lu usec\n",
	       end.tv_sec * 1000000UL + (end.tv_usec - start.tv_usec));
	
	exit(0);
}

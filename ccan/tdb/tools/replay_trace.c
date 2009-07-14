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
#include <errno.h>
#include <signal.h>
#include <assert.h>

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

/* Avoid mod by zero */
static unsigned int total_keys = 1;

/* #define DEBUG_DEPS 1 */

/* Traversals block transactions in the current implementation. */
#define TRAVERSALS_TAKE_TRANSACTION_LOCK 1

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
			fail(filename[file], i+1,			\
			     STRINGIFY(expr) "= %i", ret);		\
	} while (0)

/* Try or imitate results. */
#define unreliable(expr, expect, force, undo)				\
	do {								\
		int ret = expr;						\
		if (ret != expect) {					\
			fprintf(stderr, "%s:%u: %s gave %i not %i",	\
				filename[file], i+1, STRINGIFY(expr),	\
				ret, expect);				\
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
	/* What are we waiting for? */
	struct list_head pre;

	/* If I'm part of a group (traverse/transaction) where is
	 * start?  (Otherwise, 0) */
	unsigned int group_start;

	union {
		int flag; /* open and store */
		struct traverse *trav; /* traverse start */
		struct {  /* append */
			TDB_DATA pre;
			TDB_DATA post;
		} append;
		unsigned int transaction_end; /* transaction start */
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
	new->ret = 0;
	new->group_start = 0;
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

	op[op_num].append.post
		= make_tdb_data(op, filename, op_num+1, words[5]);

	/* By subtraction, figure out what previous data was. */
	op[op_num].append.pre.dptr = op[op_num].append.post.dptr;
	op[op_num].append.pre.dsize
		= op[op_num].append.post.dsize - op[op_num].data.dsize;
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

static void op_add_transaction(const char *filename, struct op op[],
			       unsigned int op_num, char *words[])
{
	if (words[2])
		fail(filename, op_num+1, "Expect no arguments");

	op[op_num].key = tdb_null;
	op[op_num].transaction_end = 0;
}

static void op_analyze_transaction(const char *filename,
				   struct op op[], unsigned int op_num,
				   char *words[])
{
	int i, start;

	op[op_num].key = tdb_null;

	if (words[2])
		fail(filename, op_num+1, "Expect no arguments");

	for (i = op_num-1; i >= 0; i--) {
		if (op[i].op == OP_TDB_TRANSACTION_START &&
		    !op[i].transaction_end)
			break;
	}

	if (i < 0)
		fail(filename, op_num+1, "no transaction start found");

	start = i;
	op[start].transaction_end = op_num;

	/* This rolls in nested transactions.  I think that's right. */
	for (i++; i <= op_num; i++)
		op[i].group_start = start;
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
	int i, start;
	struct traverse *trav = talloc(op, struct traverse);

	op[op_num].key = tdb_null;

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

	start = i;
	op[start].trav = trav;

	for (i = start; i <= op_num; i++)
		op[i].group_start = start;

	if (is_trivial_traverse(op+i, op_num-i)) {
		/* Fill in a plentiful hash table. */
		op[start].trav->hash = talloc_zero_array(op[i].trav,
							 struct traverse_hash,
							 trav->num * 2);
		for (i = start; i < op_num; i++) {
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

struct depend {
	/* We can have more than one */
	struct list_node pre_list;
	struct list_node post_list;
	unsigned int needs_file;
	unsigned int needs_opnum;
	unsigned int satisfies_file;
	unsigned int satisfies_opnum;
};

static void check_deps(const char *filename, struct op op[], unsigned int num)
{
#ifdef DEBUG_DEPS
	unsigned int i;

	for (i = 1; i < num; i++)
		if (!list_empty(&op[i].pre))
			fail(filename, i+1, "Still has dependencies");
#endif
}

static void dump_pre(char *filename[], unsigned int file,
		     struct op op[], unsigned int i)
{
	struct depend *dep;

	printf("%s:%u still waiting for:\n", filename[file], i+1);
	list_for_each(&op[i].pre, dep, pre_list)
		printf("    %s:%u\n",
		       filename[dep->satisfies_file], dep->satisfies_opnum+1);
	check_deps(filename[file], op, i);
}

/* We simply read/write pointers, since we all are children. */
static void do_pre(char *filename[], unsigned int file, int pre_fd,
		   struct op op[], unsigned int i)
{
	while (!list_empty(&op[i].pre)) {
		struct depend *dep;

#if DEBUG_DEPS
		printf("%s:%u:waiting for pre\n", filename[file], i+1);
		fflush(stdout);
#endif
		alarm(10);
		while (read(pre_fd, &dep, sizeof(dep)) != sizeof(dep)) {
			if (errno == EINTR) {
				dump_pre(filename, file, op, i);
				exit(1);
			} else
				errx(1, "Reading from pipe");
		}
		alarm(0);

#if DEBUG_DEPS
		printf("%s:%u:got pre %u from %s:%u\n", filename[file], i+1,
		       dep->needs_opnum+1, filename[dep->satisfies_file],
		       dep->satisfies_opnum+1);
		fflush(stdout);
#endif
		/* This could be any op, not just this one. */
		talloc_free(dep);
	}
}

static void do_post(char *filename[], unsigned int file,
		    const struct op op[], unsigned int i)
{
	struct depend *dep;

	list_for_each(&op[i].post, dep, post_list) {
#if DEBUG_DEPS
		printf("%s:%u:sending to file %s:%u\n", filename[file], i+1,
		       filename[dep->needs_file], dep->needs_opnum+1);
#endif
		if (write(pipes[dep->needs_file].fd[1], &dep, sizeof(dep))
		    != sizeof(dep))
			err(1, "%s:%u failed to tell file %s",
			    filename[file], i+1, filename[dep->needs_file]);
	}
}

static int get_len(TDB_DATA key, TDB_DATA data, void *private_data)
{
	return data.dsize;
}

static unsigned run_ops(struct tdb_context *tdb,
			int pre_fd,
			char *filename[],
			unsigned int file,
			struct op op[],
			unsigned int start, unsigned int stop);

struct traverse_info {
	struct op *op;
	char **filename;
	unsigned file;
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
			run_ops(tdb, tinfo->pre_fd, tinfo->filename,
				tinfo->file, tinfo->op, trav->hash[h].index,
				trav->end);
			tinfo->i++;
			return 0;
		}
		h = (h + 1) % (trav->num * 2);
	}
	fail(tinfo->filename[tinfo->file], tinfo->start + 1,
	     "unexpected traverse key");
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
		fail(tinfo->filename[tinfo->file], tinfo->start + 1,
		     "traverse did not terminate");
	}

	if (tinfo->op[tinfo->i].op != OP_TDB_TRAVERSE)
		fail(tinfo->filename[tinfo->file], tinfo->start + 1,
		     "%s:%u:traverse terminated early");

	/* Run any normal ops. */
	tinfo->i = run_ops(tdb, tinfo->pre_fd, tinfo->filename, tinfo->file,
			   tinfo->op, tinfo->i+1, trav->end);

	if (tinfo->i == trav->end)
		return 1;

	return 0;
}

static unsigned op_traverse(struct tdb_context *tdb,
			    int pre_fd,
			    char *filename[],
			    unsigned int file,
			    int (*traversefn)(struct tdb_context *,
					      tdb_traverse_func, void *),
			    struct op op[],
			    unsigned int start)
{
	struct traverse *trav = op[start].trav;
	struct traverse_info tinfo = { op, filename, file, pre_fd,
				       start, start+1 };

	/* Trivial case. */
	if (trav->hash) {
		int ret = traversefn(tdb, trivial_traverse, &tinfo);
		if (ret != trav->num)
			fail(filename[file], start+1,
			     "short traversal %i", ret);
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
			tinfo.i = run_ops(tdb, pre_fd, filename, file, op,
					  tinfo.i, trav->end);
	}

	return trav->end;
}

static void break_out(int sig)
{
}

static __attribute__((noinline))
unsigned run_ops(struct tdb_context *tdb,
		 int pre_fd,
		 char *filename[],
		 unsigned int file,
		 struct op op[], unsigned int start, unsigned int stop)
{
	unsigned int i;
	struct sigaction sa;

	sa.sa_handler = break_out;
	sa.sa_flags = 0;

	sigaction(SIGALRM, &sa, NULL);
	for (i = start; i < stop; i++) {
		do_pre(filename, file, pre_fd, op, i);

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
			    op[i].ret);
			break;
		case OP_TDB_APPEND:
			try(tdb_append(tdb, op[i].key, op[i].data), op[i].ret);
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
			i = op_traverse(tdb, pre_fd, filename, file,
					tdb_traverse_read, op, i);
			break;
		case OP_TDB_TRAVERSE_START:
			i = op_traverse(tdb, pre_fd, filename, file,
					tdb_traverse, op, i);
			break;
		case OP_TDB_TRAVERSE:
			/* Terminate: we're in a traverse, and we've
			 * done our ops. */
			return i;
		case OP_TDB_TRAVERSE_END:
			fail(filename[file], i+1, "unepxected end traverse");
		/* FIXME: These must be treated like traverse. */
		case OP_TDB_FIRSTKEY:
			if (!key_eq(tdb_firstkey(tdb), op[i].data))
				fail(filename[file], i+1, "bad firstkey");
			break;
		case OP_TDB_NEXTKEY:
			if (!key_eq(tdb_nextkey(tdb, op[i].key), op[i].data))
				fail(filename[file], i+1, "bad nextkey");
			break;
		case OP_TDB_FETCH: {
			TDB_DATA f = tdb_fetch(tdb, op[i].key);
			if (!key_eq(f, op[i].data))
				fail(filename[file], i+1, "bad fetch %u",
				     f.dsize);
			break;
		}
		case OP_TDB_DELETE:
			try(tdb_delete(tdb, op[i].key), op[i].ret);
			break;
		}
		do_post(filename, file, op, i);
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

static const TDB_DATA must_not_exist;
static const TDB_DATA must_exist;
static const TDB_DATA not_exists_or_empty;

/* NULL means doesn't care if it exists or not, &must_exist means
 * it must exist but we don't care what, &must_not_exist means it must
 * not exist, otherwise the data it needs. */
static const TDB_DATA *needs(const struct op *op)
{
	switch (op->op) {
	/* FIXME: Pull forward deps, since we can deadlock */
	case OP_TDB_CHAINLOCK:
	case OP_TDB_CHAINLOCK_NONBLOCK:
	case OP_TDB_CHAINLOCK_MARK:
	case OP_TDB_CHAINLOCK_UNMARK:
	case OP_TDB_CHAINUNLOCK:
	case OP_TDB_CHAINLOCK_READ:
	case OP_TDB_CHAINUNLOCK_READ:
		return NULL;

	case OP_TDB_APPEND:
		if (op->append.pre.dsize == 0)
			return &not_exists_or_empty;
		return &op->append.pre;

	case OP_TDB_STORE:
		if (op->flag == TDB_INSERT) {
			if (op->ret < 0)
				return &must_exist;
			else
				return &must_not_exist;
		} else if (op->flag == TDB_MODIFY) {
			if (op->ret < 0)
				return &must_not_exist;
			else
				return &must_exist;
		}
		/* No flags?  Don't care */
		return NULL;

	case OP_TDB_EXISTS:
		if (op->ret == 1)
			return &must_exist;
		else
			return &must_not_exist;

	case OP_TDB_PARSE_RECORD:
		if (op->ret < 0)
			return &must_not_exist;
		return &must_exist;

	/* FIXME: handle these. */
	case OP_TDB_WIPE_ALL:
	case OP_TDB_FIRSTKEY:
	case OP_TDB_NEXTKEY:
	case OP_TDB_GET_SEQNUM:
	case OP_TDB_TRAVERSE:
	case OP_TDB_TRANSACTION_COMMIT:
	case OP_TDB_TRANSACTION_CANCEL:
	case OP_TDB_TRANSACTION_START:
		return NULL;

	case OP_TDB_FETCH:
		if (!op->data.dptr)
			return &must_not_exist;
		return &op->data;

	case OP_TDB_DELETE:
		if (op->ret < 0)
			return &must_not_exist;
		return &must_exist;

	default:
		errx(1, "Unexpected op %i", op->op);
	}
	
}

/* What's the data after this op?  pre if nothing changed. */
static const TDB_DATA *gives(const struct op *op, const TDB_DATA *pre)
{
	/* Failed ops don't change state of db. */
	if (op->ret < 0)
		return pre;

	if (op->op == OP_TDB_DELETE || op->op == OP_TDB_WIPE_ALL)
		return &tdb_null;

	if (op->op == OP_TDB_APPEND)
		return &op->append.post;

	if (op->op == OP_TDB_STORE)
		return &op->data;

	return pre;
}

static struct keyinfo *hash_ops(struct op *op[], unsigned int num_ops[],
				unsigned int num)
{
	unsigned int i, j, h;
	struct keyinfo *hash;

	hash = talloc_zero_array(op[0], struct keyinfo, total_keys*2);
	for (i = 0; i < num; i++) {
		for (j = 1; j < num_ops[i]; j++) {
			/* We can't do this on allocation, due to realloc. */
			list_head_init(&op[i][j].post);
			list_head_init(&op[i][j].pre);

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

	return hash;
}

static bool satisfies(const TDB_DATA *data, const TDB_DATA *need)
{
	/* Don't need anything?  Cool. */
	if (!need)
		return true;

	/* This should be tdb_null or a real value. */
	assert(data != &must_exist);
	assert(data != &must_not_exist);
	assert(data != &not_exists_or_empty);

	/* must_not_exist == must_not_exist, must_exist == must_exist, or
	   not_exists_or_empty == not_exists_or_empty. */
	if (data->dsize == need->dsize && data->dptr == need->dptr)
		return true;

	/* Must not exist?  data must not exist. */
	if (need == &must_not_exist)
		return data->dptr == NULL;

	/* Must exist? */
	if (need == &must_exist)
		return data->dptr != NULL;

	/* Either noexist or empty. */
	if (need == &not_exists_or_empty)
		return data->dsize == 0;

	/* Needs something specific. */
	return key_eq(*data, *need);
}

static void move_to_front(struct key_user res[], unsigned int elem)
{
	if (elem != 0) {
		struct key_user tmp = res[elem];
		memmove(res + 1, res, elem*sizeof(res[0]));
		res[0] = tmp;
	}
}

static void restore_to_pos(struct key_user res[], unsigned int elem)
{
	if (elem != 0) {
		struct key_user tmp = res[0];
		memmove(res, res + 1, elem*sizeof(res[0]));
		res[elem] = tmp;
	}
}

static bool sort_deps(char *filename[], struct op *op[],
		      struct key_user res[], unsigned num,
		      const TDB_DATA *data, unsigned num_files)
{
	unsigned int i, files_done;
	struct op *this_op;
	bool done[num_files];

	/* Nothing left?  We're sorted. */
	if (num == 0)
		return true;

	memset(done, 0, sizeof(done));

	/* Since ops within a trace file are ordered, we just need to figure
	 * out which file to try next.  Since we don't take into account
	 * inter-key relationships (which exist by virtue of trace file order),
	 * we minimize the chance of harm by trying to keep in serial order. */
	for (files_done = 0, i = 0; i < num && files_done < num_files; i++) {
		if (done[res[i].file])
			continue;

		this_op = &op[res[i].file][res[i].op_num];
		/* Is what we have good enough for this op? */
		if (satisfies(data, needs(this_op))) {
			move_to_front(res, i);
			if (sort_deps(filename, op, res+1, num-1,
				      gives(this_op, data), num_files))
				return true;
			restore_to_pos(res, i);
		}
		done[res[i].file] = true;
		files_done++;
	}

	/* No combination worked. */
	return false;
}

static void check_dep_sorting(struct key_user user[], unsigned num_users,
			      unsigned num_files)
{
#if DEBUG_DEPS
	unsigned int i;
	unsigned minima[num_files];

	memset(minima, 0, sizeof(minima));
	for (i = 0; i < num_users; i++) {
		assert(minima[user[i].file] < user[i].op_num);
		minima[user[i].file] = user[i].op_num;
	}
#endif
}

/* All these ops have the same serial number.  Which comes first?
 *
 * This can happen both because read ops or failed write ops don't
 * change serial number, and also due to race since we access the
 * number unlocked (the race can cause less detectable ordering problems,
 * in which case we'll deadlock and report: fix manually in that case).
 */
static void figure_deps(char *filename[], struct op *op[],
			struct key_user user[], unsigned num_users,
			unsigned num_files)
{
	/* We assume database starts empty. */
	const struct TDB_DATA *data = &tdb_null;

	if (!sort_deps(filename, op, user, num_users, data, num_files))
		fail(filename[user[0].file], user[0].op_num+1,
		     "Could not resolve inter-dependencies");

	check_dep_sorting(user, num_users, num_files);
}

static void sort_ops(struct keyinfo hash[], char *filename[], struct op *op[],
		     unsigned int num)
{
	unsigned int h;

	/* Gcc nexted function extension.  How cool is this? */
	int compare_serial(const void *_a, const void *_b)
	{
		const struct key_user *a = _a, *b = _b;

		/* First, maintain order within any trace file. */
		if (a->file == b->file)
			return a->op_num - b->op_num;

		/* Otherwise, arrange by serial order. */
		return op[a->file][a->op_num].serial
			- op[b->file][b->op_num].serial;
	}

	/* Now sort into serial order. */
	for (h = 0; h < total_keys * 2; h++) {
		struct key_user *user = hash[h].user;

		qsort(user, hash[h].num_users, sizeof(user[0]), compare_serial);
		figure_deps(filename, op, user, hash[h].num_users, num);
	}
}

static int destroy_depend(struct depend *dep)
{
	list_del(&dep->pre_list);
	list_del(&dep->post_list);
	return 0;
}

static void add_dependency(void *ctx,
			   struct op *op[],
			   char *filename[],
			   unsigned int needs_file,
			   unsigned int needs_opnum,
			   unsigned int satisfies_file,
			   unsigned int satisfies_opnum)
{
	struct depend *dep;
	unsigned int needs_start, sat_start;

	/* We don't depend on ourselves. */
	if (needs_file == satisfies_file) {
		assert(satisfies_opnum < needs_opnum);
		return;
	}

#if DEBUG_DEPS
	printf("%s:%u: depends on %s:%u\n",
	       filename[needs_file], needs_opnum+1,
	       filename[satisfies_file], satisfies_opnum+1);
#endif

	needs_start = op[needs_file][needs_opnum].group_start;
	sat_start = op[satisfies_file][satisfies_opnum].group_start;

	/* If needs is in a transaction, we need it before start. */
	if (needs_start) {
		switch (op[needs_file][needs_start].op) {
		case OP_TDB_TRANSACTION_START:
			needs_opnum = needs_start;
#ifdef DEBUG_DEPS
			printf("  -> Back to %u\n", needs_start+1);
			fflush(stdout);
#endif
			break;
		default:
			break;
		}
	}

	/* If satisfies is in a transaction, we wait until after commit. */
	/* FIXME: If transaction is cancelled, don't need dependency. */
	if (sat_start) {
		if (op[satisfies_file][sat_start].op
		    == OP_TDB_TRANSACTION_START) {
			satisfies_opnum
				= op[satisfies_file][sat_start].transaction_end;
#ifdef DEBUG_DEPS
			printf("  -> Depends on %u\n", satisfies_opnum+1);
			fflush(stdout);
#endif
		}
	}

	dep = talloc(ctx, struct depend);
	dep->needs_file = needs_file;
	dep->needs_opnum = needs_opnum;
	dep->satisfies_file = satisfies_file;
	dep->satisfies_opnum = satisfies_opnum;
	list_add(&op[satisfies_file][satisfies_opnum].post, &dep->post_list);
	list_add(&op[needs_file][needs_opnum].pre, &dep->pre_list);
	talloc_set_destructor(dep, destroy_depend);
}

#if TRAVERSALS_TAKE_TRANSACTION_LOCK
struct traverse_dep {
	unsigned int file;
	unsigned int op_num;
	const struct op *op;
};

/* Sort by which one runs first. */
static int compare_traverse_dep(const void *_a, const void *_b)
{
	const struct traverse_dep *a = _a, *b = _b;
	const struct traverse *trava = a->op->trav, *travb = b->op->trav;

	if (a->op->serial != b->op->serial)
		return a->op->serial - b->op->serial;

	/* If they have same serial, it means one didn't make any changes.
	 * Thus sort by end in that case. */
	return a->op[trava->end - a->op_num].serial
		- b->op[travb->end - b->op_num].serial;
}

/* Traversals can deadlock against each other.  Force order. */
static void make_traverse_depends(char *filename[],
				  struct op *op[], unsigned int num_ops[],
				  unsigned int num)
{
	unsigned int i, j, num_traversals = 0;
	struct traverse_dep *dep;

	dep = talloc_array(NULL, struct traverse_dep, 1);

	/* Count them. */
	for (i = 0; i < num; i++) {
		for (j = 0; j < num_ops[i]; j++) {
			if (op[i][j].op == OP_TDB_TRAVERSE_START
			    || op[i][j].op == OP_TDB_TRAVERSE_READ_START) {
				dep = talloc_realloc(NULL, dep,
						     struct traverse_dep,
						     num_traversals+1);
				dep[num_traversals].file = i;
				dep[num_traversals].op_num = j;
				dep[num_traversals].op = &op[i][j];
				num_traversals++;
			}
		}
	}
	qsort(dep, num_traversals, sizeof(dep[0]), compare_traverse_dep);
	for (i = 1; i < num_traversals; i++) {
		/* i depends on end of traverse i-1. */
		add_dependency(NULL, op, filename, dep[i].file, dep[i].op_num,
			       dep[i-1].file, dep[i-1].op->trav->end);
	}
	talloc_free(dep);
}
#endif /* TRAVERSALS_TAKE_TRANSACTION_LOCK */

static bool changes_db(const struct op *op)
{
	return gives(op, NULL) != NULL;
}

static void depend_on_previous(struct op *op[],
			       char *filename[],
			       unsigned int num,
			       struct key_user user[],
			       unsigned int i,
			       int prev)
{
	bool deps[num];
	int j;

	if (i == 0)
		return;

	if (prev == i - 1) {
		/* Just depend on previous. */
		add_dependency(NULL, op, filename,
			       user[i].file, user[i].op_num,
			       user[prev].file, user[prev].op_num);
		return;
	}

	/* We have to wait for the readers.  Find last one in *each* file. */
	memset(deps, 0, sizeof(deps));
	deps[user[i].file] = true;
	for (j = i - 1; j > prev; j--) {
		if (!deps[user[j].file]) {
			add_dependency(NULL, op, filename,
				       user[i].file, user[i].op_num,
				       user[j].file, user[j].op_num);
			deps[user[j].file] = true;
		}
	}
}

/* This is simple, but not complete.  We don't take into account
 * indirect dependencies. */
static void optimize_dependencies(struct op *op[], unsigned int num_ops[],
				  unsigned int num)
{
	unsigned int i, j;

	/* There can only be one real dependency on each file */
	for (i = 0; i < num; i++) {
		for (j = 1; j < num_ops[i]; j++) {
			struct depend *dep, *next;
			struct depend *prev[num];

			memset(prev, 0, sizeof(prev));

			list_for_each_safe(&op[i][j].pre, dep, next, pre_list) {
				if (!prev[dep->satisfies_file]) {
					prev[dep->satisfies_file] = dep;
					continue;
				}
				if (prev[dep->satisfies_file]->satisfies_opnum
				    < dep->satisfies_opnum) {
					talloc_free(prev[dep->satisfies_file]);
					prev[dep->satisfies_file] = dep;
				} else
					talloc_free(dep);
			}
		}
	}

	for (i = 0; i < num; i++) {
		int deps[num];

		for (j = 0; j < num; j++)
			deps[j] = -1;

		for (j = 1; j < num_ops[i]; j++) {
			struct depend *dep, *next;

			list_for_each_safe(&op[i][j].pre, dep, next, pre_list) {
				if (deps[dep->satisfies_file]
				    >= (int)dep->satisfies_opnum)
					talloc_free(dep);
				else
					deps[dep->satisfies_file]
						= dep->satisfies_opnum;
			}
		}
	}
}

static void derive_dependencies(char *filename[],
				struct op *op[], unsigned int num_ops[],
				unsigned int num)
{
	struct keyinfo *hash;
	unsigned int h, i;

	/* Create hash table for faster key lookup. */
	hash = hash_ops(op, num_ops, num);

	/* Sort them by serial number. */
	sort_ops(hash, filename, op, num);

	/* Create dependencies back to the last change, rather than
	 * creating false dependencies by naively making each one
	 * depend on the previous.  This has two purposes: it makes
	 * later optimization simpler, and it also avoids deadlock with
	 * same sequence number ops inside traversals (if one
	 * traversal doesn't write anything, two ops can have the same
	 * sequence number yet we can create a traversal dependency
	 * the other way). */
	for (h = 0; h < total_keys * 2; h++) {
		int prev = -1;

		if (hash[h].num_users < 2)
			continue;

		for (i = 0; i < hash[h].num_users; i++) {
			if (changes_db(&op[hash[h].user[i].file]
				       [hash[h].user[i].op_num])) {
				depend_on_previous(op, filename, num,
						   hash[h].user, i, prev);
				prev = i;
			} else if (prev >= 0)
				add_dependency(hash, op, filename,
					       hash[h].user[i].file,
					       hash[h].user[i].op_num,
					       hash[h].user[prev].file,
					       hash[h].user[prev].op_num);
		}
	}

#if TRAVERSALS_TAKE_TRANSACTION_LOCK
	make_traverse_depends(filename, op, num_ops, num);
#endif

	optimize_dependencies(op, num_ops, num);
}

int main(int argc, char *argv[])
{
	struct timeval start, end;
	unsigned int i, num_ops[argc], hashsize[argc], tdb_flags[argc], open_flags[argc];
	struct op *op[argc];
	int fds[2];
	char c;
	bool ok = true;

	if (argc < 3)
		errx(1, "Usage: %s <tdbfile> <tracefile>...", argv[0]);

	pipes = talloc_array(NULL, struct pipe, argc - 2);
	for (i = 0; i < argc - 2; i++) {
		printf("Loading tracefile %s...", argv[2+i]);
		fflush(stdout);
		op[i] = load_tracefile(argv[2+i], &num_ops[i], &hashsize[i],
				       &tdb_flags[i], &open_flags[i]);
		if (pipe(pipes[i].fd) != 0)
			err(1, "creating pipe");
		printf("done\n");
	}

	printf("Calculating inter-dependencies...");
	fflush(stdout);
	derive_dependencies(argv+2, op, num_ops, i);
	printf("done\n");

	/* Don't fork for single arg case: simple debugging. */
	if (argc == 3) {
		struct tdb_context *tdb;
		tdb = tdb_open_ex(argv[1], hashsize[0], tdb_flags[0],
				  open_flags[0], 0600,
				  NULL, hash_key);
		printf("Single threaded run...");
		fflush(stdout);

		run_ops(tdb, pipes[0].fd[0], argv+2, 0, op[0], 1, num_ops[0]);
		check_deps(argv[2], op[0], num_ops[0]);

		printf("done\n");
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
			run_ops(tdb, pipes[i].fd[0], argv+2, i, op[i], 1,
				num_ops[i]);
			check_deps(argv[2+i], op[i], num_ops[i]);
			exit(0);
		default:
			break;
		}
	}

	/* Let everything settle. */
	sleep(1);

	printf("Starting run...");
	fflush(stdout);
	gettimeofday(&start, NULL);
	/* Tell them all to go!  Any write of sufficient length will do. */
	if (write(fds[1], hashsize, i) != i)
		err(1, "Writing to wakeup pipe");

	for (i = 0; i < argc - 2; i++) {
		int status;
		wait(&status);
		if (!WIFEXITED(status)) {
			warnx("Child died with signal %i", WTERMSIG(status));
			ok = false;
		} else if (WEXITSTATUS(status) != 0)
			/* Assume child spat out error. */
			ok = false;
	}
	if (!ok)
		exit(1);

	gettimeofday(&end, NULL);
	printf("done\n");

	end.tv_sec -= start.tv_sec;
	printf("Time replaying: %lu usec\n",
	       end.tv_sec * 1000000UL + (end.tv_usec - start.tv_usec));
	
	exit(0);
}

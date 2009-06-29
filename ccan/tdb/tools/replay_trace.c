#include <ccan/tdb/tdb.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/hash/hash.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/str/str.h>
#include <err.h>
#include <ctype.h>
#include <sys/time.h>

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

/* Try or die. */
#define try(expr, op)							\
	do {								\
		int ret = (expr);					\
		if (ret < 0) {						\
			if (tdb_error(tdb) != -op.ret)			\
				errx(1, "Line %u: " STRINGIFY(expr)	\
				     "= %i: %s",			\
				     i+1, ret, tdb_errorstr(tdb));	\
		} else if (ret != op.ret)				\
			errx(1, "Line %u: " STRINGIFY(expr) "= %i: %s",	\
			     i+1, ret, tdb_errorstr(tdb));		\
	} while (0)

/* Try or imitate results. */
#define unreliable(expr, expect, force, undo)				\
	do {								\
		int ret = expr;						\
		if (ret != expect) {					\
			warnx("Line %u: %s gave %i not %i",		\
			      i+1, STRINGIFY(expr), ret, expect);	\
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
	OP_TDB_INCREMENT_SEQNUM_NONBLOCK,
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
	OP_TDB_CLOSE,
};

struct op {
	enum op_type op;
	TDB_DATA key;
	TDB_DATA data;
	int ret;
	union {
		int flag; /* open and store */
		struct traverse *trav; /* traverse start */
	};
};

static unsigned char hex_char(unsigned int line, char c)
{
	c = toupper(c);
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= '0' && c <= '9')
		return c - '0';
	errx(1, "Line %u: invalid hex character '%c'", line, c);
}

/* TDB data is <size>:<%02x>* */
static TDB_DATA make_tdb_data(const void *ctx,
			      unsigned int line, const char *word)
{
	TDB_DATA data;
	unsigned int i;
	const char *p;

	data.dsize = atoi(word);
	data.dptr = talloc_array(ctx, unsigned char, data.dsize);
	p = strchr(word, ':');
	if (!p)
		errx(1, "Line %u: Invalid tdb data '%s'", line, word);
	p++;
	for (i = 0; i < data.dsize; i++)
		data.dptr[i] = hex_char(line, p[i*2])*16
			+ hex_char(line, p[i*2+1]);
	return data;
}

static struct op *add_op(struct op **op, unsigned int i,
			 enum op_type type, const char *key, const char *data,
			 int ret)
{
	struct op *new;
	*op = talloc_realloc(NULL, *op, struct op, i+1);
	new = (*op) + i;
	new->op = type;
	new->ret = ret;
	if (key)
		new->key = make_tdb_data(*op, i+1, key);
	else
		new->key = tdb_null;
	if (data)
		new->data = make_tdb_data(*op, i+1, data);
	else
		new->data = tdb_null;
	return new;
}

static int get_len(TDB_DATA key, TDB_DATA data, void *private_data)
{
	return data.dsize;
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

/* A trivial traversal is one which doesn't terminate early and only
 * plays with its own record.  We can reliably replay these even if
 * traverse order changes. */
static bool is_trivial_traverse(struct op op[], unsigned int end)
{
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
}

static void analyze_traverse(struct op op[], unsigned int end)
{
	int i;
	struct traverse *trav = talloc(op, struct traverse);

	trav->num = 0;
	trav->end = end;
	for (i = end-1; i >= 0; i--) {
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
		errx(1, "Line %u: no traversal start found", end+1);

	op[i].trav = trav;

	if (is_trivial_traverse(op+i, end-i)) {
		/* Fill in a plentiful hash table. */
		op[i].trav->hash = talloc_zero_array(op[i].trav,
						     struct traverse_hash,
						     trav->num * 2);
		for (; i < end; i++) {
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

static unsigned run_ops(struct tdb_context *tdb, const struct op op[],
			unsigned int start, unsigned int stop);

struct traverse_info {
	const struct op *op;
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
			run_ops(tdb, tinfo->op, trav->hash[h].index, trav->end);
			tinfo->i++;
			return 0;
		}
		h = (h + 1) % (trav->num * 2);
	}
	errx(1, "Traverse at %u: unexpected key", tinfo->start + 1);
}

/* More complex.  Just do whatever's they did at the n'th entry. */
static int nontrivial_traverse(struct tdb_context *tdb,
			       TDB_DATA key, TDB_DATA data,
			       void *_tinfo)
{
	struct traverse_info *tinfo = _tinfo;
	struct traverse *trav = tinfo->op[tinfo->start].trav;

	if (tinfo->i == trav->end)
		errx(1, "Transaction starting line %u did not terminate",
		     tinfo->start + 1);

	if (tinfo->op[tinfo->i].op != OP_TDB_TRAVERSE)
		errx(1, "Transaction starting line %u terminated early",
		     tinfo->start + 1);

	/* Run any normal ops. */
	tinfo->i = run_ops(tdb, tinfo->op, tinfo->i+1, trav->end);

	if (tinfo->i == trav->end)
		return 1;
	return 0;
}

static unsigned op_traverse(struct tdb_context *tdb,
			    int (*traversefn)(struct tdb_context *,
					      tdb_traverse_func, void *),
			    const struct op op[],
			    unsigned int start)
{
	struct traverse *trav = op[start].trav;
	struct traverse_info tinfo = { op, start, start+1 };

	/* Trivial case. */
	if (trav->hash) {
		int ret = traversefn(tdb, trivial_traverse, &tinfo);
		if (ret != trav->num)
			errx(1, "Line %u: short traversal %i", start+1, ret);
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
			tinfo.i = run_ops(tdb, op, tinfo.i, trav->end);
	}
	return trav->end;
}

static __attribute__((noinline))
unsigned run_ops(struct tdb_context *tdb, const struct op op[],
			unsigned int start, unsigned int stop)
{
	unsigned int i;
	TDB_DATA data;

	for (i = start; i < stop; i++) {
		switch (op[i].op) {
		case OP_TDB_LOCKALL:
			try(tdb_lockall(tdb), op[i]);
			break;
		case OP_TDB_LOCKALL_MARK:
			try(tdb_lockall_mark(tdb), op[i]);
			break;
		case OP_TDB_LOCKALL_UNMARK:
			try(tdb_lockall_unmark(tdb), op[i]);
			break;
		case OP_TDB_LOCKALL_NONBLOCK:
			unreliable(tdb_lockall_nonblock(tdb), op[i].ret,
				   tdb_lockall(tdb), tdb_unlockall(tdb));
			break;
		case OP_TDB_UNLOCKALL:
			try(tdb_unlockall(tdb), op[i]);
			break;
		case OP_TDB_LOCKALL_READ:
			try(tdb_lockall_read(tdb), op[i]);
			break;
		case OP_TDB_LOCKALL_READ_NONBLOCK:
			unreliable(tdb_lockall_read_nonblock(tdb), op[i].ret,
				   tdb_lockall_read(tdb),
				   tdb_unlockall_read(tdb));
			break;
		case OP_TDB_UNLOCKALL_READ:
			try(tdb_unlockall_read(tdb), op[i]);
			break;
		case OP_TDB_CHAINLOCK:
			try(tdb_chainlock(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_CHAINLOCK_NONBLOCK:
			unreliable(tdb_chainlock_nonblock(tdb, op[i].key),
				   op[i].ret,
				   tdb_chainlock(tdb, op[i].key),
				   tdb_chainunlock(tdb, op[i].key));
			break;
		case OP_TDB_CHAINLOCK_MARK:
			try(tdb_chainlock_mark(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_CHAINLOCK_UNMARK:
			try(tdb_chainlock_unmark(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_CHAINUNLOCK:
			try(tdb_chainunlock(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_CHAINLOCK_READ:
			try(tdb_chainlock_read(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_CHAINUNLOCK_READ:
			try(tdb_chainunlock_read(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_INCREMENT_SEQNUM_NONBLOCK:
			tdb_increment_seqnum_nonblock(tdb);
			break;
		case OP_TDB_PARSE_RECORD:
			try(tdb_parse_record(tdb, op[i].key, get_len, NULL), op[i]);
			break;
		case OP_TDB_EXISTS:
			try(tdb_exists(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_STORE:
			try(tdb_store(tdb, op[i].key, op[i].data, op[i].flag), op[i]);
			break;
		case OP_TDB_APPEND:
			try(tdb_append(tdb, op[i].key, op[i].data), op[i]);
			break;
		case OP_TDB_GET_SEQNUM:
			try(tdb_get_seqnum(tdb), op[i]);
			break;
		case OP_TDB_WIPE_ALL:
			try(tdb_wipe_all(tdb), op[i]);
			break;
		case OP_TDB_TRANSACTION_START:
			try(tdb_transaction_start(tdb), op[i]);
			break;
		case OP_TDB_TRANSACTION_CANCEL:
			try(tdb_transaction_cancel(tdb), op[i]);
			break;
		case OP_TDB_TRANSACTION_COMMIT:
			try(tdb_transaction_commit(tdb), op[i]);
			break;
		case OP_TDB_TRAVERSE_READ_START:
			i = op_traverse(tdb, tdb_traverse_read, op, i);
			break;
		case OP_TDB_TRAVERSE_START:
			i = op_traverse(tdb, tdb_traverse, op, i);
			break;
		case OP_TDB_TRAVERSE:
			/* Terminate: we're in a traverse, and we've
			 * done our ops. */
			return i;
		case OP_TDB_TRAVERSE_END:
			errx(1, "Line %u: unepxected end traverse\n", i+1);
		case OP_TDB_FIRSTKEY:
			data = tdb_firstkey(tdb);
			if (data.dsize != op[i].data.dsize
			    || memcmp(data.dptr, op[i].data.dptr, data.dsize))
				errx(1, "Line %u: bad firstkey", i+1);
			break;
		case OP_TDB_NEXTKEY:
			data = tdb_nextkey(tdb, op[i].key);
			if (data.dsize != op[i].data.dsize
			    || memcmp(data.dptr, op[i].data.dptr, data.dsize))
				errx(1, "Line %u: bad nextkey", i+1);
			break;
		case OP_TDB_FETCH:
			data = tdb_fetch(tdb, op[i].key);
			if (data.dsize != op[i].data.dsize
			    || memcmp(data.dptr, op[i].data.dptr, data.dsize))
				errx(1, "Line %u: bad fetch", i+1);
			break;
		case OP_TDB_DELETE:
			try(tdb_delete(tdb, op[i].key), op[i]);
			break;
		case OP_TDB_CLOSE:
			errx(1, "Line %u: unexpected close", i+1);
			break;
		}
	}
	return i;
}

int main(int argc, char *argv[])
{
	const char *file;
	char **lines;
	unsigned int i;
	struct tdb_context *tdb = NULL;
	struct op *op = talloc_array(NULL, struct op, 1);
	struct timeval start, end;

	if (argc != 3)
		errx(1, "Usage: %s <tracefile> <tdbfile>", argv[0]);

	file = grab_file(NULL, argv[1], NULL);
	if (!file)
		err(1, "Reading %s", argv[1]);

	lines = strsplit(file, file, "\n", NULL);

	for (i = 0; lines[i]; i++) {
		char **words = strsplit(lines, lines[i], " ", NULL);
		if (!tdb && !streq(words[0], "tdb_open"))
			errx(1, "Line %u is not tdb_open", i+1);

		if (streq(words[0], "tdb_open")) {
			if (tdb)
				errx(1, "Line %u: tdb_open again?", i+1);
			tdb = tdb_open_ex(argv[2], atoi(words[2]),
					  strtoul(words[3], NULL, 0),
					  strtoul(words[4], NULL, 0), 0600,
					  NULL, hash_key);
			if (!tdb)
				err(1, "Opening tdb %s", argv[2]);
		} else if (streq(words[0], "tdb_lockall")) {
			add_op(&op, i, OP_TDB_LOCKALL, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_lockall_mark")) {
			add_op(&op, i, OP_TDB_LOCKALL_MARK, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_lockall_unmark")) {
			add_op(&op, i, OP_TDB_LOCKALL_UNMARK, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_lockall_nonblock")) {
			add_op(&op, i, OP_TDB_LOCKALL_NONBLOCK, NULL, NULL,
			       atoi(words[1]));
		} else if (streq(words[0], "tdb_unlockall")) {
			add_op(&op, i, OP_TDB_UNLOCKALL, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_lockall_read")) {
			add_op(&op, i, OP_TDB_LOCKALL_READ, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_lockall_read_nonblock")) {
			add_op(&op, i, OP_TDB_LOCKALL_READ_NONBLOCK, NULL, NULL,
			       atoi(words[1]));
		} else if (streq(words[0], "tdb_unlockall_read\n")) {
			add_op(&op, i, OP_TDB_UNLOCKALL_READ, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_chainlock")) {
			add_op(&op, i, OP_TDB_CHAINLOCK, words[1], NULL, 0);
		} else if (streq(words[0], "tdb_chainlock_nonblock")) {
			add_op(&op, i, OP_TDB_CHAINLOCK_NONBLOCK,
			       words[1], NULL, atoi(words[3]));
		} else if (streq(words[0], "tdb_chainlock_mark")) {
			add_op(&op, i, OP_TDB_CHAINLOCK_MARK, words[1], NULL,
			       0);
		} else if (streq(words[0], "tdb_chainlock_unmark")) {
			add_op(&op, i, OP_TDB_CHAINLOCK_UNMARK, words[1], NULL,
			       0);
		} else if (streq(words[0], "tdb_chainunlock")) {
			add_op(&op, i, OP_TDB_CHAINUNLOCK, words[1], NULL, 0);
		} else if (streq(words[0], "tdb_chainlock_read")) {
			add_op(&op, i, OP_TDB_CHAINLOCK_READ, words[1],
			       NULL, 0);
		} else if (streq(words[0], "tdb_chainunlock_read")) {
			add_op(&op, i, OP_TDB_CHAINUNLOCK_READ, words[1],
			       NULL, 0);
		} else if (streq(words[0], "tdb_close")) {
			add_op(&op, i, OP_TDB_CLOSE, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_increment_seqnum_nonblock")) {
			add_op(&op, i, OP_TDB_INCREMENT_SEQNUM_NONBLOCK,
			       NULL, NULL, 0);
		} else if (streq(words[0], "tdb_fetch")) {
			if (streq(words[3], "ENOENT"))
				add_op(&op, i, OP_TDB_FETCH, words[1], NULL,
				       -TDB_ERR_NOEXIST);
			else
				add_op(&op, i, OP_TDB_FETCH, words[1], words[3],
				       0);
		} else if (streq(words[0], "tdb_parse_record")) {
			if (streq(words[3], "ENOENT"))
				add_op(&op, i, OP_TDB_PARSE_RECORD,
				       words[1], NULL, -TDB_ERR_NOEXIST);
			else
				add_op(&op, i, OP_TDB_PARSE_RECORD,
				       words[1], NULL, atoi(words[3]));
		} else if (streq(words[0], "tdb_exists")) {
			add_op(&op, i, OP_TDB_EXISTS, words[1], NULL,
			       atoi(words[3]));
		} else if (streq(words[0], "tdb_delete")) {
			add_op(&op, i, OP_TDB_DELETE, words[1], NULL,
			       streq(words[3], "ENOENT")
			       ? -TDB_ERR_NOEXIST : 0);
		} else if (streq(words[0], "tdb_store")) {
			struct op *new;

			if (streq(words[5], "EEXIST"))
				new = add_op(&op, i, OP_TDB_STORE, words[2],
					     words[3], -TDB_ERR_EXISTS);
			else if (streq(words[5], "ENOENT"))
				new = add_op(&op, i, OP_TDB_STORE, words[2],
					     words[3], -TDB_ERR_NOEXIST);
			else
				new = add_op(&op, i, OP_TDB_STORE, words[2],
					     words[3], 0);
			if (streq(words[1], "insert"))
				new->flag = TDB_INSERT;
			else if (streq(words[1], "modify"))
				new->flag = TDB_MODIFY;
			else if (streq(words[1], "normal"))
				new->flag = 0;
			else
				errx(1, "Line %u: invalid tdb_store", i+1);
		} else if (streq(words[0], "tdb_append")) {
			add_op(&op, i, OP_TDB_APPEND, words[1], words[2], 0);
		} else if (streq(words[0], "tdb_get_seqnum")) {
			add_op(&op, i, OP_TDB_GET_SEQNUM, NULL, NULL,
			       atoi(words[2]));
		} else if (streq(words[0], "tdb_wipe_all")) {
			add_op(&op, i, OP_TDB_WIPE_ALL, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_transaction_start")) {
			add_op(&op, i, OP_TDB_TRANSACTION_START, NULL, NULL, 0);
		} else if (streq(words[0], "tdb_transaction_cancel")) {
			add_op(&op, i, OP_TDB_TRANSACTION_CANCEL, NULL, NULL,
			       0);
		} else if (streq(words[0], "tdb_transaction_commit")) {
			add_op(&op, i, OP_TDB_TRANSACTION_COMMIT, NULL, NULL,
			       0);
		} else if (streq(words[0], "tdb_traverse_read_start")) {
			add_op(&op, i, OP_TDB_TRAVERSE_READ_START, NULL, NULL,
			       0)->trav = NULL;
		} else if (streq(words[0], "tdb_traverse_start")) {
			add_op(&op, i, OP_TDB_TRAVERSE_START, NULL, NULL, 0)
				->trav = NULL;
		} else if (streq(words[0], "tdb_traverse_end")) {
			/* = %u means traverse function terminated. */
			if (words[1] == NULL)
				add_op(&op, i, OP_TDB_TRAVERSE_END, NULL, NULL,
				       0);
			else
				add_op(&op, i, OP_TDB_TRAVERSE_END, NULL, NULL,
				       atoi(words[2]));
			analyze_traverse(op, i);
		} else if (streq(words[0], "traverse")) {
			add_op(&op, i, OP_TDB_TRAVERSE, words[1], words[2], 0);
		} else if (streq(words[0], "tdb_firstkey")) {
			if (streq(words[2], "ENOENT"))
				add_op(&op, i, OP_TDB_FIRSTKEY, NULL, NULL,
				       -TDB_ERR_NOEXIST);
			else
				add_op(&op, i, OP_TDB_FIRSTKEY, NULL, words[2],
				       0);
		} else if (streq(words[0], "tdb_nextkey")) {
			if (streq(words[3], "ENOENT"))
				add_op(&op, i, OP_TDB_NEXTKEY, words[1], NULL,
				       -TDB_ERR_NOEXIST);
			else
				add_op(&op, i, OP_TDB_NEXTKEY,
				       words[1], words[3], 0);
		} else
			errx(1, "Line %u: unknown op '%s'", i+1, words[0]);
	}

	printf("Successfully input %u lines\n", i);
	gettimeofday(&start, NULL);
	run_ops(tdb, op, 1, i-1);
	gettimeofday(&end, NULL);
	if (op[i-1].op != OP_TDB_CLOSE)
		warnx("Last operation is not tdb_close: incomplete?");
	tdb_close(tdb);
	end.tv_sec -= start.tv_sec;
	printf("Time replaying: %lu usec\n",
	       end.tv_sec * 1000000UL + (end.tv_usec - start.tv_usec));
	exit(0);
}

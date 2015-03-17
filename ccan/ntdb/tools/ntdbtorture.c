/* this tests ntdb by doing lots of ops from several simultaneous
   writers - that stresses the locking code.
*/

#include "config.h"
#include "ntdb.h"
#include "private.h"
#include <ccan/err/err.h>

//#define REOPEN_PROB 30
#define DELETE_PROB 8
#define STORE_PROB 4
#define APPEND_PROB 6
#define TRANSACTION_PROB 10
#define TRANSACTION_PREPARE_PROB 2
#define LOCKSTORE_PROB 5
#define TRAVERSE_PROB 20
#define TRAVERSE_MOD_PROB 100
#define TRAVERSE_ABORT_PROB 500
#define CULL_PROB 100
#define KEYLEN 3
#define DATALEN 100

static struct ntdb_context *db;
static int in_transaction;
static int in_traverse;
static int error_count;
#if TRANSACTION_PROB
static int always_transaction = 0;
#endif
static int loopnum;
static int count_pipe;
static union ntdb_attribute log_attr;
static union ntdb_attribute seed_attr;
static union ntdb_attribute hsize_attr;

static void ntdb_log(struct ntdb_context *ntdb,
		    enum ntdb_log_level level,
		    enum NTDB_ERROR ecode,
		    const char *message,
		    void *data)
{
	printf("ntdb:%s:%s:%s\n",
	       ntdb_name(ntdb), ntdb_errorstr(ecode), message);
	fflush(stdout);
#if 0
	{
		char str[200];
		signal(SIGUSR1, SIG_IGN);
		sprintf(str,"xterm -e gdb /proc/%u/exe %u", (unsigned int)getpid(), (unsigned int)getpid());
		system(str);
	}
#endif
}

#include "../private.h"

static void segv_handler(int sig, siginfo_t *info, void *p)
{
	char string[100];

	sprintf(string, "%u: death at %p (map_ptr %p, map_size %zu)\n",
		(unsigned int)getpid(), info->si_addr, db->file->map_ptr,
		(size_t)db->file->map_size);
	if (write(2, string, strlen(string)) > 0)
		sleep(60);
	_exit(11);
}

static void warn_on_err(enum NTDB_ERROR e, struct ntdb_context *ntdb,
			const char *why)
{
	if (e != NTDB_SUCCESS) {
		fprintf(stderr, "%u:%s:%s\n", (unsigned int)getpid(), why,
			ntdb ? ntdb_errorstr(e) : "(no ntdb)");
		error_count++;
	}
}

static char *randbuf(int len)
{
	char *buf;
	int i;
	buf = (char *)malloc(len+1);
	if (buf == NULL) {
		perror("randbuf: unable to allocate memory for buffer.\n");
		exit(1);
	}

	for (i=0;i<len;i++) {
		buf[i] = 'a' + (rand() % 26);
	}
	buf[i] = 0;
	return buf;
}

static void addrec_db(void);
static int modify_traverse(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf,
			   void *state)
{
#if CULL_PROB
	if (random() % CULL_PROB == 0) {
		ntdb_delete(ntdb, key);
	}
#endif

#if TRAVERSE_MOD_PROB
	if (random() % TRAVERSE_MOD_PROB == 0) {
		addrec_db();
	}
#endif

#if TRAVERSE_ABORT_PROB
	if (random() % TRAVERSE_ABORT_PROB == 0)
		return 1;
#endif

	return 0;
}

static void addrec_db(void)
{
	int klen, dlen;
	char *k, *d;
	NTDB_DATA key, data;
	enum NTDB_ERROR e;

	klen = 1 + (rand() % KEYLEN);
	dlen = 1 + (rand() % DATALEN);

	k = randbuf(klen);
	d = randbuf(dlen);

	key.dptr = (unsigned char *)k;
	key.dsize = klen+1;

	data.dptr = (unsigned char *)d;
	data.dsize = dlen+1;

#if REOPEN_PROB
	if (in_traverse == 0 && in_transaction == 0 && random() % REOPEN_PROB == 0) {
		ntdb_reopen_all(0);
		goto next;
	}
#endif

#if TRANSACTION_PROB
	if (in_traverse == 0 && in_transaction == 0 && (always_transaction || random() % TRANSACTION_PROB == 0)) {
		e = ntdb_transaction_start(db);
		warn_on_err(e, db, "ntdb_transaction_start failed");
		in_transaction++;
		goto next;
	}
	if (in_traverse == 0 && in_transaction && random() % TRANSACTION_PROB == 0) {
		if (random() % TRANSACTION_PREPARE_PROB == 0) {
			e = ntdb_transaction_prepare_commit(db);
			warn_on_err(e, db, "ntdb_transaction_prepare_commit failed");
		}
		e = ntdb_transaction_commit(db);
		warn_on_err(e, db, "ntdb_transaction_commit failed");
		in_transaction--;
		goto next;
	}

	if (in_traverse == 0 && in_transaction && random() % TRANSACTION_PROB == 0) {
		ntdb_transaction_cancel(db);
		in_transaction--;
		goto next;
	}
#endif

#if DELETE_PROB
	if (random() % DELETE_PROB == 0) {
		ntdb_delete(db, key);
		goto next;
	}
#endif

#if STORE_PROB
	if (random() % STORE_PROB == 0) {
		e = ntdb_store(db, key, data, NTDB_REPLACE);
		warn_on_err(e, db, "ntdb_store failed");
		goto next;
	}
#endif

#if APPEND_PROB
	if (random() % APPEND_PROB == 0) {
		e = ntdb_append(db, key, data);
		warn_on_err(e, db, "ntdb_append failed");
		goto next;
	}
#endif

#if LOCKSTORE_PROB
	if (random() % LOCKSTORE_PROB == 0) {
		ntdb_chainlock(db, key);
		if (ntdb_fetch(db, key, &data) != NTDB_SUCCESS) {
			data.dsize = 0;
			data.dptr = NULL;
		}
		e = ntdb_store(db, key, data, NTDB_REPLACE);
		warn_on_err(e, db, "ntdb_store failed");
		if (data.dptr) free(data.dptr);
		ntdb_chainunlock(db, key);
		goto next;
	}
#endif

#if TRAVERSE_PROB
	/* FIXME: recursive traverses break transactions? */
	if (in_traverse == 0 && random() % TRAVERSE_PROB == 0) {
		in_traverse++;
		ntdb_traverse(db, modify_traverse, NULL);
		in_traverse--;
		goto next;
	}
#endif

	if (ntdb_fetch(db, key, &data) == NTDB_SUCCESS)
		free(data.dptr);

next:
	free(k);
	free(d);
}

static int traverse_fn(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf,
                       void *state)
{
	ntdb_delete(ntdb, key);
	return 0;
}

static void usage(void)
{
	printf("Usage: ntdbtorture"
#if TRANSACTION_PROB
	       " [-t]"
#endif
	       " [-k] [-n NUM_PROCS] [-l NUM_LOOPS] [-s SEED] [-S] [-H HASH_SIZE]\n");
	exit(0);
}

static void send_count_and_suicide(int sig)
{
	/* This ensures our successor can continue where we left off. */
	if (write(count_pipe, &loopnum, sizeof(loopnum)) != sizeof(loopnum))
		exit(2);
	/* This gives a unique signature. */
	kill(getpid(), SIGUSR2);
}

static int run_child(const char *filename, int i, int seed, unsigned num_loops,
		     unsigned start, int ntdb_flags)
{
	struct sigaction act = { .sa_sigaction = segv_handler,
				 .sa_flags = SA_SIGINFO };
	sigaction(11, &act, NULL);

	db = ntdb_open(filename, ntdb_flags, O_RDWR | O_CREAT, 0600,
		      &log_attr);
	if (!db) {
		fprintf(stderr, "%u:%s:%s\n", (unsigned int)getpid(), filename,
			"db open failed");
		exit(1);
	}

#if 0
	if (i == 0) {
		printf("pid %u\n", (unsigned int)getpid());
		sleep(9);
	} else
		sleep(10);
#endif

	srand(seed + i);
	srandom(seed + i);

	/* Set global, then we're ready to handle being killed. */
	loopnum = start;
	signal(SIGUSR1, send_count_and_suicide);

	for (;loopnum<num_loops && error_count == 0;loopnum++) {
		addrec_db();
	}

	if (error_count == 0) {
		enum NTDB_ERROR e;

		ntdb_traverse(db, NULL, NULL);
#if TRANSACTION_PROB
		if (always_transaction) {
			while (in_transaction) {
				ntdb_transaction_cancel(db);
				in_transaction--;
			}
			e = ntdb_transaction_start(db);
			if (e) {
				warn_on_err(e, db,
					    "ntdb_transaction_start failed");
				exit(1);
			}
		}
#endif
		ntdb_traverse(db, traverse_fn, NULL);
		ntdb_traverse(db, traverse_fn, NULL);

#if TRANSACTION_PROB
		if (always_transaction) {
			e = ntdb_transaction_commit(db);
			warn_on_err(e, db, "ntdb_transaction_commit failed");
		}
#endif
	}

	ntdb_close(db);

	return (error_count < 100 ? error_count : 100);
}

static char *test_path(const char *filename)
{
	const char *prefix = getenv("TEST_DATA_PREFIX");

	if (prefix) {
		char *path = NULL;
		int ret;

		ret = asprintf(&path, "%s/%s", prefix, filename);
		if (ret == -1) {
			return NULL;
		}
		return path;
	}

	return strdup(filename);
}

int main(int argc, char * const *argv)
{
	int i, seed = -1;
	int num_loops = 5000;
	int num_procs = 3;
	int c, pfds[2];
	extern char *optarg;
	pid_t *pids;
	int kill_random = 0;
	int *done;
	int ntdb_flags = NTDB_DEFAULT;
	char *test_ntdb;
	enum NTDB_ERROR e;

	log_attr.base.attr = NTDB_ATTRIBUTE_LOG;
	log_attr.base.next = &seed_attr;
	log_attr.log.fn = ntdb_log;
	seed_attr.base.attr = NTDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &hsize_attr;
	hsize_attr.base.attr = NTDB_ATTRIBUTE_HASHSIZE;
	hsize_attr.base.next = NULL;
	hsize_attr.hashsize.size = 2; /* stress it by default. */

	while ((c = getopt(argc, argv, "n:l:s:thkSH:")) != -1) {
		switch (c) {
		case 'n':
			num_procs = strtol(optarg, NULL, 0);
			break;
		case 'l':
			num_loops = strtol(optarg, NULL, 0);
			break;
		case 's':
			seed = strtol(optarg, NULL, 0);
			break;
		case 'S':
			ntdb_flags = NTDB_NOSYNC;
			break;
		case 't':
#if TRANSACTION_PROB
			always_transaction = 1;
#else
			fprintf(stderr, "Transactions not supported\n");
			usage();
#endif
			break;
		case 'k':
			kill_random = 1;
			break;
		case 'H':
			hsize_attr.hashsize.size = strtol(optarg, NULL, 0);
			break;
		default:
			usage();
		}
	}

	test_ntdb = test_path("torture.ntdb");

	unlink(test_ntdb);

	if (seed == -1) {
		seed = (getpid() + time(NULL)) & 0x7FFFFFFF;
	}
	seed_attr.seed.seed = (((uint64_t)seed) << 32) | seed;

	if (num_procs == 1 && !kill_random) {
		/* Don't fork for this case, makes debugging easier. */
		error_count = run_child(test_ntdb, 0, seed, num_loops, 0,
					ntdb_flags);
		goto done;
	}

	pids = (pid_t *)calloc(sizeof(pid_t), num_procs);
	done = (int *)calloc(sizeof(int), num_procs);

	if (pipe(pfds) != 0) {
		perror("Creating pipe");
		exit(1);
	}
	count_pipe = pfds[1];

	for (i=0;i<num_procs;i++) {
		if ((pids[i]=fork()) == 0) {
			close(pfds[0]);
			if (i == 0) {
				printf("testing with %d processes, %d loops, seed=%d%s\n",
				       num_procs, num_loops, seed,
#if TRANSACTION_PROB
				       always_transaction ? " (all within transactions)" : ""
#else
				       ""
#endif
					);
			}
			exit(run_child(test_ntdb, i, seed, num_loops, 0,
				       ntdb_flags));
		}
	}

	while (num_procs) {
		int status, j;
		pid_t pid;

		if (error_count != 0) {
			/* try and stop the test on any failure */
			for (j=0;j<num_procs;j++) {
				if (pids[j] != 0) {
					kill(pids[j], SIGTERM);
				}
			}
		}

		pid = waitpid(-1, &status, kill_random ? WNOHANG : 0);
		if (pid == 0) {
			struct timespec ts;

			/* Sleep for 1/10 second. */
			ts.tv_sec = 0;
			ts.tv_nsec = 100000000;
			nanosleep(&ts, NULL);

			/* Kill someone. */
			kill(pids[random() % num_procs], SIGUSR1);
			continue;
		}

		if (pid == -1) {
			perror("failed to wait for child\n");
			exit(1);
		}

		for (j=0;j<num_procs;j++) {
			if (pids[j] == pid) break;
		}
		if (j == num_procs) {
			printf("unknown child %d exited!?\n", (int)pid);
			exit(1);
		}
		if (WIFSIGNALED(status)) {
			if (WTERMSIG(status) == SIGUSR2
			    || WTERMSIG(status) == SIGUSR1) {
				/* SIGUSR2 means they wrote to pipe. */
				if (WTERMSIG(status) == SIGUSR2) {
					if (read(pfds[0], &done[j],
						 sizeof(done[j]))
					    != sizeof(done[j]))
						err(1,
						    "Short read from child?");
				}
				pids[j] = fork();
				if (pids[j] == 0)
					exit(run_child(test_ntdb, j, seed,
						       num_loops, done[j],
						       ntdb_flags));
				printf("Restarting child %i for %u-%u\n",
				       j, done[j], num_loops);
				continue;
			}
			printf("child %d exited with signal %d\n",
			       (int)pid, WTERMSIG(status));
			error_count++;
		} else {
			if (WEXITSTATUS(status) != 0) {
				printf("child %d exited with status %d\n",
				       (int)pid, WEXITSTATUS(status));
				error_count++;
			}
		}
		memmove(&pids[j], &pids[j+1],
			(num_procs - j - 1)*sizeof(pids[0]));
		num_procs--;
	}

	free(pids);

done:
	if (error_count == 0) {
		db = ntdb_open(test_ntdb, NTDB_DEFAULT, O_RDWR | O_CREAT,
			      0600, &log_attr);
		if (!db) {
			fprintf(stderr, "%u:%s:%s\n", (unsigned int)getpid(), test_ntdb,
				"db open failed");
			exit(1);
		}
		e = ntdb_check(db, NULL, NULL);
		if (e) {
			warn_on_err(e, db, "db check failed");
			exit(1);
		}
		ntdb_close(db);
		printf("OK\n");
	}

	free(test_ntdb);
	return error_count;
}

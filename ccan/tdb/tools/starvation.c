/* Demonstrate starvation of tdb_lockall */
#include <ccan/tdb/tdb.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

static void usage(const char *extra)
{
	errx(1, "%s%s"
	     "Usage: starvation [lockall|gradual] <num> <worktime-in-ms>\n"
	     "  Each locker holds lock for between 1/2 and 1 1/2 times\n"
	     "  worktime, then sleeps for one second.\n\n"
	     "  Main process tries tdb_lockall or tdb_lockall_gradual.",
	     extra ? extra : "", extra ? "\n" : "");
}

static void run_and_sleep(struct tdb_context *tdb, int parentfd, unsigned time)
{
	char c;
	struct timespec hold;
	unsigned rand, randtime;
	TDB_DATA key;

	key.dptr = (void *)&rand;
	key.dsize = sizeof(rand);

	while (read(parentfd, &c, 1) != 0) {
		/* Lock a random key. */
		rand = random();
		if (tdb_chainlock(tdb, key) != 0)
			errx(1, "chainlock failed: %s", tdb_errorstr(tdb));

		/* Hold it for some variable time. */
		randtime = time / 2 + (random() % time);
		hold.tv_sec = randtime / 1000;
		hold.tv_nsec = (randtime % 1000) * 1000000;
		nanosleep(&hold, NULL);

		if (tdb_chainunlock(tdb, key) != 0)
			errx(1, "chainunlock failed: %s", tdb_errorstr(tdb));

		/* Wait for a second without the lock. */
		sleep(1);
	}
	exit(0);
}

static void logfn(struct tdb_context *tdb,
		  enum tdb_debug_level level,
		  const char *fmt, ...)
{
           va_list ap;

           va_start(ap, fmt);
	   vfprintf(stderr, fmt, ap);
	   va_end(ap);
}

int main(int argc, char *argv[])
{
	int (*lockall)(struct tdb_context *);
	unsigned int num, worktime, i;
	int pfd[2];
	struct tdb_context *tdb;
	struct tdb_logging_context log = { logfn, NULL };
	struct timeval start, end, duration;

	if (argc != 4)
		usage(NULL);

	if (strcmp(argv[1], "lockall") == 0)
		lockall = tdb_lockall;
	else if (strcmp(argv[1], "gradual") == 0) {
#if 0
		lockall = tdb_lockall_gradual;
#else
		errx(1, "gradual is now the default implementation");
#endif
	} else
		usage("Arg1 should be 'lockall' or 'gradual'");

	num = atoi(argv[2]);
	worktime = atoi(argv[3]);

	if (!num || !worktime)
		usage("Number of threads and worktime must be non-zero");

	if (pipe(pfd) != 0)
		err(1, "Creating pipe");

	tdb = tdb_open_ex("/tmp/starvation.tdb", 10000, TDB_DEFAULT,
			  O_RDWR|O_CREAT|O_TRUNC, 0600, &log, NULL);
	if (!tdb)
		err(1, "Opening tdb /tmp/starvation.tdb");

	for (i = 0; i < num; i++) {
		switch (fork()) {
		case 0:
			close(pfd[1]);
			fcntl(pfd[0], F_SETFL,
			      fcntl(pfd[0], F_GETFL)|O_NONBLOCK);
			srandom(getpid() + i);
			if (tdb_reopen(tdb) != 0)
				err(1, "Reopening tdb %s", tdb_name(tdb));

			run_and_sleep(tdb, pfd[0], worktime);
		case -1:
			err(1, "forking");
		}
		/* Stagger the children. */
		usleep(random() % (1000000 / num));
	}

	close(pfd[0]);
	sleep(1);
	gettimeofday(&start, NULL);
	if (lockall(tdb) != 0)
		errx(1, "lockall failed: %s", tdb_errorstr(tdb));
	gettimeofday(&end, NULL);

	duration.tv_sec = end.tv_sec - start.tv_sec;
	duration.tv_usec = end.tv_usec - start.tv_usec;
	if (duration.tv_usec < 0) {
		--duration.tv_sec;
		duration.tv_usec += 1000000;
	}

	if (tdb_unlockall(tdb) != 0)
		errx(1, "unlockall failed: %s", tdb_errorstr(tdb));
	tdb_close(tdb);
	unlink("/tmp/starvation.tdb");

	printf("Took %lu.%06lu seconds\n", duration.tv_sec, duration.tv_usec);
	return 0;
}

#include "ccanlint.h"
#include "../tools.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <assert.h>
#include <ccan/lbalance/lbalance.h>
#include <ccan/tlist/tlist.h>
#include <ccan/time/time.h>

static struct lbalance *lb;
TLIST_TYPE(command, struct command);
static struct tlist_command pending = TLIST_INIT(pending);
static struct tlist_command running = TLIST_INIT(running);
static unsigned int num_running = 0;
static struct tlist_command done = TLIST_INIT(done);

struct command {
	struct list_node list;
	char *command;
	pid_t pid;
	int output_fd;
	unsigned int time_ms;
	struct lbalance_task *task;
	int status;
	char *output;
	bool done;
	const void *ctx;
};

static void killme(int sig UNNEEDED)
{
	kill(-getpid(), SIGKILL);
}

static void run_more(void)
{
	struct command *c;

	while (num_running < lbalance_target(lb)) {
		int p[2];

		c = tlist_top(&pending, list);
		if (!c)
			break;

		fflush(stdout);
		if (pipe(p) != 0)
			err(1, "Pipe failed");
		c->pid = fork();
		if (c->pid == -1)
			err(1, "Fork failed");
		if (c->pid == 0) {
			struct itimerval itim;

			if (dup2(p[1], STDOUT_FILENO) != STDOUT_FILENO
			    || dup2(p[1], STDERR_FILENO) != STDERR_FILENO
			    || close(p[0]) != 0
			    || close(STDIN_FILENO) != 0
			    || open("/dev/null", O_RDONLY) != STDIN_FILENO)
				exit(128);

			signal(SIGALRM, killme);
			itim.it_interval.tv_sec = itim.it_interval.tv_usec = 0;
			itim.it_value = timespec_to_timeval(time_from_msec(c->time_ms).ts);
			setitimer(ITIMER_REAL, &itim, NULL);

			c->status = system(c->command);
			if (WIFEXITED(c->status))
				exit(WEXITSTATUS(c->status));
			/* Here's a hint... */
			exit(128 + WTERMSIG(c->status));
		}

		if (tools_verbose)
			printf("Running async: %s => %i\n", c->command, c->pid);

		close(p[1]);
		c->output_fd = p[0];
		c->task = lbalance_task_new(lb);
		tlist_del_from(&pending, c, list);
		tlist_add_tail(&running, c, list);
		num_running++;
	}
}

static void destroy_command(struct command *command)
{
	if (!command->done && command->pid) {
		kill(-command->pid, SIGKILL);
		close(command->output_fd);
		num_running--;
	}

	tlist_del(command, list);
}

void run_command_async(const void *ctx, unsigned int time_ms,
		       const char *fmt, ...)
{
	struct command *command;
	va_list ap;

	assert(ctx);

	if (!lb)
		lb = lbalance_new();

	command = tal(ctx, struct command);
	command->ctx = ctx;
	command->time_ms = time_ms;
	command->pid = 0;
	/* We want to track length, so don't use tal_strdup */
	command->output = tal_arrz(command, char, 1);
	va_start(ap, fmt);
	command->command = tal_vfmt(command, fmt, ap);
	va_end(ap);
	tlist_add_tail(&pending, command, list);
	command->done = false;
	tal_add_destructor(command, destroy_command);

	run_more();
}

static void reap_output(void)
{
	fd_set in;
	struct command *c, *next;
	int max_fd = 0;

	FD_ZERO(&in);

	tlist_for_each(&running, c, list) {
		FD_SET(c->output_fd, &in);
		if (c->output_fd > max_fd)
			max_fd = c->output_fd;
	}

	if (select(max_fd+1, &in, NULL, NULL, NULL) < 0)
		err(1, "select failed");

	tlist_for_each_safe(&running, c, next, list) {
		if (FD_ISSET(c->output_fd, &in)) {
			int old_len, len;
			/* This length includes nul terminator! */
			old_len = tal_count(c->output);
			tal_resize(&c->output, old_len + 1024);
			len = read(c->output_fd, c->output + old_len - 1, 1024);
			if (len < 0)
				err(1, "Reading from async command");
			tal_resize(&c->output, old_len + len);
			c->output[old_len + len - 1] = '\0';
			if (len == 0) {
				struct rusage ru;
				wait4(c->pid, &c->status, 0, &ru);
				if (tools_verbose)
					printf("Finished async %i: %s %u\n",
					       c->pid,
					       WIFEXITED(c->status)
					       ? "exit status"
					       : "killed by signal",
					       WIFEXITED(c->status)
					       ? WEXITSTATUS(c->status)
					       : WTERMSIG(c->status));
				lbalance_task_free(c->task, &ru);
				c->task = NULL;
				c->done = true;
				close(c->output_fd);
				tlist_del_from(&running, c, list);
				tlist_add_tail(&done, c, list);
				num_running--;
			}
		}
	}
}

void *collect_command(bool *ok, char **output)
{
	struct command *c;
	const void *ctx;

	while ((c = tlist_top(&done, list)) == NULL) {
		if (tlist_empty(&pending) && tlist_empty(&running))
			return NULL;
		reap_output();
		run_more();
	}

	*ok = (WIFEXITED(c->status) && WEXITSTATUS(c->status) == 0);
	ctx = c->ctx;
	*output = tal_steal(ctx, c->output);
	tal_free(c);
	return (void *)ctx;
}

/* Compile and link single C file, with object files, async. */
void compile_and_link_async(const void *ctx, unsigned int time_ms,
			    const char *cfile, const char *ccandir,
			    const char *objs, const char *compiler,
			    const char *cflags,
			    const char *libs, const char *outfile)
{
	if (compile_verbose)
		printf("Compiling and linking (async) %s\n", outfile);
	run_command_async(ctx, time_ms,
			  "%s %s -I%s -o %s %s %s %s",
			  compiler, cflags,
			  ccandir, outfile, cfile, objs, libs);
}

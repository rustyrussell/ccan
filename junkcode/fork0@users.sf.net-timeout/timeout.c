/* execute a program with a timeout by alarm(2) */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static const char *argv0;
static char **prgargv;
static pid_t pid;
static int signo;
static unsigned int timeout;

static void timedout(int sig)
{
	fprintf(stderr, "%s[%d]: %s[%d] timed out after %u sec\n",
		argv0, getpid(), *prgargv, pid, timeout);
	if (pid)
		kill(-pid, signo);
}

static void interrupted(int sig)
{
	alarm(0);
	if (pid)
		kill(-pid, sig);
}

static void usage()
{
	fprintf(stderr, "%s <timeout-seconds> [-<signal>] program ...\n"
		"Where <signal> is a signal number (see kill -l).\n"
		"Some symbolic names recognized. KILL used by default\n",
		argv0);
	exit(1);
}

static struct {
	const char *name;
	int signo;
} known[] = {
	{"HUP",	    SIGHUP},
	{"INT",	    SIGINT},
	{"QUIT",    SIGQUIT},
	{"ILL",	    SIGILL},
	{"TRAP",    SIGTRAP},
	{"ABRT",    SIGABRT},
	{"BUS",	    SIGBUS},
	{"FPE",	    SIGFPE},
	{"KILL",    SIGKILL},
	{"USR1",    SIGUSR1},
	{"SEGV",    SIGSEGV},
	{"USR2",    SIGUSR2},
	{"PIPE",    SIGPIPE},
	{"ALRM",    SIGALRM},
	{"TERM",    SIGTERM},
	{"STKFLT",  SIGSTKFLT},
	{"CHLD",    SIGCHLD},
	{"CONT",    SIGCONT},
	{"STOP",    SIGSTOP},
	{"TSTP",    SIGTSTP},
	{"TTIN",    SIGTTIN},
	{"TTOU",    SIGTTOU},
	{"URG",	    SIGURG},
	{"XCPU",    SIGXCPU},
	{"XFSZ",    SIGXFSZ},
	{"VTALRM",  SIGVTALRM},
	{"PROF",    SIGPROF},
	{"WINCH",   SIGWINCH},
	{"IO",	    SIGIO},
	{"PWR",	    SIGPWR},
	{"SYS",     SIGSYS},
};

static int signo_arg(const char *arg)
{
	if (*arg == '-') {
		char *p;
		int s = strtol(++arg, &p, 10);
		if (!*p && p > arg && s > 0 && s < _NSIG) {
			signo = s;
			return 1;
		}
		if (!strncasecmp(arg, "SIG", 3))
			arg += 3;
		for (s = 0; s < sizeof(known)/sizeof(*known); ++s)
			if (!strcasecmp(arg, known[s].name)) {
				signo = known[s].signo;
				return 1;
			}
	}
	return 0;
}

int main(int argc, char** argv)
{
	argv0 = strrchr(*argv, '/');
	if (argv0)
		++argv0;
	else
		argv0 = *argv;

	signal(SIGALRM, timedout);
	signal(SIGINT, interrupted);
	signal(SIGHUP, interrupted);

	++argv;

	if (!*argv)
		usage();

	if (signo_arg(*argv))
		++argv;
	if (sscanf(*argv, "%u", &timeout) == 1)
		++argv;
	else
		usage();
	if (!signo && signo_arg(*argv))
		++argv;
	if (!signo)
		signo = SIGKILL;

	if (!*argv)
		usage();

	prgargv = argv;
	alarm(timeout);
	pid = fork();

	if (!pid) {
		signal(SIGALRM, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		setpgid(0, 0);
		execvp(*prgargv, prgargv);
		fprintf(stderr, "%s: %s: %s\n",
			argv0, *prgargv, strerror(errno));
		_exit(2);
	} else if (pid < 0) {
		fprintf(stderr, "%s: %s\n", argv0, strerror(errno));
	} else {
		int status;
		while (waitpid(pid, &status, 0) < 0 && EINTR == errno)
			;
		alarm(0);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		if (WIFSIGNALED(status)) {
			/*
			 * Some signals are special, lets die with
			 * the same signal as child process
			 */
			if (WTERMSIG(status) == SIGHUP  ||
			    WTERMSIG(status) == SIGINT  ||
			    WTERMSIG(status) == SIGTERM ||
			    WTERMSIG(status) == SIGQUIT ||
			    WTERMSIG(status) == SIGKILL) {
				signal(WTERMSIG(status), SIG_DFL);
				raise(WTERMSIG(status));
			}
			fprintf(stderr, "%s: %s: %s\n",
				argv0, *prgargv, strsignal(WTERMSIG(status)));
		}
		else
			fprintf(stderr, "%s died\n", *prgargv);
	}
	return 2;
}

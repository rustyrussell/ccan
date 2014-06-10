#include <ccan/lbalance/lbalance.h>
#include <ccan/lbalance/lbalance.c>
#include <ccan/time/time.h>
#include <ccan/jmap/jmap.h>
#include <stdio.h>
#include <err.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

struct jmap_task {
	JMAP_MEMBERS(unsigned int, struct lbalance_task *);
};

/* Figure out how many loops we need to run for about 1 second. */
static unsigned long burn_count;

static void calibrate_burn_cpu(void)
{
	struct timeabs start = time_now();

	while (time_before(time_now(),
			   timeabs_add(start, time_from_msec(1000))))
		burn_count++;
	printf("Burn count = %lu\n", burn_count);
}

static void burn_cpu(void)
{
	unsigned int i, after = 0;
	struct timeabs start = time_now();

	/* We do a loop similar to the calibrate_burn_cpu loop. */ 
	for (i = 0; i < burn_count; i++) {
		after += time_before(time_now(),
				     timeabs_add(start, time_from_msec(1000)));
	}
	/* We use the result so the compiler can't discard it. */
	exit(after);
}

static pid_t spawn(char *args[])
{
	pid_t pid = fork();

	if (pid == -1)
		err(1, "forking");
	if (pid == 0) {
		if (!args[0])
			burn_cpu();
		execvp(args[0], args);
		err(1, "exec failed");
	}
	return pid;
}

int main(int argc, char *argv[])
{
	unsigned int num, fixed_target = 0, num_done = 0, num_running = 0;
	struct lbalance *lb;
	struct jmap_task *tasks = jmap_new(struct jmap_task);

	if (argc < 2) {
		fprintf(stderr,
			"Usage: lbalance --fixed=<num> <num> [<command>...]\n"
			"OR: lbalance <num> [<command>...]\n");
		exit(1);
	}

	if (strncmp(argv[1], "--fixed=", strlen("--fixed=")) == 0) {
		fixed_target = atoi(argv[1] + strlen("--fixed="));
		if (!fixed_target)
			errx(1, "Need positive number after --fixed");
		argv++;
		argc--;
		lb = NULL;
	} else {
		lb = lbalance_new();
	}
	num = atoi(argv[1]);
	argv++;
	argc--;

	if (!argv[1])
		calibrate_burn_cpu();

	while (num_done < num) {
		unsigned int target = fixed_target;
		struct lbalance_task *task;
		struct rusage ru;
		pid_t pid;

		if (lb) {
			target = lbalance_target(lb);
			printf("(%u)", target);
		}

		while (num_running < target && num_done + num_running < num) {
			pid = spawn(argv+1);
			if (lb)
				task = lbalance_task_new(lb);
			else
				task = (void *)1;
			jmap_add(tasks, pid, task);
			num_running++;
			printf("+"); fflush(stdout);
		}

		/* Now wait for something to die! */
		pid = wait3(NULL, 0, &ru);
		task = jmap_get(tasks, pid);
		if (lb)
			lbalance_task_free(task, &ru);
		num_done++;
		num_running--;
		printf("-"); fflush(stdout);
	}
	printf("\n");
	if (lb)
		lbalance_free(lb);
	return 0;
}

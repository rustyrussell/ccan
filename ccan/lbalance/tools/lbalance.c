#include <ccan/lbalance/lbalance.h>
#include <ccan/lbalance/lbalance.c>
#include <ccan/time/time.h>
#include <ccan/jmap/jmap_type.h>
#include <stdio.h>
#include <err.h>

/* Defines struct jmap_task. */
JMAP_DEFINE_UINTIDX_TYPE(struct lbalance_task, task);

/* Figure out how many loops we need to run for about 1 second. */
static unsigned long burn_count;

static void calibrate_burn_cpu(void)
{
	struct timeval start = time_now();

	while (time_less(time_now(), time_add(start, time_from_msec(1000))))
		burn_count++;
	printf("Burn count = %lu\n", burn_count);
}

static void burn_cpu(void)
{
	unsigned int i, after = 0;
	struct timeval start = time_now();

	/* We do a loop similar to the calibrate_burn_cpu loop. */ 
	for (i = 0; i < burn_count; i++) {
		after += time_less(time_now(),
				   time_add(start, time_from_msec(1000)));
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
	unsigned int i, num, fixed_target = 0, num_done = 0, num_running = 0;
	struct lbalance *lb;
	struct jmap_task *tasks = jmap_task_new();

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
		unsigned int j, target = fixed_target;
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
			jmap_task_add(tasks, pid, task);
			num_running++;
			printf("+"); fflush(stdout);
		}

		/* Now wait for something to die! */
		pid = wait3(NULL, 0, &ru);
		task = jmap_task_get(tasks, pid);
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

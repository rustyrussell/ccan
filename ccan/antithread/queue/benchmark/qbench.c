/* How long does it take to pass 100M pointers? */
#define _GNU_SOURCE
#include <sched.h>
#include <ccan/antithread/antithread.h>
#include <ccan/antithread/queue/queue.h>
#include <ccan/err/err.h>
#include <ccan/time/time.h>
#include <stdio.h>

static unsigned int prod_num, cons_num;

static void bind_to_cpu(struct at_parent *parent, void *cpu)
{
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET((unsigned long)cpu, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
		err(1, "sched_setaffinity(%lu)", (long)cpu);
	at_tell_parent(parent, parent);
}

static void *produce(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
		queue_insert(q, (void *)i);
		sum += i;
	}
	return (void *)sum;
}

static void *consume(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++)
		ret -= (unsigned long)queue_remove(q);
	return (void *)ret;
}

static void *produce_excl(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
		queue_insert_excl(q, (void *)i);
		sum += i;
	}
	return (void *)sum;
}

static void *consume_excl(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++)
		ret -= (unsigned long)queue_remove_excl(q);
	return (void *)ret;
}

static void lock(unsigned int *lptr)
{
	while (__atomic_test_and_set(lptr, __ATOMIC_ACQUIRE));
}

static void unlock(unsigned int *lptr)
{
	__atomic_clear(lptr, __ATOMIC_RELEASE);
}

static void *dumb_produce(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
	again:
		lock(&q->prod_lock);
		if (q->head == q->tail + QUEUE_ELEMS) {
			unlock(&q->prod_lock);
			while (q->head == __atomic_load_n(&q->tail, __ATOMIC_RELAXED) + QUEUE_ELEMS);
			goto again;
		}
		q->elems[q->head % QUEUE_ELEMS] = (void *)i;
		q->head++;
		unlock(&q->prod_lock);
		sum += i;
	}

	return (void *)sum;
}

static void *dumb_consume(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++) {
	again:
		lock(&q->prod_lock);
		if (q->head == q->tail) {
			unlock(&q->prod_lock);
			while (q->tail == __atomic_load_n(&q->head, __ATOMIC_RELAXED));
			goto again;
		}
		ret -= (unsigned long)q->elems[q->tail % QUEUE_ELEMS];
		q->tail++;
		unlock(&q->prod_lock);
	}

	return (void *)ret;
}

int main(int argc, char *argv[])
{
	struct at_pool *pool;
	struct queue *q;
	unsigned long i, num, prods, cons, total;
	struct timespec start, end;
	char numstr[50];
	struct at_child **children;
	void *(*consfn)(struct at_parent *parent, void *cpu) = consume;
	void *(*prodfn)(struct at_parent *parent, void *cpu) = produce;
	char **orig_args = argv;

	if (argv[1] && strcmp(argv[1], "--xprod") == 0) {
		prodfn = produce_excl;
		argv++;
		argc--;
	}
	if (argv[1] && strcmp(argv[1], "--xcons") == 0) {
		consfn = consume_excl;
		argv++;
		argc--;
	}
	if (argv[1] && strcmp(argv[1], "--dumb") == 0) {
		consfn = dumb_consume;
		prodfn = dumb_produce;
		argv++;
		argc--;
	}

	if (argc != 4)
		errx(1, "Usage: qbench [--xprod][--xcons][--dumb] <num> <num-producers> <num-consumers>");

	num = atoi(argv[1]);
	prods = atoi(argv[2]);
	cons = atoi(argv[3]);

	/* Make it evenly divisible. */
	while ((num % prods) || (num % cons))
		num++;
	sprintf(numstr, "%lu", num);
	argv[1] = numstr;

	prod_num = num / prods;
	cons_num = num / cons;

	children = tal_arr(NULL, struct at_child *, prods + cons);

	pool = at_new_pool(sizeof(*q));
	q = tal(pool, struct queue);
	queue_init(q);

	for (i = 0; i < prods + cons; i++) {
		children[i] = at_run(pool, i < prods ? prodfn : consfn,
				     (void *)i);
		if (!at_read_child(children[i]))
			err(1, "Child %li failed", i);
	}

	for (i = 0; i < prods + cons; i++)
		at_tell_child(children[i], q);

	start = time_now();
	for (i = total = 0; i < prods + cons; i++) {
		void *ret = at_read_child(children[i]);
		if (!ret)
			err(1, "Child %li failed", i);
		total += (unsigned long)ret;
	}
	end = time_now();

	/* They should cancel out... */
	if (total != 0)
		errx(1, "Unbalanced total: %li\n", total);

	/* Kill them all. */
	tal_free(pool);
	while (*orig_args) {
		printf("%s ", *orig_args);
		orig_args++;
	}
	printf("in %llums\n", (long long)time_to_msec(time_sub(end, start)));
	return 0;
}

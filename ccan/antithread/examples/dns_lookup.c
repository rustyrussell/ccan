/* Async DNS lookup.  Shows passing complex data through pool. */
#include <ccan/antithread/antithread.h>
#include <ccan/str/str.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
#include <err.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

struct lookup_answer {
	bool ok;
	union {
		struct hostent hent;
		int herrno; /* If !ok */
	};
};

/* Including NULL terminator. */
static inline unsigned count_entries(char **entries)
{
	unsigned int i;

	for (i = 0; entries[i]; i++);
	return i+1;
}

/* Copy as one nice talated object.  Since ans is in the pool, it
 * all gets put in the pool. */
static void copy_answer(struct lookup_answer *ans, const struct hostent *host)
{
	unsigned int i;

	ans->hent.h_name = tal_strdup(ans, host->h_name);
	ans->hent.h_aliases = tal_arr(ans, char *,
				      count_entries(host->h_aliases));
	for (i = 0; host->h_aliases[i]; i++)
		ans->hent.h_aliases[i] = tal_strdup(ans->hent.h_aliases,
						       host->h_aliases[i]);
	ans->hent.h_aliases[i] = NULL;
	ans->hent.h_addrtype = host->h_addrtype;
	ans->hent.h_length = host->h_length;
	ans->hent.h_addr_list = tal_arr(ans, char *,
					count_entries(host->h_addr_list));
	for (i = 0; host->h_addr_list[i]; i++)
		ans->hent.h_addr_list[i] = tal_dup(ans->hent.h_addr_list,
						   char, host->h_addr_list[i],
						   ans->hent.h_length, 0);
	ans->hent.h_addr_list[i] = NULL;
}

static void *lookup_dns(struct at_parent *parent, char *name)
{
	struct lookup_answer *ans;
	struct hostent *host;

	host = gethostbyname(name);

	ans = tal(parent, struct lookup_answer);
	if (!host) {
		ans->ok = false;
		ans->herrno = h_errno;
	} else {
		ans->ok = true;
		copy_answer(ans, host);
	}

	return ans;
}

static void report_answer(const char *name, const struct lookup_answer *ans)
{
	unsigned int i;

	if (!ans->ok) {
		printf("%s: %s\n", name, hstrerror(ans->herrno));
		return;
	}

	printf("%s: ", name);
	for (i = 0; ans->hent.h_aliases[i]; i++)
		printf("%c%s", i == 0 ? '[' : ' ', ans->hent.h_aliases[i]);
	if (i)
		printf("]");
	printf("%#x", ans->hent.h_addrtype);
	for (i = 0; ans->hent.h_addr_list[i]; i++) {
		unsigned int j;
		printf(" ");
		for (j = 0; j < ans->hent.h_length; j++)
			printf("%02x", ans->hent.h_addr_list[i][j]);
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct at_child **at;
	unsigned int i;

	if (argc < 2)
		errx(1, "Usage: dns_lookup [--sync] name...");

	if (streq(argv[1], "--sync")) {
		for (i = 2; i < argc; i++) {
			struct lookup_answer *ans = lookup_dns(NULL, argv[i]);
			report_answer(argv[i], ans);
			tal_free(ans);
		}
		return 0;
	}			

	/* Give it plenty of room. */
	atp = at_new_pool(argc * 1024*1024);
	if (!atp)
		err(1, "Can't create pool");
	at = tal_arr(atp, struct at_child *, argc);

	for (i = 1; i < argc; i++) {
		at[i] = at_run(atp, lookup_dns, argv[i]);
		if (!at[i])
			err(1, "Can't spawn child");
	}

	for (i = 1; i < argc; i++) {
		struct lookup_answer *ans = at_read_child(at[i]);
		if (!ans)
			warn("Child died on '%s'", argv[i]);
		else
			report_answer(argv[i], ans);
	}

	tal_free(atp);
	return 0;
}		

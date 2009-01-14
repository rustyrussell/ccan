/* Async DNS lookup.  Shows passing complex data through pool. */
#include <ccan/antithread/antithread.h>
#include <ccan/str/str.h>
#include <ccan/talloc/talloc.h>
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

/* Copy as one nice tallocated object.  Since ans is in the pool, it
 * all gets put in the pool. */
static void copy_answer(struct lookup_answer *ans, const struct hostent *host)
{
	unsigned int i;

	ans->hent.h_name = talloc_strdup(ans, host->h_name);
	ans->hent.h_aliases = talloc_array(ans, char *,
					   count_entries(host->h_aliases));
	for (i = 0; host->h_aliases[i]; i++)
		ans->hent.h_aliases[i] = talloc_strdup(ans->hent.h_aliases,
						       host->h_aliases[i]);
	ans->hent.h_aliases[i] = NULL;
	ans->hent.h_addrtype = host->h_addrtype;
	ans->hent.h_length = host->h_length;
	ans->hent.h_addr_list = talloc_array(ans, char *,
					     count_entries(host->h_addr_list));
	for (i = 0; host->h_addr_list[i]; i++)
		ans->hent.h_addr_list[i] = talloc_memdup(ans->hent.h_addr_list,
							 host->h_addr_list[i],
							 ans->hent.h_length);
}

static void *lookup_dns(struct at_pool *atp, char *name)
{
	struct lookup_answer *ans;
	struct hostent *host;

	host = gethostbyname(name);

	ans = talloc(at_pool_ctx(atp), struct lookup_answer);
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
	struct athread **at;
	unsigned int i;

	if (argc < 2)
		errx(1, "Usage: dns_lookup [--sync] name...");

	/* Give it plenty of room. */
	atp = at_pool(argc * 1024*1024);
	if (!atp)
		err(1, "Can't create pool");

	/* Free pool on exit. */
	talloc_steal(talloc_autofree_context(), atp);

	if (streq(argv[1], "--sync")) {
		for (i = 2; i < argc; i++) {
			struct lookup_answer *ans = lookup_dns(atp, argv[i]);
			report_answer(argv[i], ans);
			talloc_free(ans);
		}
		return 0;
	}			

	at = talloc_array(atp, struct athread *, argc);

	for (i = 1; i < argc; i++) {
		at[i] = at_run(atp, lookup_dns, argv[i]);
		if (!at[i])
			err(1, "Can't spawn child");
	}

	for (i = 1; i < argc; i++) {
		struct lookup_answer *ans = at_read(at[i]);
		if (!ans)
			warn("Child died on '%s'", argv[i]);
		else {
			report_answer(argv[i], ans);
			talloc_free(ans);
		}
	}
	return 0;
}		

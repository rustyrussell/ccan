#include "config.h"

#include <stddef.h>
#include <assert.h>

#include <ccan/ptrint/ptrint.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
#include <ccan/take/take.h>
#include <ccan/tal/grab_file/grab_file.h>
#include <ccan/container_of/container_of.h>

#include <ccan/aga/aga.h>

#include <ccan/tap/tap.h>

#define NWORDS		1000
#define LETTERS		"abcdefghijklmnopqrstuvwxyz"
#define NLETTERS	(sizeof(LETTERS) - 1)

char **wordarray;
char **wordarrayb; /* Word array sorted by length */
int nwords;

struct trie_node {
	int start, end;
	const char *prefix;
	bool isword;
	struct trie_node *next[NLETTERS];
	struct aga_node agan;
};

/* Sorts by length first, then alphabetically */
static int lensort(const void *xv, const void *yv)
{
	char *x = *((char **)xv);
	char *y = *((char **)yv);
	int xn = strlen(x);
	int yn = strlen(y);

	if (xn < yn)
		return -1;
	else if (xn > yn)
		return 1;
	else
		/* We need this because qsort is not stable */
		return strcmp(x, y);
}

static void setup_words(void)
{
	char *wordfile;

	/* read in the words */
	wordfile = grab_file(NULL, "test/api-lazytrie-words.txt");
	ok1(wordfile);
	wordarray = tal_strsplit(NULL, take(wordfile), "\n", STR_NO_EMPTY);
	ok1(wordarray);
	nwords = tal_count(wordarray) - 1;
	diag("lazytrie: %d words loaded", nwords);
	ok1(nwords == NWORDS);

	wordarrayb = tal_arr(NULL, char *, nwords);
	memcpy(wordarrayb, wordarray, tal_count(wordarrayb) * sizeof(char *));

	qsort(wordarrayb, nwords, sizeof(char *), lensort);
}

struct lazytrie {
	struct trie_node root;
	struct aga_graph g;
};

static void init_trie_node(struct trie_node *n, int start, int end,
			   const char *prefix)
{
	int i;

	n->start = start;
	n->end = end;
	n->prefix = prefix;
	n->isword = (strcmp(n->prefix, wordarray[n->start]) == 0);

	for (i = 0; i < NLETTERS; i++)
		n->next[i] = NULL;

	aga_node_init(&n->agan);
}

static ptrint_t *lazytrie_first_edge(const struct aga_graph *g,
				     const struct aga_node *n)
{
	return int2ptr(1);
}

static ptrint_t *lazytrie_next_edge(const struct aga_graph *g,
				    const struct aga_node *n,
				    ptrint_t *e)
{
	int index = ptr2int(e);

	assert((index >= 1) && (index <= NLETTERS));
	if (index == NLETTERS)
		return NULL;
	else
		return int2ptr(index + 1);
}

static int lazytrie_edge_info(const struct aga_graph *g,
			      const struct aga_node *n,
			      ptrint_t *e,
			      struct aga_edge_info *ei)
{
	struct trie_node *tn = container_of(n, struct trie_node, agan);
	struct trie_node *next;
	int index = ptr2int(e);

	assert((index >= 1) && (index <= NLETTERS));

	next = tn->next[index - 1];

	if (!next) {
		int depth = strlen(tn->prefix);
		int start, end;

		start = tn->start;
		while (start < tn->end) {
			if (wordarray[start][depth] >= LETTERS[index - 1])
				break;
			start++;
		}

		end = start;
		while (end < tn->end) {
			if (wordarray[end][depth] > LETTERS[index - 1])
				break;
			end++;
		}

		if (end > start) {
			char plus[2] = { LETTERS[index - 1], '\0' };
			next = tal(tn, struct trie_node);
			init_trie_node(next, start, end,
				       tal_strcat(next, tn->prefix, plus));
		}
	}

	if (next)
		ei->to = &next->agan;
	return 0;
}

static struct lazytrie *setup_trie(void)
{
	struct lazytrie *lt;

	lt = tal(NULL, struct lazytrie);
	init_trie_node(&lt->root, 0, nwords, "");
	aga_init_graph(&lt->g, lazytrie_first_edge, lazytrie_next_edge,
		       lazytrie_edge_info);
	return lt;
}

int main(void)
{
	struct lazytrie *lt;
	struct aga_node *an;
	int xn;

	plan_tests(3 + NWORDS*2);
	setup_words();

	lt = setup_trie();

	aga_dfs_start(&lt->g);

	xn = 0;
	aga_dfs(an, &lt->g, &lt->root.agan) {
		struct trie_node *n = container_of(an, struct trie_node, agan);

		diag("Visited \"%s\"\n", n->prefix);

		if (n->isword) {
			ok1(strcmp(n->prefix, wordarray[xn]) == 0);
			xn++;
		}
	}

	aga_finish(&lt->g);

	tal_free(lt);

	lt = setup_trie();

	aga_bfs_start(&lt->g);

	xn = 0;
	aga_bfs(an, &lt->g, &lt->root.agan) {
		struct trie_node *n = container_of(an, struct trie_node, agan);

		diag("Visited \"%s\"\n", n->prefix);

		if (n->isword) {
			ok1(strcmp(n->prefix, wordarrayb[xn]) == 0);
			xn++;
		}
	}

	/* This exits depending on whether all tests passed */
	return exit_status();
}

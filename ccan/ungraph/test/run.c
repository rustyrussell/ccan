#include <ccan/ungraph/ungraph.h>
/* Include the C files directly. */
#include <ccan/ungraph/ungraph.c>
#include <ccan/tap/tap.h>
#include <assert.h>

static void *add_node(const tal_t *ctx,
		      const char *name,
		      const char **errstr,
		      char **out)
{
	tal_append_fmt(out, "add_node %s\n", (char *)name);
	return (void *)tal_steal(ctx, name);
}

static const char *add_edge(const tal_t *ctx,
			    void *source_node,
			    void *dest_node,
			    bool bidir,
			    const char **labels,
			    char **out)
{
	tal_append_fmt(out, "add_edge %s-%s bidir=%i\n",
		       (char *)source_node,
		       (char *)dest_node,
		       bidir);
	for (size_t i = 0; i < tal_count(labels); i++)
		tal_append_fmt(out, "- label %s\n", labels[i]);
	return NULL;
}

int main(void)
{
	const tal_t *ctx = tal(NULL, char);
	char *out = tal_arrz(ctx, char, 1);
	/* This is how many tests you plan to run */
	plan_tests(16);

	ok1(ungraph(ctx,
		    "AAA----->BBB",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_edge AAA-BBB bidir=0\n") == 0);
	
	out = tal_arrz(ctx, char, 1);
	ok1(ungraph(ctx,
		    "AAA<------BBB",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_edge BBB-AAA bidir=0\n") == 0);
	
	out = tal_arrz(ctx, char, 1);
	ok1(ungraph(ctx,
		    "AAA------BBB",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_edge AAA-BBB bidir=1\n") == 0);

	out = tal_arrz(ctx, char, 1);
	ok1(ungraph(ctx,
		    "AAA<------>BBB",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_edge AAA-BBB bidir=1\n") == 0);

	out = tal_arrz(ctx, char, 1);
	ok1(ungraph(ctx,
		    "AAA\n"
		    " ^ \n"
		    " | \n"
		    " | \n"
		    " v \n"
		    "BBB",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_edge AAA-BBB bidir=1\n") == 0);
	
	out = tal_arrz(ctx, char, 1);
	ok1(ungraph(ctx,
		    "AAA\n"
		    " / \n"
		    "| \n"
		    " \\ \n"
		    "  v \n"
		    "  BBB\n",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_edge AAA-BBB bidir=0\n") == 0);
	
	out = tal_arrz(ctx, char, 1);
	ok1(ungraph(ctx,
		    "AAA\n"
		    " / \n"
		    "|xyx \n"
		    " \\ \n"
		    "  v \n"
		    "  BBB",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_edge AAA-BBB bidir=0\n"
		   "- label xyx\n") == 0);

	out = tal_arrz(ctx, char, 1);
	ok1(ungraph(ctx,
		    " AAA \n"
		    "  |   \n"
		    "A-+----B \n"
		    "  | LABEL  \n"
		    "  |  xyz\n"
		    "  v  \n"
		    " BBB",
		    add_node, add_edge, &out) == NULL);
	ok1(strcmp(out,
		   "add_node AAA\n"
		   "add_node BBB\n"
		   "add_node A\n"
		   "add_node B\n"
		   "add_edge AAA-BBB bidir=0\n"
		   "add_edge A-B bidir=1\n"
		   "- label LABEL\n"
		   "- label xyz\n") == 0);

	tal_free(ctx);
	/* This exits depending on whether all tests passed */
	return exit_status();
}

#ifndef CCAN_OPT_PRIVATE_H
#define CCAN_OPT_PRIVATE_H

extern struct opt_table *opt_table;
extern unsigned int opt_count;

extern const char *opt_argv0;

#define subtable_of(entry) ((struct opt_table *)((entry)->longopt))

#endif /* CCAN_OPT_PRIVATE_H */

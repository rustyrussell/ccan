#ifndef CCANLINT_LICENSES_H
#define CCANLINT_LICENSES_H
#include <stdbool.h>
#include <ccan/tal/tal.h>

enum license {
	LICENSE_LGPLv2_PLUS,
	LICENSE_LGPLv2,
	LICENSE_LGPLv3,
	LICENSE_LGPL,
	LICENSE_GPLv2_PLUS,
	LICENSE_GPLv2,
	LICENSE_GPLv3,
	LICENSE_GPL,
	LICENSE_BSD,
	LICENSE_MIT,
	LICENSE_CC0,
	LICENSE_PUBLIC_DOMAIN,
	LICENSE_UNKNOWN
};

#define NUM_CLAUSES 3

struct license_info {
	const char *name;
	const char *shortname;
	const char *describe;
	/* Edit distance is expensive, and this works quite well. */
	const char *clause[NUM_CLAUSES];
};

/* Is [project license][file license] compatible? */
bool license_compatible[LICENSE_UNKNOWN+1][LICENSE_UNKNOWN];

extern const struct license_info licenses[];

struct ccan_file;
bool find_boilerplate(struct ccan_file *f, enum license license);

struct doc_section;
enum license which_license(struct doc_section *d);

struct manifest;
struct doc_section *find_license_tag(const struct manifest *m);

const char *get_license_oneliner(const tal_t *ctx, enum license license);

#endif /* CCANLINT_LICENSES_H */

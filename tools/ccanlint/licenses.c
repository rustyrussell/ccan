#include "licenses.h"
#include "ccanlint.h"
#include <ccan/str/str.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>

const struct license_info licenses[] = {
	{ "LGPLv2+", "LGPL",
	  "GNU LGPL version 2 (or later)",
	  { "gnu lesser general public license",
	    "version 2",
	    "or at your option any later version"
	  }
	},
	{ "LGPLv2", "LGPL",
	  "GNU LGPL version 2",
	  { "gnu lesser general public license",
	    "version 2",
	    NULL
	  }
	},
	{ "LGPLv3", "LGPL",
	  "GNU LGPL version 3",
	  { "gnu lesser general public license",
	    "version 3",
	    NULL
	  }
	},
	{ "LGPL", "LGPL",
	  "GNU LGPL",
	  { "gnu lesser general public license",
	    NULL,
	    NULL
	  }
	},
	{ "GPLv2+", "GPL",
	  "GNU GPL version 2 (or later)",
	  { "gnu general public license",
	    "version 2",
	    "or at your option any later version"
	  }
	},
	{ "GPLv2", "GPL",
	  "GNU GPL version 2",
	  { "gnu general public license",
	    "version 2",
	    NULL
	  }
	},
	{ "GPLv3", "GPL",
	  "GNU GPL version 3 (or later)",
	  { "gnu general public license",
	    "version 3",
	    NULL
	  }
	},
	{ "GPL", "GPL",
	  "GNU GPL",
	  { "gnu general public license",
	    NULL,
	    NULL
	  }
	},
	{ "BSD-3CLAUSE", "BSD",
	  "3-clause BSD license",
	  { "redistributions of source code must retain",
	    "redistributions in binary form must reproduce",
	    "endorse or promote"
	  }
	},
	{ "BSD-MIT", "MIT",
	  "MIT (BSD) license",
	  { "without restriction",
	    "above copyright notice",
	    "without warranty"
	  }
	},
	{ "CC0", "CC0",
	  "CC0 license (public domain)",
	  { "Waiver.",
	    "unconditionally waives",
	    NULL
	  }
	},
	{ "Public domain", "Public domain",
	  NULL,
	  { NULL, NULL, NULL  }
	},
	{ "Unknown license", "Unknown license",
	  NULL,
	  { NULL, NULL, NULL  }
	},
};

/* License compatibilty chart (simplified: we don't test that licenses between
 * files are compatible). */
#define O true
#define X false
bool license_compatible[LICENSE_UNKNOWN+1][LICENSE_UNKNOWN] = {
/*     LGPL2+   LGPL3   GPL2+   GPL3     BSD     CC0
            LGPL2    LGPL   GPL2     GPL     MIT     PD   */
/* _info says: LGPL2+ */
	{ O,  X,  X,  O,  X,  X,  X,  X,  O,  O,  O,  O },
/* _info says: LGPL2 only */
	{ O,  O,  X,  O,  X,  X,  X,  X,  O,  O,  O,  O },
/* _info says: LGPL3 (or any later version) */
	{ O,  X,  O,  O,  X,  X,  X,  X,  O,  O,  O,  O },
/* _info says: LGPL (no version specified) */
	{ O,  O,  O,  O,  X,  X,  X,  X,  O,  O,  O,  O },
/* _info says: GPL2+ */
	{ O,  O,  O,  O,  O,  X,  X,  O,  O,  O,  O,  O },
/* _info says: GPL2 only */
	{ O,  O,  O,  O,  O,  O,  X,  O,  O,  O,  O,  O },
/* _info says: GPL3 (or any later version) */
	{ O,  O,  O,  O,  O,  X,  O,  O,  O,  O,  O,  O },
/* _info says: GPL (unknown version) */
	{ O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O,  O },
/* _info says: BSD (3-clause) */
	{ X,  X,  X,  X,  X,  X,  X,  X,  O,  O,  O,  O },
/* _info says: MIT */
	{ X,  X,  X,  X,  X,  X,  X,  X,  X,  O,  O,  O },
/* _info says: CC0 */
	{ X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  O,  O },
/* _info says: Public domain */
	{ X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  O,  O },
/* _info says something we don't understand */
	{ X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  O,  O }
};
#undef X
#undef O

/* See GPLv2 and v2 (basically same wording) for interpreting versions:
 * the "any later version" means the recepient can choose. */
enum license which_license(struct doc_section *d)
{
	if (!d)
		return LICENSE_UNKNOWN;

	/* This means "user chooses what version", including GPLv1! */
	if (streq(d->lines[0], "GPL"))
		return LICENSE_GPL;
	/* This means "v2 only". */
	if (streq(d->lines[0], "GPLv2"))
		return LICENSE_GPLv2;
	/* This means "v2 or above" at user's choice. */
	if (streq(d->lines[0], "GPL (v2 or any later version)"))
		return LICENSE_GPLv2_PLUS;
	/* This means "v3 or above" at user's choice. */
	if (streq(d->lines[0], "GPL (v3 or any later version)"))
		return LICENSE_GPLv3;

	/* This means "user chooses what version" */
	if (streq(d->lines[0], "LGPL"))
		return LICENSE_LGPL;
	/* This means "v2.1 only". */
	if (streq(d->lines[0], "LGPLv2.1"))
		return LICENSE_LGPLv2;
	/* This means "v2.1 or above" at user's choice. */
	if (streq(d->lines[0], "LGPL (v2.1 or any later version)"))
		return LICENSE_LGPLv2_PLUS;
	/* This means "v3 or above" at user's choice. */
	if (streq(d->lines[0], "LGPL (v3 or any later version)"))
		return LICENSE_LGPLv3;

	if (streq(d->lines[0], "BSD-MIT") || streq(d->lines[0], "MIT"))
		return LICENSE_MIT;
	if (streq(d->lines[0], "BSD (3 clause)"))
		return LICENSE_BSD;
	if (streq(d->lines[0], "CC0"))
		return LICENSE_CC0;
	if (tal_strreg(NULL, d->lines[0], "CC0 \\([Pp]ublic [Dd]omain\\)",
		       NULL))
		return LICENSE_CC0;
	if (tal_strreg(NULL, d->lines[0], "[Pp]ublic [Dd]omain"))
		return LICENSE_PUBLIC_DOMAIN;

	return LICENSE_UNKNOWN;
}

const char *get_ccan_simplified(struct ccan_file *f)
{
	if (!f->simplified) {
		unsigned int i, j;

		/* Simplify for easy matching: only alnum and single spaces. */
		f->simplified = tal_strdup(f, get_ccan_file_contents(f));
		for (i = 0, j = 0; f->simplified[i]; i++) {
			if (cisupper(f->simplified[i]))
				f->simplified[j++] = tolower(f->simplified[i]);
			else if (cislower(f->simplified[i]))
				f->simplified[j++] = f->simplified[i];
			else if (cisdigit(f->simplified[i]))
				f->simplified[j++] = f->simplified[i];
			else if (cisspace(f->simplified[i])) {
				if (j != 0 && f->simplified[j-1] != ' ')
					f->simplified[j++] = ' ';
			}
		}
		f->simplified[j] = '\0';
	}
	return f->simplified;
}

bool find_boilerplate(struct ccan_file *f, enum license license)
{
	unsigned int i;

	for (i = 0; i < NUM_CLAUSES; i++) {
		if (!licenses[license].clause[i])
			break;

		if (!strstr(get_ccan_simplified(f),
			    licenses[license].clause[i])) {
			return false;
		}
	}
	return true;
}

struct doc_section *find_license_tag(const struct manifest *m)
{
	struct doc_section *d;

	list_for_each(get_ccan_file_docs(m->info_file), d, list) {
		if (!streq(d->function, m->modname))
			continue;
		if (streq(d->type, "license"))
			return d;
	}
	return NULL;
}

const char *get_license_oneliner(const tal_t *ctx, enum license license)
{
	return tal_fmt(ctx, "/* %s - see LICENSE file for details */",
		       licenses[license].describe);
}

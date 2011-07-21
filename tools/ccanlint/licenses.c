#include "licenses.h"
#include "ccanlint.h"
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>

const struct license_info licenses[] = {
	{ "LGPLv2+", "LGPL",
	  { "gnu lesser general public license",
	    "version 2",
	    "or at your option any later version"
	  }
	},
	{ "LGPLv2", "LGPL",
	  { "gnu lesser general public license",
	    "version 2",
	    NULL
	  }
	},
	{ "LGPLv3", "LGPL",
	  { "gnu lesser general public license",
	    "version 3",
	    NULL
	  }
	},
	{ "LGPL", "LGPL",
	  { "gnu lesser general public license",
	    NULL,
	    NULL
	  }
	},
	{ "GPLv2+", "GPL",
	  { "gnu general public license",
	    "version 2",
	    "or at your option any later version"
	  }
	},
	{ "GPLv2", "GPL",
	  { "gnu general public license",
	    "version 2",
	    NULL
	  }
	},
	{ "GPLv3", "GPL",
	  { "gnu general public license",
	    "version 3",
	    NULL
	  }
	},
	{ "GPL", "GPL",
	  { "gnu general public license",
	    NULL,
	    NULL
	  }
	},
	{ "BSD-3CLAUSE", "BSD",
	  { "redistributions of source code must retain",
	    "redistributions in binary form must reproduce",
	    "endorse or promote"
	  }
	},
	{ "BSD-MIT", "MIT",
	  { "without restriction",
	    "above copyright notice",
	    "without warranty"
	  }
	},
	{ "Public domain", "Public domain",
	  { NULL, NULL, NULL  }
	},
	{ "Unknown license", "Unknown license",
	  { NULL, NULL, NULL  }
	},
};

const char *get_ccan_simplified(struct ccan_file *f)
{
	if (!f->simplified) {
		unsigned int i, j;

		/* Simplify for easy matching: only alnum and single spaces. */
		f->simplified = talloc_strdup(f, get_ccan_file_contents(f));
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

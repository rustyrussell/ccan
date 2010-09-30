#include <ccan/opt/opt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "private.h"

/* We only use this for pointer comparisons. */
const char opt_table_hidden[1];

static unsigned write_short_options(char *str)
{
	unsigned int i, num = 0;

	for (i = 0; i < opt_count; i++) {
		if (opt_table[i].flags == OPT_SUBTABLE) {
			if (opt_table[i].desc == opt_table_hidden) {
				/* Skip these options. */
				i += (intptr_t)opt_table[i].arg - 1;
				continue;
			}
		} else if (opt_table[i].shortopt)
			str[num++] = opt_table[i].shortopt;
	}
	return num;
}

#define OPT_SPACE_PAD "                    "

/* FIXME: Get all purdy. */
char *opt_usage(const char *argv0, const char *extra)
{
	unsigned int i, num, len;
	char *ret, *p;

	/* An overestimate of our length. */
	len = strlen("Usage: %s ") + strlen(argv0)
		+ strlen("[-%.*s]") + opt_count + 1
		+ strlen(" ") + strlen(extra)
		+ strlen("\n");

	for (i = 0; i < opt_count; i++) {
		if (opt_table[i].flags == OPT_SUBTABLE) {
			len += strlen("\n") + strlen(opt_table[i].desc)
				+ strlen(":\n");
		} else {
			len += strlen("--%s/-%c") + strlen(" <arg>");
			if (opt_table[i].longopt)
				len += strlen(opt_table[i].longopt);
			if (opt_table[i].desc) {
				len += strlen(OPT_SPACE_PAD)
					+ strlen(opt_table[i].desc) + 1;
			}
			if (opt_table[i].show) {
				len += strlen("(default: %s)")
					+ OPT_SHOW_LEN + sizeof("...");
			}
			len += strlen("\n");
		}
	}

	p = ret = malloc(len);
	if (!ret)
		return NULL;

	p += sprintf(p, "Usage: %s", argv0);
	p += sprintf(p, " [-");
	num = write_short_options(p);
	if (num) {
		p += num;
		p += sprintf(p, "]");
	} else {
		/* Remove start of single-entry options */
		p -= 3;
	}
	if (extra)
		p += sprintf(p, " %s", extra);
	p += sprintf(p, "\n");

	for (i = 0; i < opt_count; i++) {
		if (opt_table[i].flags == OPT_SUBTABLE) {
			if (opt_table[i].desc == opt_table_hidden) {
				/* Skip these options. */
				i += (intptr_t)opt_table[i].arg - 1;
				continue;
			}
			p += sprintf(p, "%s:\n", opt_table[i].desc);
			continue;
		}
		if (opt_table[i].shortopt && opt_table[i].longopt)
			len = sprintf(p, "--%s/-%c",
				     opt_table[i].longopt,
				      opt_table[i].shortopt);
		else if (opt_table[i].shortopt)
			len = sprintf(p, "-%c", opt_table[i].shortopt);
		else
			len = sprintf(p, "--%s", opt_table[i].longopt);
		if (opt_table[i].flags == OPT_HASARG)
			len += sprintf(p + len, " <arg>");
		if (opt_table[i].desc || opt_table[i].show)
			len += sprintf(p + len, "%.*s",
				       len < strlen(OPT_SPACE_PAD)
				       ? strlen(OPT_SPACE_PAD) - len : 1,
				       OPT_SPACE_PAD);

		if (opt_table[i].desc)
			len += sprintf(p + len, "%s", opt_table[i].desc);
		if (opt_table[i].show) {
			char buf[OPT_SHOW_LEN + sizeof("...")];
			strcpy(buf + OPT_SHOW_LEN, "...");
			opt_table[i].show(buf, opt_table[i].arg);
			len += sprintf(p + len, "%s(default: %s)",
				       opt_table[i].desc ? " " : "", buf);
		}
		p += len;
		p += sprintf(p, "\n");
	}
	*p = '\0';
	return ret;
}

/* Licensed under GPLv3+ - see LICENSE file for details */
#include <ccan/opt/opt.h>
#include <sys/ioctl.h>
#include <sys/termios.h> /* Required on Solaris for struct winsize */
#include <sys/unistd.h> /* Required on Solaris for ioctl */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "private.h"

/* We only use this for pointer comparisons. */
const char opt_hidden[1];

#define MIN_DESC_WIDTH 40
#define MIN_TOTAL_WIDTH 50

static unsigned int get_columns(void)
{
	struct winsize w;
	const char *env = getenv("COLUMNS");

	w.ws_col = 0;
	if (env)
		w.ws_col = atoi(env);
	if (!w.ws_col)
		if (ioctl(0, TIOCGWINSZ, &w) == -1)
			w.ws_col = 0;
	if (!w.ws_col)
		w.ws_col = 80;

	return w.ws_col;
}

/* Return number of chars of words to put on this line.
 * Prefix is set to number to skip at start, maxlen is max width, returns
 * length (after prefix) to put on this line. */
static size_t consume_words(const char *words, size_t maxlen, size_t *prefix)
{
	size_t oldlen, len;

	/* Swallow leading whitespace. */
	*prefix = strspn(words, " ");
	words += *prefix;

	/* Use at least one word, even if it takes us over maxlen. */
	oldlen = len = strcspn(words, " ");
	while (len <= maxlen) {
		oldlen = len;
		len += strspn(words+len, " ");
		len += strcspn(words+len, " ");
		if (len == oldlen)
			break;
	}

	return oldlen;
}

static char *add_str_len(char *base, size_t *len, size_t *max,
			 const char *str, size_t slen)
{
	if (slen >= *max - *len)
		base = realloc(base, *max = (*max * 2 + slen + 1));
	memcpy(base + *len, str, slen);
	*len += slen;
	return base;
}

static char *add_str(char *base, size_t *len, size_t *max, const char *str)
{
	return add_str_len(base, len, max, str, strlen(str));
}

static char *add_indent(char *base, size_t *len, size_t *max, size_t indent)
{
	if (indent >= *max - *len)
		base = realloc(base, *max = (*max * 2 + indent + 1));
	memset(base + *len, ' ', indent);
	*len += indent;
	return base;
}

static char *add_desc(char *base, size_t *len, size_t *max,
		      unsigned int indent, unsigned int width,
		      const struct opt_table *opt)
{
	size_t off, prefix, l;
	const char *p;
	bool same_line = false;

	base = add_str(base, len, max, opt->names);
	off = strlen(opt->names);
	if (opt->type == OPT_HASARG
	    && !strchr(opt->names, ' ')
	    && !strchr(opt->names, '=')) {
		base = add_str(base, len, max, " <arg>");
		off += strlen(" <arg>");
	}

	/* Do we start description on next line? */
	if (off + 2 > indent) {
		base = add_str(base, len, max, "\n");
		off = 0;
	} else {
		base = add_indent(base, len, max, indent - off);
		off = indent;
		same_line = true;
	}

	/* Indent description. */
	p = opt->desc;
	while ((l = consume_words(p, width - indent, &prefix)) != 0) {
		if (!same_line)
			base = add_indent(base, len, max, indent);
		p += prefix;
		base = add_str_len(base, len, max, p, l);
		base = add_str(base, len, max, "\n");
		off = indent + l;
		p += l;
		same_line = false;
	}

	/* Empty description?  Make it match normal case. */
	if (same_line)
		base = add_str(base, len, max, "\n");

	if (opt->show) {
		char buf[OPT_SHOW_LEN + sizeof("...")];
		strcpy(buf + OPT_SHOW_LEN, "...");
		opt->show(buf, opt->u.arg);

		/* If it doesn't fit on this line, indent. */
		if (off + strlen(" (default: ") + strlen(buf) + strlen(")")
		    > width) {
			base = add_indent(base, len, max, indent);
		} else {
			/* Remove \n. */
			(*len)--;
		}

		base = add_str(base, len, max, " (default: ");
		base = add_str(base, len, max, buf);
		base = add_str(base, len, max, ")\n");
	}
	return base;
}

char *opt_usage(const char *argv0, const char *extra)
{
	unsigned int i;
	size_t max, len, width, indent;
	char *ret;

	width = get_columns();
	if (width < MIN_TOTAL_WIDTH)
		width = MIN_TOTAL_WIDTH;

	/* Figure out longest option. */
	indent = 0;
	for (i = 0; i < opt_count; i++) {
		size_t l;
		if (opt_table[i].desc == opt_hidden)
			continue;
		if (opt_table[i].type == OPT_SUBTABLE)
			continue;
		l = strlen(opt_table[i].names);
		if (opt_table[i].type == OPT_HASARG
		    && !strchr(opt_table[i].names, ' ')
		    && !strchr(opt_table[i].names, '='))
			l += strlen(" <arg>");
		if (l + 2 > indent)
			indent = l + 2;
	}

	/* Now we know how much to indent */
	if (indent + MIN_DESC_WIDTH > width)
		indent = width - MIN_DESC_WIDTH;

	len = max = 0;
	ret = NULL;

	ret = add_str(ret, &len, &max, "Usage: ");
	ret = add_str(ret, &len, &max, argv0);

	/* Find usage message from among registered options if necessary. */
	if (!extra) {
		extra = "";
		for (i = 0; i < opt_count; i++) {
			if (opt_table[i].cb == (void *)opt_usage_and_exit
			    && opt_table[i].u.carg) {
				extra = opt_table[i].u.carg;
				break;
			}
		}
	}
	ret = add_str(ret, &len, &max, " ");
	ret = add_str(ret, &len, &max, extra);
	ret = add_str(ret, &len, &max, "\n");

	for (i = 0; i < opt_count; i++) {
		if (opt_table[i].desc == opt_hidden)
			continue;
		if (opt_table[i].type == OPT_SUBTABLE) {
			ret = add_str(ret, &len, &max, opt_table[i].desc);
			ret = add_str(ret, &len, &max, ":\n");
			continue;
		}
		ret = add_desc(ret, &len, &max, indent, width, &opt_table[i]);
	}
	ret[len] = '\0';
	return ret;
}

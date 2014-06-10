#include <ccan/err/err.h>
#include <ccan/str/str.h>
#include <ccan/tal/path/path.h>
#include <ccan/tal/grab_file/grab_file.h>
#include "read_config_header.h"
#include "tools.h"
#include <string.h>

/* Get an identifier token. */
char *get_symbol_token(void *ctx, const char **line)
{
	unsigned int toklen;
	char *ret;

	*line += strspn(*line, " \t");
	toklen = strspn(*line, IDENT_CHARS);
	if (!toklen)
		return NULL;
	ret = tal_strndup(ctx, *line, toklen);
	*line += toklen;
	return ret;
}

/* Get token if it's equal to token. */
bool get_token(const char **line, const char *token)
{
	unsigned int toklen;

	*line += strspn(*line, " \t");
	if (cisalnum(token[0]) || token[0] == '_')
		toklen = strspn(*line, IDENT_CHARS);
	else {
		/* FIXME: real tokenizer handles ++ and other multi-chars.  */
		toklen = strlen(token);
	}

	if (toklen == strlen(token) && !strncmp(*line, token, toklen)) {
		*line += toklen;
		return true;
	}
	return false;
}

static char *demangle_string(char *string)
{
	unsigned int i;
	const char mapfrom[] = "abfnrtv";
	const char mapto[] = "\a\b\f\n\r\t\v";

	if (!strchr(string, '"'))
		return NULL;
	string = strchr(string, '"') + 1;
	if (!strrchr(string, '"'))
		return NULL;
	*strrchr(string, '"') = '\0';

	for (i = 0; i < strlen(string); i++) {
		if (string[i] == '\\') {
			char repl;
			unsigned len = 0;
			const char *p = strchr(mapfrom, string[i+1]);
			if (p) {
				repl = mapto[p - mapfrom];
				len = 1;
			} else if (strlen(string+i+1) >= 3) {
				if (string[i+1] == 'x') {
					repl = (string[i+2]-'0')*16
						+ string[i+3]-'0';
					len = 3;
				} else if (cisdigit(string[i+1])) {
					repl = (string[i+2]-'0')*8*8
						+ (string[i+3]-'0')*8
						+ (string[i+4]-'0');
					len = 3;
				}
			}
			if (len == 0) {
				repl = string[i+1];
				len = 1;
			}

			string[i] = repl;
			memmove(string + i + 1, string + i + len + 1,
				strlen(string + i + len + 1) + 1);
		}
	}

	return string;
}

char *read_config_header(const char *ccan_dir, bool verbose)
{
	char *fname = path_join(NULL, ccan_dir, "config.h");
	char **lines;
	unsigned int i;
	char *config_header;

	config_header = grab_file(NULL, fname);
	tal_free(fname);

	if (!config_header)
		return NULL;

	lines = tal_strsplit(config_header, config_header, "\n", STR_EMPTY_OK);
	for (i = 0; i < tal_count(lines) - 1; i++) {
		char *sym;
		const char **line = (const char **)&lines[i];

		if (!get_token(line, "#"))
			continue;
		if (!get_token(line, "define"))
			continue;
		sym = get_symbol_token(lines, line);
		if (streq(sym, "CCAN_COMPILER")) {
			compiler = demangle_string(lines[i]);
			if (!compiler)
				errx(1, "%s:%u:could not parse CCAN_COMPILER",
				     fname, i+1);
			if (verbose)
				printf("%s: compiler set to '%s'\n",
				       fname, compiler);
		} else if (streq(sym, "CCAN_CFLAGS")) {
			cflags = demangle_string(lines[i]);
			if (!cflags)
				errx(1, "%s:%u:could not parse CCAN_CFLAGS",
				     fname, i+1);
			if (verbose)
				printf("%s: compiler flags set to '%s'\n",
				       fname, cflags);
		}
	}
	return config_header;
}

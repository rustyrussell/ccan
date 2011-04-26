/* Copyright (c) 2000-2007 by Nicolas Devillard.
 * Copyright (x) 2009 by Tim Post <tinkertim@gmail.com>
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/** @addtogroup ciniparser
 * @{
 */
/**
 *  @file ciniparser.c
 *  @author N. Devillard
 *  @date Sep 2007
 *  @version 3.0
 *  @brief Parser for ini files.
 */

#include <ctype.h>
#include <ccan/ciniparser/ciniparser.h>

#define ASCIILINESZ      (1024)
#define INI_INVALID_KEY  ((char*) NULL)

/**
 * This enum stores the status for each parsed line (internal use only).
 */
typedef enum _line_status_ {
	LINE_UNPROCESSED,
	LINE_ERROR,
	LINE_EMPTY,
	LINE_COMMENT,
	LINE_SECTION,
	LINE_VALUE
} line_status;


/**
 * @brief Convert a string to lowercase.
 * @param s String to convert.
 * @return ptr to statically allocated string.
 *
 * This function returns a pointer to a statically allocated string
 * containing a lowercased version of the input string. Do not free
 * or modify the returned string! Since the returned string is statically
 * allocated, it will be modified at each function call (not re-entrant).
 */
static char *strlwc(const char *s)
{
	static char l[ASCIILINESZ+1];
	int i;

	if (s == NULL)
		return NULL;

	for (i = 0; s[i] && i < ASCIILINESZ; i++)
		l[i] = tolower(s[i]);
	l[i] = '\0';
	return l;
}

/**
 * @brief Remove blanks at the beginning and the end of a string.
 * @param s String to parse.
 * @return ptr to statically allocated string.
 *
 * This function returns a pointer to a statically allocated string,
 * which is identical to the input string, except that all blank
 * characters at the end and the beg. of the string have been removed.
 * Do not free or modify the returned string! Since the returned string
 * is statically allocated, it will be modified at each function call
 * (not re-entrant).
 */
static char *strstrip(const char *s)
{
	static char l[ASCIILINESZ+1];
	unsigned int i, numspc;

	if (s == NULL)
		return NULL;

	while (isspace(*s))
		s++;

	for (i = numspc = 0; s[i] && i < ASCIILINESZ; i++) {
		l[i] = s[i];
		if (isspace(l[i]))
			numspc++;
		else
			numspc = 0;
	}
	l[i - numspc] = '\0';
	return l;
}

/**
 * @brief Load a single line from an INI file
 * @param input_line Input line, may be concatenated multi-line input
 * @param section Output space to store section
 * @param key Output space to store key
 * @param value Output space to store value
 * @return line_status value
 */
static
line_status ciniparser_line(char *input_line, char *section,
	char *key, char *value)
{
	line_status sta;
	char line[ASCIILINESZ+1];
	int	 len;

	strcpy(line, strstrip(input_line));
	len = (int) strlen(line);

	if (len < 1) {
		/* Empty line */
		sta = LINE_EMPTY;
	} else if (line[0] == '#') {
		/* Comment line */
		sta = LINE_COMMENT;
	} else if (line[0] == '[' && line[len-1] == ']') {
		/* Section name */
		sscanf(line, "[%[^]]", section);
		strcpy(section, strstrip(section));
		strcpy(section, strlwc(section));
		sta = LINE_SECTION;
	} else if (sscanf (line, "%[^=] = \"%[^\"]\"", key, value) == 2
		   ||  sscanf (line, "%[^=] = '%[^\']'", key, value) == 2
		   ||  sscanf (line, "%[^=] = %[^;#]", key, value) == 2) {
		/* Usual key=value, with or without comments */
		strcpy(key, strstrip(key));
		strcpy(key, strlwc(key));
		strcpy(value, strstrip(value));
		/*
		 * sscanf cannot handle '' or "" as empty values
		 * this is done here
		 */
		if (!strcmp(value, "\"\"") || (!strcmp(value, "''"))) {
			value[0] = 0;
		}
		sta = LINE_VALUE;
	} else if (sscanf(line, "%[^=] = %[;#]", key, value) == 2
		||  sscanf(line, "%[^=] %[=]", key, value) == 2) {
		/*
		 * Special cases:
		 * key=
		 * key=;
		 * key=#
		 */
		strcpy(key, strstrip(key));
		strcpy(key, strlwc(key));
		value[0] = 0;
		sta = LINE_VALUE;
	} else {
		/* Generate syntax error */
		sta = LINE_ERROR;
	}
	return sta;
}

/* The remaining public functions are documented in ciniparser.h */

int ciniparser_getnsec(dictionary *d)
{
	int i;
	int nsec;

	if (d == NULL)
		return -1;

	nsec = 0;
	for (i = 0; i < d->size; i++) {
		if (d->key[i] == NULL)
			continue;
		if (strchr(d->key[i], ':') == NULL) {
			nsec ++;
		}
	}

	return nsec;
}

char *ciniparser_getsecname(dictionary *d, int n)
{
	int i;
	int foundsec;

	if (d == NULL || n < 0)
		return NULL;

	if (n == 0)
		n ++;

	foundsec = 0;

	for (i = 0; i < d->size; i++) {
		if (d->key[i] == NULL)
			continue;
		if (! strchr(d->key[i], ':')) {
			foundsec++;
			if (foundsec >= n)
				break;
		}
	}

	if (foundsec == n) {
		return d->key[i];
	}

	return (char *) NULL;
}

void ciniparser_dump(dictionary *d, FILE *f)
{
	int i;

	if (d == NULL || f == NULL)
		return;

	for (i = 0; i < d->size; i++) {
		if (d->key[i] == NULL)
			continue;
		if (d->val[i] != NULL) {
			fprintf(f, "[%s]=[%s]\n", d->key[i], d->val[i]);
		} else {
			fprintf(f, "[%s]=UNDEF\n", d->key[i]);
		}
	}

	return;
}

void ciniparser_dump_ini(dictionary *d, FILE *f)
{
	int i, j;
	char keym[ASCIILINESZ+1];
	int nsec;
	char *secname;
	int seclen;

	if (d == NULL || f == NULL)
		return;

	memset(keym, 0, ASCIILINESZ + 1);

	nsec = ciniparser_getnsec(d);
	if (nsec < 1) {
		/* No section in file: dump all keys as they are */
		for (i = 0; i < d->size; i++) {
			if (d->key[i] == NULL)
				continue;
			fprintf(f, "%s = %s\n", d->key[i], d->val[i]);
		}
		return;
	}

	for (i = 0; i < nsec; i++) {
		secname = ciniparser_getsecname(d, i);
		seclen  = (int)strlen(secname);
		fprintf(f, "\n[%s]\n", secname);
		snprintf(keym, ASCIILINESZ + 1, "%s:", secname);
		for (j = 0; j < d->size; j++) {
			if (d->key[j] == NULL)
				continue;
			if (!strncmp(d->key[j], keym, seclen+1)) {
				fprintf(f, "%-30s = %s\n",
					d->key[j]+seclen+1,
					d->val[j] ? d->val[j] : "");
			}
		}
	}
	fprintf(f, "\n");

	return;
}

char *ciniparser_getstring(dictionary *d, const char *key, char *def)
{
	char *lc_key;
	char *sval;

	if (d == NULL || key == NULL)
		return def;

	lc_key = strlwc(key);
	sval = dictionary_get(d, lc_key, def);

	return sval;
}

int ciniparser_getint(dictionary *d, const char *key, int notfound)
{
	char *str;

	str = ciniparser_getstring(d, key, INI_INVALID_KEY);

	if (str == INI_INVALID_KEY)
		return notfound;

	return (int) strtol(str, NULL, 10);
}

double ciniparser_getdouble(dictionary *d, char *key, double notfound)
{
	char *str;

	str = ciniparser_getstring(d, key, INI_INVALID_KEY);

	if (str == INI_INVALID_KEY)
		return notfound;

	return atof(str);
}

int ciniparser_getboolean(dictionary *d, const char *key, int notfound)
{
	char *c;
	int ret;

	c = ciniparser_getstring(d, key, INI_INVALID_KEY);
	if (c == INI_INVALID_KEY)
		return notfound;

	switch(c[0]) {
	case 'y': case 'Y': case '1': case 't': case 'T':
		ret = 1;
		break;
	case 'n': case 'N': case '0': case 'f': case 'F':
		ret = 0;
		break;
	default:
		ret = notfound;
		break;
	}

	return ret;
}

int ciniparser_find_entry(dictionary *ini, char *entry)
{
	int found = 0;

	if (ciniparser_getstring(ini, entry, INI_INVALID_KEY) != INI_INVALID_KEY) {
		found = 1;
	}

	return found;
}

int ciniparser_set(dictionary *d, char *entry, char *val)
{
	return dictionary_set(d, strlwc(entry), val);
}

void ciniparser_unset(dictionary *ini, char *entry)
{
	dictionary_unset(ini, strlwc(entry));
}

dictionary *ciniparser_load(const char *ininame)
{
	FILE *in;
	char line[ASCIILINESZ+1];
	char section[ASCIILINESZ+1];
	char key[ASCIILINESZ+1];
	char tmp[ASCIILINESZ+1];
	char val[ASCIILINESZ+1];
	int  last = 0, len, lineno = 0, errs = 0;
	dictionary *dict;

	if ((in = fopen(ininame, "r")) == NULL) {
		fprintf(stderr, "ciniparser: cannot open %s\n", ininame);
		return NULL;
	}

	dict = dictionary_new(0);
	if (!dict) {
		fclose(in);
		return NULL;
	}

	memset(line, 0, ASCIILINESZ + 1);
	memset(section, 0, ASCIILINESZ + 1);
	memset(key, 0, ASCIILINESZ + 1);
	memset(val, 0, ASCIILINESZ + 1);
	last = 0;

	while (fgets(line+last, ASCIILINESZ-last, in)!=NULL) {
		lineno++;
		len = (int) strlen(line)-1;
		/* Safety check against buffer overflows */
		if (line[len] != '\n') {
			fprintf(stderr,
					"ciniparser: input line too long in %s (%d)\n",
					ininame,
					lineno);
			dictionary_del(dict);
			fclose(in);
			return NULL;
		}

		/* Get rid of \n and spaces at end of line */
		while ((len >= 0) &&
				((line[len] == '\n') || (isspace(line[len])))) {
			line[len] = 0;
			len--;
		}

		/* Detect multi-line */
		if (len >= 0 && line[len] == '\\') {
			/* Multi-line value */
			last = len;
			continue;
		}

		switch (ciniparser_line(line, section, key, val)) {
		case LINE_EMPTY:
		case LINE_COMMENT:
			break;

		case LINE_SECTION:
			errs = dictionary_set(dict, section, NULL);
			break;

		case LINE_VALUE:
			snprintf(tmp, ASCIILINESZ + 1, "%s:%s", section, key);
			errs = dictionary_set(dict, tmp, val);
			break;

		case LINE_ERROR:
			fprintf(stderr, "ciniparser: syntax error in %s (%d):\n",
					ininame, lineno);
			fprintf(stderr, "-> %s\n", line);
			errs++;
			break;

		default:
			break;
		}
		memset(line, 0, ASCIILINESZ);
		last = 0;
		if (errs < 0) {
			fprintf(stderr, "ciniparser: memory allocation failure\n");
			break;
		}
	}

	if (errs) {
		dictionary_del(dict);
		dict = NULL;
	}

	fclose(in);

	return dict;
}

void ciniparser_freedict(dictionary *d)
{
	dictionary_del(d);
}

/** @}
 */

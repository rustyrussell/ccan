/* Copyright (c) 2008, Tim Post <tinkertim@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the original program's authors nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
 
/* Some example usages:
 * grawk shutdown '$5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15' messages
 * grawk shutdown '$5, $6, $7, $8, $9, $10, " -- " $1, $2, $3' messages
 * grawk dhclient '$1, $2 " \"$$\"-- " $3' syslog
 * cat syslog | grawk dhclient '$0'
 * cat myservice.log | grawk -F , error '$3'
 *
 * Contributors:
 * Tim Post, Nicholas Clements, Alex Karlov
 * We hope that you find this useful! */

/* FIXME:
 * readline() should probably be renamed
 */

/* TODO:
 * Add a tail -f like behavior that applies expressions and fields
 * Recursive (like grep -r) or at least honor symlinks ? */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>

#define VERSION    "1.0.7"
#define MAINTAINER "Tim Post <echo@echoreply.us>"

/* Storage structure to hold awk-style pattern */
struct awk_pattern
{
	int maxfield;   /* Maximum field number for $# fields */
	int numfields;  /* Number of awk pattern fields */
	char **fields;  /* The awk pattern fields */
};

typedef struct awk_pattern awk_pat_t;

/* Option arguments */
static struct option const long_options[] = {
	{ "ignore-case",     no_argument,       0, 'i' },
	{ "with-filename",   no_argument,       0, 'W' },
	{ "no-filename",     no_argument,       0, 'w' },
	{ "line-number",     no_argument,       0, 'n' },
	{ "field-separator", required_argument, 0, 'F' },
	{ "help",            no_argument,       0, 'h' },
	{ "version",         no_argument,       0, 'v' },
	{ 0, 0, 0, 0}
};

/* The official name of the program */
const char *progname = "grawk";

/* Global for delimiters used in tokenizing strings */
char *tokdelim = NULL;

/* Prototypes */
static void usage(void);
static int process(FILE *, regex_t, awk_pat_t, char *, int);
static int process_line(char *, awk_pat_t, char *, char *);
static int process_files(int, char **, regex_t, awk_pat_t, int, int);
static int process_pipe(regex_t, awk_pat_t, int);
static int awkcomp(awk_pat_t *, char *);
static void awkfree(awk_pat_t *);
static char *readline(FILE *);

static void usage(void)
{
	printf("%s %s\n", progname, VERSION);
	printf("Usage: %s [OPTION] PATTERN OUTPUT_PATTERN file1 [file2]...\n",
			progname);
	printf("Options:\n");
	printf("  --help                       "
		"show help and examples\n");
	printf("  -i, --ignore-case            "
		"ignore case distinctions\n");
	printf("  -W, --with-filename          "
		"Print filename for each match\n");
	printf("  -w, --no-filename            "
		"Never print filename for each match\n");
	printf("  -n, --line-number            "
		"Prefix each line of output with line number.\n");
	printf("  -F fs, --field-separator=fs  "
		"Use fs as the field separator\n");
	printf("  -h, --help                   "
		"Print a brief help summary\n");
	printf("  -v, --version                "
		"Print version information and exit normally\n");
	printf("  PATTERN                      "
		"a basic regular expression\n");
	printf("  OUTPUT_PATTERN               "
		"awk-style print statement; defines "
		"output fields\n");
	printf("\nExamples:\n");
	printf("  Retrieve joe123's home directory from /etc/passwd:\n");
	printf("\t%s -F : \"joe123\" '$6' /etc/passwd\n", progname);
	printf("\n  Find fields 2 3 and 4 on lines that begin with @ from stdin:\n");
	printf("\tcat file.txt | %s \"^@\" '$2,$3,$4'\n", progname);
	printf("\n  Use as a simple grep:\n");
	printf("\t%s \"string to find\" '$0' /file.txt\n", progname);
	printf("\nReport bugs to %s\n", MAINTAINER);
}

/* readline() - read a line from the file handle.
 * Return an allocated string */
static char *readline(FILE *fp)
{
	char *str = (char *)NULL;
	int ch = 0, len = 256, step = 256, i = 0;

	str = (char *)malloc(len);
	if (str == NULL)
		return str;

	while (1) {
		ch = fgetc(fp);
		if (feof(fp))
			break;
		if (ch == '\n' || ch == '\r') {
			str[i++] = 0;
			break;
		}
		str[i++] = ch;
		if (i == len - 2) {
			len += step;
			str = (char *)realloc(str, len);
			if (str == NULL) {
				fclose(fp);
				return str;
			}
		}
	}
	return str;
}

/* process() - this is the actual processing where we compare against a
 * previously compiled grep pattern and output based on the awk pattern.
 * The file is opened by the calling function. We pass in an empty string
 * if we don't want to show the filename. If we want to show the line number,
 * the value of show_lineno is 1. If we find a line, return 1. If no line is
 * found, return 0. If an error occurs, return -1. */
static int process(FILE *fp, regex_t re, awk_pat_t awk,
	char *filename, int show_lineno)
{
	char *inbuf = NULL;
	char slineno[32];
	memset(slineno, 0, sizeof(slineno));
	long lineno = 0;
	int found = 0;

	while (1) {
		inbuf = readline(fp);
		if (!inbuf)
			break;
		if (feof(fp))
			break;
		lineno++;
		if (regexec(&re, inbuf, (size_t)0, NULL, 0) == 0) {
			found = 1;  // Found a line.
			if (show_lineno)
				sprintf(slineno, "%ld:", lineno);
			if (process_line(inbuf, awk, filename, slineno)) {
				fprintf (stderr, "Error processing line [%s]\n", inbuf);
				free (inbuf);
				return -1;
			}
		}
		free (inbuf);
	}

	if (inbuf)
		free(inbuf);

	return found;
}

/* process_files() - process one or more files from the command-line.
 * If at least one line is found, return 1, else return 0 if no lines
 * were found or an error occurs. */
static int process_files(int numfiles, char **files, regex_t re, awk_pat_t awk,
		int show_filename, int show_lineno)
{
	int i, found = 0;
	FILE *fp = NULL;
	struct stat fstat;
	char filename[1024];
	memset(filename, 0, sizeof(filename));

	for(i = 0; i < numfiles; i++) {
		if (stat(files[i], &fstat) == -1) {
			/* Did a file get deleted from the time we started running? */
			fprintf (stderr,
				"Error accessing file %s. No such file\n", files[i]);
			continue;
		}
		if (show_filename)
			sprintf( filename, "%s:", files[i] );
		/* For now, we aren't recursive. Perhaps allow symlinks? */
		if ((fstat.st_mode & S_IFMT) != S_IFREG)
			continue;
		if (NULL == (fp = fopen(files[i], "r"))) {
			fprintf(stderr,
				"Error opening file %s. Permission denied\n", files[i]);
			continue;
		}
		if (process(fp, re, awk, filename, show_lineno) == 1)
			found = 1;
		fclose(fp);
	}

	return found;
}

/* process_pipe() - process input from stdin */
static int process_pipe(regex_t re, awk_pat_t awk, int show_lineno)
{
	if (process(stdin, re, awk, "", show_lineno) == 1)
		return 1;

	return 0;
}

/* process_line() - process the line based on the awk-style pattern and output
 * the results. */
static int process_line(char *inbuf, awk_pat_t awk, char *filename, char *lineno)
{
	char full_line[3] = { '\1', '0', '\0' };

	if (awk.numfields == 1 && strcmp(awk.fields[0], full_line) == 0) {
		/* If the caller only wants the whole string, oblige, quickly. */
		fprintf (stdout, "%s%s%s\n", filename, lineno, inbuf);
		return 0;
	}

	/* Build an array of fields from the line using strtok()
	 * TODO: make this re-entrant so that grawk can be spawned as a thread */
	char **linefields = (char **)malloc((awk.maxfield + 1) * sizeof(char *));
	char *wrkbuf = strdup(inbuf), *tbuf;

	int count = 0, n = 1, i;
	for (i = 0; i < (awk.maxfield + 1); i++) {
		linefields[i] = NULL;
	}

	tbuf = strtok(wrkbuf, tokdelim);
	if(tbuf)
		linefields[0] = strdup(tbuf);

	while (tbuf != NULL) {
		tbuf = strtok(NULL, tokdelim);
		if (!tbuf)
			break;
		count++;
		if (count > awk.maxfield)
			break;
		linefields[count] = strdup(tbuf);
		if (!linefields[count]) {
			fprintf(stderr, "Could not allocate memory to process file %s\n",
				filename);
			return -1;
		}
	}
	/* For each field in the awk structure,
	 * find the field and print it to stdout.*/
	fprintf(stdout, "%s%s", filename, lineno);	/* if needed */
	for (i = 0; i < awk.numfields; i++) {
		if (awk.fields[i][0] == '\1') {
			n = atoi(&awk.fields[i][1]);
			if (n == 0) {
				fprintf(stdout, "%s", inbuf);
				continue;
			}
			if (linefields[n-1])
				fprintf(stdout, "%s", linefields[n-1]);
			continue;
		} else
			fprintf(stdout, "%s", awk.fields[i]);
	}
	fprintf(stdout, "\n");
	/* Cleanup */
	if (wrkbuf)
		free(wrkbuf);

	for (i = 0; i < count; i++) {
		free(linefields[i]);
		linefields[i] = (char *) NULL;
	}

	free(linefields);
	linefields = (char **)NULL;

	return 0;
}

/* awkcomp() - little awk-style print format compilation routine.
 * Returns structure with the apattern broken down into an array for easier
 * comparison and printing. Handles string literals as well as fields and
 * delimiters. Example: $1,$2 " \$ and \"blah\" " $4
 * Returns -1 on error, else 0. */
static int awkcomp(awk_pat_t *awk, char *apattern)
{
	awk->maxfield = 0;
	awk->numfields = 0;
	awk->fields = NULL;
	awk->fields = (char **)malloc(sizeof(char *));

	int i, num = 0;
	char *wrkbuf;

	wrkbuf = (char *)malloc(strlen(apattern) + 1);
	if (wrkbuf == NULL) {
		free(awk);
		fprintf(stderr, "Memory allocation error (wrkbuf) in awkcomp()\n");
		return -1;
	}

	int inString = 0, offs = 0;
	char ch;
	for (i = 0; i < strlen( apattern ); i++) {
		ch = apattern[i];
		if (inString && ch != '"' && ch != '\\') {
			wrkbuf[offs++] = ch;
			continue;
		}
		if (ch == ' ')
			continue;
		switch (ch) {
		/* Handle delimited strings inside of literal strings */
		case '\\':
			if (inString) {
				wrkbuf[offs++] = apattern[++i];
				continue;
			} else {
				/* Unexpected and unconventional escape (can get these
				 * from improper invocations of sed in a pipe with grawk),
				 * if sed is used to build the field delimiters */
				fprintf(stderr,
					"Unexpected character \'\\\' in output format\n");
				return -1;
			}
			break;
		/* Beginning or ending of a literal string */
		case '"':
			inString = !inString;
			if (inString)
				continue;
			break;
		/* Handle the awk-like $# field variables */
		case '$':
			/* We use a non-printable ASCII character to
			 * delimit the string field values.*/
			wrkbuf[offs++] = '\1';
			/* We also need the max. field number */
			num = 0;
			while (1) {
				ch = apattern[++i];
				/* Not a number, exit this loop */
				if (ch < 48 || ch > 57) {
					i--;
					break;
				}
				num = (num * 10) + (ch - 48);
				wrkbuf[offs++] = ch;
			}
			if (num > awk->maxfield)
				awk->maxfield = num;
			/* Incomplete expression, a $ not followed by a number */
			if (wrkbuf[1] == 0) {
				fprintf(stderr, "Incomplete field descriptor at "
						"or near character %d in awk pattern\n", i+1);
				return -1;
			}
			break;
		/* Field separator */
		case ',':
			wrkbuf[offs++] = ' ';
			break;
		}
		/* if wrkbuf has nothing, we've got rubbish. Continue in the hopes
		 * that something else makes sense. */
		if (offs == 0)
			continue;
		/* End of a field reached, put it into awk->fields */
		wrkbuf[offs] = '\0';
		awk->fields =
			(char **)realloc(awk->fields, (awk->numfields + 1)
				* sizeof(char *));
		if (!awk->fields ) {
			fprintf(stderr,
				"Memory allocation error (awk->fields) in awkcomp()\n");
			return -1;
		}
		awk->fields[awk->numfields] = strdup(wrkbuf);
		if (!awk->fields[awk->numfields]) {
			fprintf(stderr,
				"Memory allocation error (awk->fields[%d]) in awkcomp()\n",
					awk->numfields);
			return -1;
		}
		memset(wrkbuf, 0, strlen(apattern) + 1);
		awk->numfields++;
		offs = 0;
	}

	free(wrkbuf);

	if (awk->numfields == 0) {
		fprintf(stderr,
			"Unable to parse and compile the pattern; no fields found\n");
		return -1;
	}

	return 0;
}

/* awkfree() - free a previously allocated awk_pat structure */
static void awkfree(awk_pat_t *awk )
{
	int i;
	for (i = 0; i < awk->numfields; i++)
		free(awk->fields[i]);

	free(awk->fields);
}

int main(int argc, char **argv)
{
	char *apattern = NULL, *gpattern = NULL;
	char **files = NULL;
	int numfiles = 0, i = 0, c = 0;
	int ignore_case = 0, no_filename = 0, with_filename = 0, line_number = 0;

	if (argc < 3) {
		usage();
		return EXIT_FAILURE;
	}

	tokdelim = strdup("\t\r\n ");
	while (1) {
		int opt_ind = 0;
		while (c != -1) {
			c = getopt_long(argc, argv, "wWhinF:", long_options, &opt_ind);
			switch (c) {
			case 'w':
				with_filename = 0;
				no_filename = 1;
				break;
			case 'i':
				ignore_case = 1;
				break;
			case 'W':
				with_filename = 1;
				no_filename = 0;
				break;
			case 'n':
				line_number = 1;
				break;
			case 'F':
				tokdelim = realloc(tokdelim, 3 + strlen(optarg) + 1);
				memset(tokdelim, 0, 3 + strlen( optarg ) + 1);
				sprintf(tokdelim, "\t\r\n%s", optarg);
				break;
			case 'h':
				usage();
				free(tokdelim);
				return EXIT_SUCCESS;
				break;
			case 'v':
				printf("%s\n", VERSION);
				free(tokdelim);
				return EXIT_SUCCESS;
				break;
			}
		}

		/* Now we'll grab our patterns and files. */
		if ((argc - optind) < 2) {
			usage();
			free(tokdelim);
			return EXIT_FAILURE;
		}

		/* pattern one will be our "grep" pattern */
		gpattern = strdup(argv[optind]);
		if (gpattern == NULL) {
			fprintf(stderr, "Memory allocation error");
			exit(EXIT_FAILURE);
		}
		optind++;

		/* pattern two is our "awk" pattern */
		apattern = strdup(argv[optind]);
		if(apattern == NULL) {
			fprintf(stderr, "Memory allocation error");
			exit(EXIT_FAILURE);
		}
		optind++;

		/* Anything that remains is a file or wildcard which should be
		 * expanded by the calling shell. */
		if (optind < argc) {
			numfiles = argc - optind;
			files = (char **)malloc(sizeof(char *) * (numfiles + 1));
			for (i = 0; i < numfiles; i++) {
				files[i] = strdup(argv[optind + i]);
			}
		}
		/* If the number of files is greater than 1 then we default to
		 * showing the filename unless specifically directed against it.*/
		if (numfiles > 1 && no_filename == 0)
			with_filename = 1;
		break;
	}

	/* Process everything */
	regex_t re;
	int cflags = 0, rc = 0;

	if (ignore_case)
		cflags = REG_ICASE;
	/* compile the regular expression parser */
	if (regcomp(&re, gpattern, cflags)) {
		fprintf(stderr,
			"Error compiling grep-style pattern [%s]\n", gpattern);
		return EXIT_FAILURE;
	}

	awk_pat_t awk;
	if (awkcomp(&awk, apattern))
	{
		fprintf(stderr,
			"Error compiling awk-style pattern [%s]\n", apattern);
		return EXIT_FAILURE;
	}

	if (numfiles > 0) {
		if(process_files(
			numfiles, files, re, awk, with_filename, line_number) == 0)
			rc = 255;   // We'll return 255 if no lines were found.
	} else {
		if(process_pipe(re, awk, line_number) == 0)
			rc = 255;
	}

	/* Destructor */
	for (i = 0; i < numfiles; i++) {
		if (files[i])
			free(files[i]);
	}
	free(files);

	/* Awk pattern */
	free(apattern);

	/* Grep pattern */
	free(gpattern);

	/* Grep regex */
	regfree(&re);

	/* Awk pattern structure */
	awkfree(&awk);

	/* Token delimiter (might have been freed elsewhere) */
	if (tokdelim)
		free(tokdelim);
	return rc;
}

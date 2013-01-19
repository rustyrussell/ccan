#include <log.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

// escape codes for different colors on the terminal
#define FG_RED		"\033[31m"
#define FG_YELLOW	"\033[33m"
#define FG_BLUE		"\033[34m"
#define FG_PURPLE	"\033[35m"
#define TEXT_BOLD 	"\033[1m"
#define COLOR_END	"\033[0m" // reset colors to default

static FILE *_logging_files[16] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int  _log_current_mode, _current_log_file = 1;

FILE *set_log_file(char *filename)
{
	if (_current_log_file+1 > 16) return NULL; // we only support 16 different files
	_logging_files[_current_log_file++] = fopen(filename, "a+");
	return _logging_files[_current_log_file];
}

void set_log_mode(int mode)
{
	_log_current_mode = mode;
}

void get_log_mode()
{
	return _log_current_mode;
}

// this function is headache-inducing.
void _print_log(int loglevel, char *file, const char *func, char *clock, int line, char *msg, ...)
{
	_logging_files[0] = stdout;
	va_list args;
	va_start(args, msg);
	
	// append the varargs to the buffer we want to print.
	// this is so that our pipe chars don't get fucked later.
	// also, make sure we don't get an invalid loglevel.
	char buffer[strlen(msg) + 1024];
	vsnprintf(buffer, strlen(msg)+1024, msg, args);	
	if (loglevel < 0 || loglevel > 3) loglevel = LOG_INVALID;

	// set console color for printing the tag
	switch (loglevel) {
		case LOG_CRITICAL:
			printf(TEXT_BOLD FG_RED);
			break;
		case LOG_ERROR:
			printf(FG_RED);
			break;
		case LOG_WARNING:
			printf(FG_YELLOW);
			break;
		case LOG_INFO:
			printf(FG_BLUE);
			break;
		case LOG_INVALID:
			printf(FG_PURPLE);
			break;
	}
	// print the log tag
	int i;
	for (i=0; i < 16; i++)
		if (_logging_files[i])
			fprintf(_logging_files[i], "%s", log_tags[_log_current_mode][loglevel]);
		else break;
	printf(COLOR_END);
	
	if (_log_current_mode == LOG_VERBOSE) {
		for (i=0; i < 16; i++)
			if (_logging_files[i])
				fprintf(_logging_files[i],
					"%s() (%s:%d) at %s |\t",
					func, file, line, clock);
			else break;
	}

	// print the first line
	 char *to_print = strtok(buffer, "\n");
	 for (i = 0; i < 16; i++)
	 	if (_logging_files[i])
	 		fprintf(_logging_files[i], "%s\n", to_print);
		else break;

	// for these next lines, add a pipe and tab once.
	while (to_print = strtok(NULL, "\n")) {
		for (i = 0; i < 16; i++)
			if (_logging_files[i])
				fprintf(_logging_files[i], "%s\n", to_print);
			else break;
	}
}

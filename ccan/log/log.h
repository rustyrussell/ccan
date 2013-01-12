#pragma once

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <colors.h>
#include <time.h>

#define LOG_CRITICAL 0
#define LOG_ERROR    1
#define LOG_WARNING  2
#define LOG_INFO     3
#define LOG_INVALID  4

static 
char *log_tags[2][5] = { { "[CRITICAL] ",
		           "[ERROR]    ",
		           "[WARNING]  ",
		           "[INFO]     ", 
		           "[INVALID LOG LEVEL] "},
		       {   "[!] ",
			   "[*] ",
			   "[-] ",
			   "[+] ",
			   "[~] " }};

extern FILE *_logging_file;
extern int _log_current_mode;
#define LOG_VERBOSE 0
#define LOG_CONCISE 1

extern FILE *set_log_file(char *);

extern void set_log_mode(int);

#define print_log(level, ...) do {				\
				time_t _clk = time(NULL);	\
				_print_log(level, __FILE__, __func__, ctime(&_clk), __LINE__, __VA_ARGS__); \
			     } while (0)

extern void _print_log(int, char *, char *, char*, int, char *, ...);

#ifdef TEST
extern FILE *_logging_file;
int main(int argc, char *argv[])
{
	set_log_file("test.log");
	set_log_mode(LOG_VERBOSE);
	print_log(LOG_CRITICAL, "This is a critical log with variable:\ni=%d", 3);
	print_log(LOG_ERROR, "This is an error log with variable:\nj=%d", 4);
	print_log(LOG_WARNING, "This is a warning log.");
	print_log(LOG_INFO, "This is an info log.");
	print_log(43, "This loglevel is invalid.\n");
	print_log(LOG_INFO, "THis is a log\nwith multiple newlines\nSo that you suffer while\nreading this.");
	log(LOG_CRITICAL, "This is a critical message.");
	log(LOG_ERROR, "This is an error message.");
	log(LOG_WARNING, "This is a warning message.");
	log(LOG_INFO, "This is an info message.");
	log(LOG_INFO, "This is an info message\nwith multiple lines\nin order to test\nhow printing is handled.");
	set_log_mode(LOG_CONCISE);
	print_log(LOG_CRITICAL, "This is a critical log with variable:\ni=%d", 3);
	print_log(LOG_ERROR, "This is an error log with variable:\nj=%d", 4);
	return 0;
}
#endif // TEST

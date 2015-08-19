/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_PR_LOG_H_
#define CCAN_PR_LOG_H_

#include <stdbool.h>
#include <ccan/compiler/compiler.h>

/*
 * pr_emerg, pr_alert, pr_crit, pr_error, pr_warn, pr_notice, pr_info, pr_debug
 *
 * Each of these prints a format string only in the case where we're running at
 * the selected debug level.
 *
 * Note that using these functions also causes a pre-pended log level to be
 * included in the printed string.
 *
 * Log levels correspond to those used in syslog(3) .
 */
#define pr_emerg(...)  pr_log_(LOG_EMERG  __VA_ARGS__)
#define pr_alert(...)  pr_log_(LOG_ALERT  __VA_ARGS__)
#define pr_crit(...)   pr_log_(LOG_CRIT   __VA_ARGS__)
#define pr_error(...)  pr_log_(LOG_ERROR  __VA_ARGS__)
#define pr_warn(...)   pr_log_(LOG_WARN   __VA_ARGS__)
#define pr_notice(...) pr_log_(LOG_NOTICE __VA_ARGS__)
#define pr_info(...)   pr_log_(LOG_INFO   __VA_ARGS__)
#define pr_debug(...)  pr_log_(LOG_DEBUG  __VA_ARGS__)

#ifdef DEBUG
# define pr_devel(...)  pr_debug(__VA_ARGS__)
#else
static PRINTF_FMT(1,2) inline void pr_check_printf_args(const char *fmt, ...)
{
	(void)fmt;
}
# define pr_devel(...) pr_check_printf_args(__VA_ARGS__)
#endif

#ifndef CCAN_PR_LOG_DEFAULT_LEVEL
# define CCAN_PR_LOG_DEFAULT_LEVEL 6
#endif

#define LOG_EMERG  "<0>"
#define LOG_ALERT  "<1>"
#define LOG_CRIT   "<2>"
#define LOG_ERROR  "<3>"
#define LOG_WARN   "<4>"
#define LOG_NOTICE "<5>"
#define LOG_INFO   "<6>"
#define LOG_DEBUG  "<7>"

#ifndef CCAN_PR_LOG_DISABLE
/**
 * pr_log_ - print output based on the given logging level
 *
 * Example:
 *
 *	pr_log_(LOG_EMERG "something went terribly wrong\n");
 *	pr_log_(LOG_DEBUG "everything is fine\n");
 */
void PRINTF_FMT(1,2) pr_log_(char const *fmt, ...);
bool debug_is(int lvl);
int debug_level(void);
#else
static PRINTF_FMT(1,2) inline void pr_log_(char const *fmt, ...)
{
	(void)fmt;
}
static inline bool debug_is(int lvl) { (void)lvl; return false; }
static inline int debug_level(void) { return -1; }
#endif

#endif

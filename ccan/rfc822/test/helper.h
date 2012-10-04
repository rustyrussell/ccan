#include <ccan/tap/tap.h>

void failtest_setup(int argc, char *argv[]);
void allocation_failure_check(void);

#define CHECK_HEADER_NUMTESTS	4
void check_header(struct rfc822_msg *msg, struct rfc822_header *h,
		  const char *name, const char *val,
		  enum rfc822_header_errors experr, int crlf);

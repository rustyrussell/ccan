#include <ccan/foreach/foreach.h>
#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <string.h>

#define CCAN_RFC822_DEBUG

#include <ccan/rfc822/rfc822.h>

#include <ccan/rfc822/rfc822.c>

#include "testdata.h"
#include "helper.h"

#define UNFOLDED " This is a string with\tlots of \tplaces to fold"
#define FOLD_POINTS 11

#define BEFORE "Date: Tue, 22 Feb 2011 00:15:59 +1100\n" \
	"From: Mister From <from@example.com>\n" \
	"To: Mizz To <to@example.org>\n" \
	"Subject:"

#define AFTER "Message-ID: <20110221131559.GA28327@example>\n" \
	"\n" \
	"body"

static struct bytestring fold_and_assemble(int foldat, int crlf, int truncated)
{
	char *buf, *p;
	int i, n = 0;

	buf = tal_arr(NULL, char, strlen(BEFORE) + strlen(AFTER) + 3*strlen(UNFOLDED) + 2);
	if (!buf)
		exit(0);

	memcpy(buf, BEFORE, strlen(BEFORE));

	p = buf + strlen(BEFORE);

	for (i = 0; i < strlen(UNFOLDED); i++) {
		if (rfc822_iswsp(UNFOLDED[i])) {
			n++;
			if ((foldat == -1) || (foldat == n)) {
				if (crlf)
					*p++ = '\r';
				*p++ = '\n';
			}
		}
		*p++ = UNFOLDED[i];
	}

	if (!truncated) {
		if (crlf)
			*p++ = '\r';
		*p++ = '\n';

		memcpy(p, AFTER, strlen(AFTER));
		p += strlen(AFTER);
	}

	return bytestring(buf, p - buf);
}

static void check_folded_header(const char *buf, size_t len)
{
	struct rfc822_msg *msg;
	struct rfc822_header *hdr;
	struct bytestring hunfold;

	msg = rfc822_start(NULL, buf, len);
	allocation_failure_check();

	hdr = rfc822_first_header(msg);
	allocation_failure_check();
	hdr = rfc822_next_header(msg, hdr);
	allocation_failure_check();
	hdr = rfc822_next_header(msg, hdr);
	allocation_failure_check();

	/* This is the one we care about */
	hdr = rfc822_next_header(msg, hdr);
	allocation_failure_check();

	ok(hdr && (rfc822_header_errors(msg, hdr) == 0), "Folded header valid");
	allocation_failure_check();

	hunfold = rfc822_header_unfolded_value(msg, hdr);
	allocation_failure_check();

	ok(hunfold.len == strlen(UNFOLDED), "Unfolded length %zd, should be %zd",
	   hunfold.len, strlen(UNFOLDED));
	ok1(memcmp(hunfold.ptr, UNFOLDED, hunfold.len) == 0);

	rfc822_free(msg);
	allocation_failure_check();
}

int main(int argc, char *argv[])
{
	struct bytestring msgbuf;
	int crlf, truncated, i;

	failtest_setup(argc, argv);

	plan_tests(3 * 2 * 2 * (FOLD_POINTS + 2));

	foreach_int(crlf, 0, 1) {
		foreach_int(truncated, 0, 1) {
			for (i = -1; i <= FOLD_POINTS; i++) {
				msgbuf = fold_and_assemble(i, crlf, truncated);
				check_folded_header(msgbuf.ptr, msgbuf.len);
				tal_free(msgbuf.ptr);
			}
		}
	}

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}

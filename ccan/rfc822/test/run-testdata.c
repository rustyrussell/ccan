#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <ccan/tap/tap.h>
#include <ccan/array_size/array_size.h>

#define CCAN_RFC822_DEBUG

#include <ccan/rfc822/rfc822.h>
#include <ccan/rfc822/rfc822.c>

#include "testdata.h"

/* Test some of the test infrastructure */

static const char test_msg_1_cmp[] = 
	"Date:Tue, 22 Feb 2011 00:15:59 +1100\n"
	"From:Mister From <from@example.com>\n"
	"To:Mizz To <to@example.org>\n"
	"Subject:Some subject\n"
	"Message-ID:<20110221131559.GA28327@example>\n"
	"MIME-Version:1.0\n"
	"Content-Type:text/plain; charset=us-ascii\n"
	"Content-Disposition:inline\n"
	"\n"
	"Test message\n";

static void test_assemble(const struct aexample *e, int crlf,
			  const char *cmp, size_t cmplen)
{
	const char *msg;
	size_t len;

	msg = assemble_msg(e, &len, crlf);
	ok1(msg != NULL);
	fprintf(stderr, "Assembled message %zd bytes (versus %zd bytes)\n",
		len, cmplen);
	ok1(len == cmplen);
	ok1(memcmp(msg, cmp, cmplen) == 0);
	tal_free(msg);
}

int main(int argc, char *argv[])
{
	plan_tests(3);

	test_assemble(&test_msg_1, 0, test_msg_1_cmp, sizeof(test_msg_1_cmp) - 1);
	exit(exit_status());
}

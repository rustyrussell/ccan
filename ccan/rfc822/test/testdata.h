#ifndef RFC822_TESTDATA_H
#define RFC822_TESTDATA_H

#include <ccan/tal/str/str.h>
#include <ccan/array_size/array_size.h>
#include <ccan/foreach/foreach.h>

struct testhdr {
	const char *name, *val;
	int index, last;
	enum rfc822_header_errors errors;
};

struct aexample {
	const char *name;
	struct testhdr *hdrs;
	size_t nhdrs;
	const char *body;
};

#define AEXAMPLE(s)				\
	struct aexample s = {		\
		.name = #s,			\
		.hdrs = s##_hdrs,		\
		.nhdrs = ARRAY_SIZE(s##_hdrs),	\
		.body = s##_body,		\
	};

struct testhdr test_msg_1_hdrs[] = {
	{"Date", "Tue, 22 Feb 2011 00:15:59 +1100"},
	{"From", "Mister From <from@example.com>"},
	{"To", "Mizz To <to@example.org>"},
	{"Subject", "Some subject"},
	{"Message-ID", "<20110221131559.GA28327@example>"},
	{"MIME-Version", "1.0"},
	{"Content-Type", "text/plain; charset=us-ascii"},
	{"Content-Disposition", "inline"},
};
const char test_msg_1_body[] = "Test message\n";
AEXAMPLE(test_msg_1);

#define test_msg_empty_body_hdrs test_msg_1_hdrs
const char test_msg_empty_body_body[] = "";
AEXAMPLE(test_msg_empty_body);

#define test_msg_nlnl_lf_hdrs test_msg_1_hdrs
const char test_msg_nlnl_lf_body[] = "Message containing \n\n inside body\n";
AEXAMPLE(test_msg_nlnl_lf);

#define test_msg_nlnl_crlf_hdrs test_msg_1_hdrs
const char test_msg_nlnl_crlf_body[] = "Message containing \r\n\r\n inside body\r\n";
AEXAMPLE(test_msg_nlnl_crlf);

#define test_msg_nlnl_mixed_hdrs test_msg_1_hdrs
const char test_msg_nlnl_mixed_body[] = "Message containing both \n\n and \r\n\r\n inside body\n\r\n";
AEXAMPLE(test_msg_nlnl_mixed);

#define test_msg_space_body_hdrs test_msg_1_hdrs
const char test_msg_space_body_body[] = " Message with LWS at start of body\n";
AEXAMPLE(test_msg_space_body);

struct testhdr bad_hdrs_hdrs[] = {
	{"From", "Mister From <from@example.com>"},
	{"To", "Mizz To <to@example.org>"},
	{"X-Bad-\bName", "This header field has bad characters in the name",
		 .errors = RFC822_HDR_BAD_NAME_CHARS},
	{"Subject", "Some subject"},
	{"Message-ID", "<20110221131559.GA28327@example>"},
};
#define bad_hdrs_body test_msg_1_body
AEXAMPLE(bad_hdrs)

struct testhdr repeated_hdrs_1_hdrs[] = {
	{"X-Repeated-Header", "#1", 0, 4},
	{"x-repeated-header", "#2", 1, 4},
	{"X-REPEATED-HEADER", "#3", 2, 4},
	{"x-rEpEaTeD-hEaDeR", "#4", 3, 4},
	{"X-Repeated-Header", "#5", 4, 4},
};
#define repeated_hdrs_1_body test_msg_1_body
AEXAMPLE(repeated_hdrs_1);

struct testhdr prefix_hdr_hdrs[] = {
	{"X-Prefix", "Prefix", 0},
	{"X-Prefix-and-Suffix", "Suffix", 0},
};
#define prefix_hdr_body test_msg_1_body
AEXAMPLE(prefix_hdr);

#define for_each_aexample(_e)				     \
	foreach_ptr((_e), &test_msg_1, &test_msg_empty_body, \
		    &test_msg_nlnl_lf, &test_msg_nlnl_crlf, \
		    &test_msg_nlnl_mixed, \
		    &test_msg_space_body, \
		    &bad_hdrs,		  \
		    &repeated_hdrs_1,	  \
		    &prefix_hdr)

#define for_each_aexample_buf(_e, _buf, _len)	\
	for_each_aexample((_e)) 		\
	if (((_buf) = assemble_msg((_e), &(_len))) != NULL)

static inline int num_aexamples(void)
{
	const struct aexample *e;
	int n = 0;

	for_each_aexample(e)
		n++;

	return n;
}

static inline int num_aexample_hdrs(void)
{
	const struct aexample *e;
	int n = 0;

	for_each_aexample(e)
		n += e->nhdrs;

	return n;
}

static inline const char *assemble_msg(const struct aexample *e,
				       size_t *len, int crlf)
{
	const char *nl = crlf ? "\r\n" : "\n";
	int nln = crlf ? 2 : 1;
	char *msg;
	size_t n = 0;
	int i;

	msg = tal_strdup(NULL, "");
	if (!msg)
		return NULL;

	for (i = 0; i < e->nhdrs; i++) {
		if (!tal_append_fmt(&msg, "%s:%s%s", e->hdrs[i].name,
				    e->hdrs[i].val, nl)) {
			tal_free(msg);
			return NULL;
		}
		n += strlen(e->hdrs[i].name) + strlen(e->hdrs[i].val) + 1 + nln;
	}
	if (!tal_append_fmt(&msg, "%s%s", nl, e->body)) {
		tal_free(msg);
		return NULL;
	}
	n += strlen(e->body) + nln;
	*len = n;
	return msg;
}

#define NLT(crlf)	((crlf) ? "CRLF" : "LF")

#endif /* RFC822_TESTDATA_H */

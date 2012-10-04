/* Licensed under LGPLv2.1+ - see LICENSE file for details */

#include "config.h"

#include <string.h>

#include <ccan/str/str.h>
#include <ccan/talloc/talloc.h>
#include <ccan/list/list.h>

#include <ccan/rfc822/rfc822.h>

#if !HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen)
{
	const char *p, *last;

	p = haystack;
	last = p + haystacklen - needlelen;

	do {
		if (memcmp(p, needle, needlelen) == 0)
			return (void *)p;
	} while (p++ <= last);

	return NULL;
}
#endif

static void (*allocation_failure_hook)(const char *);

static void NORETURN default_allocation_failure(const char *s)
{
	fprintf(stderr, "ccan/rfc822: Allocation failure: %s", s);
	abort();
}

static void allocation_failure(const char *s)
{
	if (allocation_failure_hook)
		(*allocation_failure_hook)(s);
	else
		default_allocation_failure(s);
}

void rfc822_set_allocation_failure_handler(void (*h)(const char *))
{
	allocation_failure_hook = h;
}

#define ALLOC_CHECK(p, r) \
	do { \
		if (!(p)) { \
			allocation_failure(__FILE__ ":" stringify(__LINE__)); \
			return (r); \
		} \
	} while (0)

struct rfc822_msg {
	const char *data, *end;
	const char *remainder;
	struct list_head headers;
	const char *body;
};

struct rfc822_header {
	struct bytestring all, rawname, rawvalue;
	struct bytestring unfolded;
	struct list_node list;
};

struct rfc822_msg *rfc822_check(const struct rfc822_msg *msg,
				const char *abortstr)
{
	assert(msg);
	if (!list_check(&msg->headers, abortstr))
                return NULL;
        return (struct rfc822_msg *)msg;
}

#ifdef CCAN_RFC822_DEBUG
#define CHECK(msg, str)	do { rfc822_check((msg), (str)); } while (0)
#else
#define CHECK(msg, str)	do { } while (0)
#endif

struct rfc822_msg *rfc822_start(const void *ctx, const char *p, size_t len)
{
	struct rfc822_msg *msg;

	msg = talloc(ctx, struct rfc822_msg);
	ALLOC_CHECK(msg, NULL);

	msg->data = p;
	msg->end = p + len;

	msg->remainder = msg->data;
	msg->body = NULL;

	list_head_init(&msg->headers);

	CHECK(msg, "<rfc22_start");

	return msg;
}

void rfc822_free(struct rfc822_msg *msg)
{
	CHECK(msg, ">rfc822_free");
	talloc_free(msg);
}

static struct rfc822_header *next_header_cached(struct rfc822_msg *msg,
						struct rfc822_header *hdr)
{
	struct list_node *h = &msg->headers.n;
	const struct list_node *n = h;

	CHECK(msg, ">next_header_cached");

	if (hdr)
		n = &hdr->list;

	if (n->next == h)
		return NULL;

	CHECK(msg, "<next_header_cached");

	return list_entry(n->next, struct rfc822_header, list);
}

static const char *next_line(const char *start, const char *end)
{
	const char *p = memchr(start, '\n', end - start);

	return p ? (p + 1) : end;
}

static struct rfc822_header *next_header_parse(struct rfc822_msg *msg)
{
	const char *h, *eh, *ev, *colon;
	struct rfc822_header *hi;

	CHECK(msg, ">next_header_parse");

	if (!msg->remainder)
		return NULL;

	if (msg->body && (msg->remainder >= msg->body))
		return NULL;

	h = msg->remainder;
	eh = next_line(h, msg->end);

	ev = eh;
	if ((ev > h) && (ev[-1] == '\n'))
		ev--;
	if ((ev > h) && (ev[-1] == '\r'))
		ev--;
	if (ev == h) {
		/* Found the end of the headers */

		assert(!msg->body || (msg->body == eh));

		if (eh < msg->end)
			msg->body = eh;
		return NULL;
	}

	while ((eh < msg->end) && rfc822_iswsp(*eh))
		eh = next_line(eh, msg->end);

	if (eh >= msg->end)
		msg->remainder = NULL;
	else
		msg->remainder = eh;


	hi = talloc_zero(msg, struct rfc822_header);
	ALLOC_CHECK(hi, NULL);

	hi->all = bytestring(h, eh - h);
	list_add_tail(&msg->headers, &hi->list);

	colon = memchr(h, ':', hi->all.len);
	if (colon) {
		hi->rawname = bytestring(h, colon - h);
		hi->rawvalue = bytestring(colon + 1, eh - colon - 1);
	} else {
		hi->rawname = bytestring_NULL;
		hi->rawvalue = bytestring_NULL;
	}

	CHECK(msg, "<next_header_parse");

	return hi;
}

struct rfc822_header *rfc822_next_header(struct rfc822_msg *msg,
					 struct rfc822_header *hdr)
{
	struct rfc822_header *h;

	CHECK(msg, ">rfc822_next_header");

	h = next_header_cached(msg, hdr);
	if (h)
		return h;

	return next_header_parse(msg);
}

struct bytestring rfc822_body(struct rfc822_msg *msg)
{
	CHECK(msg, ">rfc822_body");

	if (!msg->body && msg->remainder) {
		const char *p, *q;

		p = memmem(msg->remainder, msg->end - msg->remainder,
			   "\n\r\n", 3);
		q = memmem(msg->remainder, msg->end - msg->remainder,
			   "\n\n", 2);

		if (p && (!q || (p < q)))
			msg->body = p + 3;
		else if (q && (!p || (q < p)))
			msg->body = q + 2;

		if (msg->body >= msg->end) {
			assert(msg->body == msg->end);
			msg->body = NULL;
		}
	}

	CHECK(msg, "<rfc822_body");

	if (msg->body)
		return bytestring(msg->body, msg->end - msg->body);
	else
		return bytestring_NULL;
}

enum rfc822_header_errors rfc822_header_errors(struct rfc822_msg *msg,
					       struct rfc822_header *hdr)
{
	enum rfc822_header_errors err = 0;
	int i;

	if (!hdr->rawname.ptr) {
		err |= RFC822_HDR_NO_COLON;
	} else {
		for (i = 0; i < hdr->rawname.len; i++) {
			char c = hdr->rawname.ptr[i];

			assert(c != ':');

			if ((c < 33) || (c > 126)) {
				err |= RFC822_HDR_BAD_NAME_CHARS;
				break;
			}
		}
	}
	return err;
}

struct bytestring rfc822_header_raw_content(struct rfc822_msg *msg,
					    struct rfc822_header *hdr)
{
	return hdr->all;
}

struct bytestring rfc822_header_raw_name(struct rfc822_msg *msg,
					 struct rfc822_header *hdr)
{
	return hdr->rawname;
}

struct bytestring rfc822_header_raw_value(struct rfc822_msg *msg,
					  struct rfc822_header *hdr)
{
	return hdr->rawvalue;
}

static void get_line(struct bytestring in, struct bytestring *first,
		     struct bytestring *rest)
{
	size_t rawlen, trimlen;
	const char *inp = in.ptr;
	const char *nl;

	nl = memchr(inp, '\n', in.len);
	if (!nl)
		rawlen = in.len;
	else
		rawlen = nl - inp + 1;

	trimlen = rawlen;
	if ((trimlen > 0) && (inp[trimlen-1] == '\n')) {
		trimlen--;
		if ((trimlen > 0) && (inp[trimlen-1] == '\r'))
			trimlen--;
	}

	*first = bytestring(in.ptr, trimlen);

	if (rawlen < in.len)
		*rest = bytestring(in.ptr + rawlen, in.len - rawlen);
	else
		*rest = bytestring_NULL;
}


struct bytestring rfc822_header_unfolded_value(struct rfc822_msg *msg,
					       struct rfc822_header *hdr)
{
	struct bytestring raw = rfc822_header_raw_value(msg, hdr);
	struct bytestring next, rest;
	int lines = 0;
	size_t len = 0;

	if (!hdr->unfolded.ptr) {
		rest = raw;
		while (rest.ptr) {
			get_line(rest, &next, &rest);
			lines++;
			len += next.len;
		}

		if (lines <= 1) {
			hdr->unfolded = bytestring(raw.ptr, len);
		} else {
			char *unfold = talloc_array(msg, char, len);
			char *p = unfold;

			ALLOC_CHECK(unfold, bytestring_NULL);

			rest = raw;
			while (rest.ptr) {
				get_line(rest, &next, &rest);
				memcpy(p, next.ptr, next.len);
				p += next.len;
			}

			assert(p == (unfold + len));
			hdr->unfolded = bytestring(unfold, len);
		}
	}

	return hdr->unfolded;
}

/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_RFC822_H_
#define CCAN_RFC822_H_

#include <stdlib.h>

#include <ccan/bytestring/bytestring.h>

/* #define CCAN_RFC822_DEBUG 1 */

struct rfc822_header;
struct rfc822_msg;

/**
 * rfc822_set_allocation_failure_handler - set function to call on allocation
 *					   failure
 * @h: failure handler function pointer
 *
 * Normally functions in this module abort() on allocation failure for
 * simplicity.  You can change this behaviour by calling this function
 * to set an alternative callback for allocation failures.  The
 * callback is called with a string describing where the failure
 * occurred, which can be used to log a more useful error message.
 *
 * Note that tal also has a default function which calls abort() on allocation
 * failure: see tal_set_backend().
 *
 * Example:
 *	static void my_handler(const char *str)
 *	{
 *		fprintf(stderr, "Allocation failure: %s\n", str);
 *		exit(1);
 *	}
 *
 *	static void do_rfc822_stuff(void *buf, size_t len)
 *	{
 *		rfc822_set_allocation_failure_handler(&my_handler);
 *		rfc822_start(NULL, buf, len);
 *	}
 */
void rfc822_set_allocation_failure_handler(void (*h)(const char *));


static inline bool rfc822_iswsp(char c)
{
	return (c == ' ') || (c == '\t');
}

/**
 * rfc822_check - check validity of an rfc822_msg context
 * @msg: message to validate
 *
 * This debugging function checks the validity of the internal data
 * structures in an active rfc822_msg context.  If @abortstr is
 * non-NULL, that will be printed in a diagnostic if the state is
 * inconsistent, and the function will abort.  If the state of the
 * structure is valid it returns it unchanged.
 *
 * Returns the @msg if the message is consistent, NULL if not (it can
 * never return NULL if @abortstr is set).
 */
struct rfc822_msg *rfc822_check(const struct rfc822_msg *msg,
				const char *abortstr);

/**
 * rfc822_start - start parsing a new rfc822 message
 * @ctx: tal context to make allocations in (or talloc #ifdef TAL_USE_TALLOC)
 * @p: pointer to a buffer containing the message text
 * @len: length of the message text
 *
 * This function creates a new rfc822_msg context for parsing an
 * rfc822 message, initialized based on the message text given by the
 * pointer.
 */
struct rfc822_msg *rfc822_start(const void *ctx, const char *p, size_t len);

/**
 * rfc822_free - free an rfc822 message
 * @msg: message to free
 *
 * Frees an rfc822_msg context, including all subsiduary data
 * structures.
 */
void rfc822_free(struct rfc822_msg *msg);

/**
 * rfc822_first_header - retrieve the first header of an rfc822 message
 * @msg: message
 *
 * Finds the first header field of @msg and returns a struct
 * rfc822_header pointer representing it.
 */
#define rfc822_first_header(msg) (rfc822_next_header((msg), NULL))

/**
 * rfc822_next_header - retrieve the next header of an rfc822 message
 * @msg: message
 * @hdr: current header field
 *
 * Finds the header field of @msg which immediately follows @hdr and
 * returns a struct rfc822_header pointer for it.  If @hdr is NULL,
 * returns the first header in the message.
 */
struct rfc822_header *rfc822_next_header(struct rfc822_msg *msg,
					 struct rfc822_header *hdr);

#define rfc822_for_each_header(msg, hdr) \
	for ((hdr) = rfc822_first_header((msg)); \
	     (hdr);					\
	     (hdr) = rfc822_next_header((msg), (hdr)))

/**
 * rfc822_body - retrieve the body of an rfc822 message
 * @msg: message
 *
 * Finds the body of @msg and returns a bytestring containing its
 * contents.
 */
struct bytestring rfc822_body(struct rfc822_msg *msg);

enum rfc822_header_errors {
	RFC822_HDR_NO_COLON = 1,
	RFC822_HDR_BAD_NAME_CHARS = 2,
};

enum rfc822_header_errors rfc822_header_errors(struct rfc822_msg *msg,
					       struct rfc822_header *hdr);

/**
 * rfc822_header_raw_content - retrieve the raw content of an rfc822 header
 * @hdr: a header handle
 *
 * This returns a bytestring containing the complete contents (name
 * and value) of @hdr.  This will work even if the header is badly
 * formatted and cannot otherwise be parsed.
 */
struct bytestring rfc822_header_raw_content(struct rfc822_msg *msg,
					    struct rfc822_header *hdr);


/**
 * rfc822_header_raw_name - retrieve the name of an rfc822 header
 * @hdr: a header handle
 *
 * This returns a bytestring containing the header name of @hdr.  This
 * could include any invalid characters, in the case of a badly
 * formatted header.
 */
struct bytestring rfc822_header_raw_name(struct rfc822_msg *msg,
					 struct rfc822_header *hdr);

/**
 * rfc822_header_raw_value - retrieve the unprocessed value of an rfc822 header
 * @hdr: a header handle
 *
 * This returns a bytestring containing the complete contents of
 * @hdr's value.  This includes the terminating and any internal
 * (folded) newlines.
 */
struct bytestring rfc822_header_raw_value(struct rfc822_msg *msg,
					  struct rfc822_header *hdr);

/**
 * rfc822_header_unfolded_value - retrieve the unfolded value of an rfc822 header
 * @hdr: a header handle
 *
 * This returns a bytestring containing the unfolded contents of
 * @hdr's value.  That is, the header value with any internal and the
 * terminating newline removed.
 */
struct bytestring rfc822_header_unfolded_value(struct rfc822_msg *msg,
					       struct rfc822_header *hdr);

/**
 * rfc822_header_is - determine if a header is of a given name
 * @msg: message
 * @hdr: a header handle
 * @name: header name
 *
 * This returns true if the header field @hdr has name @name (case
 * insensitive), otherwise false.
 */
bool rfc822_header_is(struct rfc822_msg *msg, struct rfc822_header *hdr,
		      const char *name);

struct rfc822_header *rfc822_first_header_of_name(struct rfc822_msg *msg,
						  const char *name);
struct rfc822_header *rfc822_next_header_of_name(struct rfc822_msg *msg,
						 struct rfc822_header *hdr,
						 const char *name);

#endif /* CCAN_RFC822_H_ */

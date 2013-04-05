/* Licensed under LGPL - see LICENSE file for details */
#ifndef CCAN_TOK_ITR_H
#define CCAN_TOK_ITR_H

#include <string.h>
#include <stdbool.h>

/**
 * struct tok_itr - a structure containing the token iterator state
 * @delim: the token delimiter
 * @curr: a pointer the current token (may be empty)
 * @next: a pointer to the next token
 * @len: the length of the token at which @curr points.
 */
struct tok_itr {
	char delim;
	const char *curr;
	const char *next;
	size_t len;
};

/**
 * TOK_ITR_FOREACH - iterate through tokens in a string
 * @tok_itr_ptr: a pointer to a tok_itr struct
 * @str: the current token value for each iteration will be saved here.
 * @str_size: the maximum number of bytes writable to @str
 * @tokens: a string containing tokens separated by @delim
 * @delim: the token delimiter
 *
 * This wraps the most straightforward usage of this module in a
 * single macro. It's a for loop, so all the normal for loop
 * operations such as break and continue apply.
 *
 * If desired, possible truncation of tokens can be prevented by
 * using the module functions directly and calling tok_itr_val_len
 * in order to allocate an appropriately sized buffer for the call
 * to tok_itr_val.
 *
 * Example:
 *   char val[32];
 *   struct tok_itr itr;
 *
 *   TOK_ITR_FOREACH(&itr, val, 32, "/bin:/usr/bin:/sbin:/usr/local/bin", ':') {
 *     printf("token = %s", val);
 *   }
 */
#define TOK_ITR_FOREACH(tok_itr_ptr, str, str_size, tokens, delim) \
	for(tok_itr_init(tok_itr_ptr, tokens, delim); \
		!tok_itr_end(tok_itr_ptr) && (tok_itr_val(tok_itr_ptr, str, str_size) || 1); \
		tok_itr_next(tok_itr_ptr) \
		)

/**
 * tok_itr_init - initialize an iterator on a string of tokens separated
 * by delimiters.
 * @itr: a pointer to a tok_itr struct.
 * @tokens: a null terminated string of tokens.
 * @delim: the delimiter which separates tokens in @tokens
 *
 * Initializes (or resets) a tok_itr struct with a
 * particular token string and token delimiter.
 *
 * Example:
 * 	 struct tok_itr itr;
 *   tok_itr_init(&itr, "i=1&article=foo&side=bar", '&');
 */
void tok_itr_init(struct tok_itr *itr, const char *tokens, char delim);

/**
 * tok_itr_end - checks whether iterator has finished
 * @itr: a pointer to a tok_itr struct
 *
 * This function returns false if @itr is not at the end of the token
 * string, otherwise it returns true.
 */
static inline bool tok_itr_end(const struct tok_itr *itr) {
	return (itr->curr == NULL);
}

/**
 * tok_itr_next - increments the iterator to the next token
 * @itr: a tok_itr struct
 *
 * This function modifies @itr such that a call to
 * tok_itr_val() will return the value of the next
 * token (provided tok_itr_end() returns false).
 */
void tok_itr_next(struct tok_itr *itr);

/**
 * tok_itr_partial_val - was the last token terminated by a delimiter?
 * @itr: a pointer to a tok_itr struct
 *
 * In situations where one is iterating over tokens coming from
 * a file, it is possible that the last token in the token string
 * may be a partial token value, with the rest of it arriving in
 * the next chunk of bytes to be read from the file. In this
 * situation, this function can be used to differentiate between a
 * complete token and an incomplete one.
 *
 * This function returns true if the last token encountered was
 * non-empty and was terminated by the null terminator and not
 * a delimiter (thus, if there is more data, the last token may
 * be continued in the next chunk of bytes).
 *
 * The value of this function will remain valid even after
 * tok_itr_end has returned true.
 */
static inline bool tok_itr_partial_val(const struct tok_itr *itr) {
	return (itr->next == NULL && itr->len > 0);
}

/**
 * tok_itr_val_len - the length of the current token
 * @itr: a pointer to a tok_itr struct.
 *
 * This function returns the length of the current token
 * value in bytes. After tok_itr_end has returned true,
 * the value returned by this function will be the length
 * of the last token found in the token string (not including
 * a null terminator).
 */
static inline size_t tok_itr_val_len(const struct tok_itr *itr) {
	return itr->len;
}

/**
 * tok_itr_val - returns the current token value
 * @itr: a pointer to a tok_itr struct
 * @val: a pointer to a character buffer
 * @size: the size of @val in bytes
 *
 * This function copies at most @size characters (including null
 * terminating character) to the memory at @val. It returns the
 * length of the token. If the return value is greater than or
 * equal to @size then @val holds the truncated value of the token.
 *
 * When two delimiters appear subsequently with no data in between
 * then that is considered an empty token and @val will point to
 * an empty string.
 *
 * Calling this function on a tok_itr struct for which tok_itr_end()
 * has returned true (and which has not been subsequently
 * re-initialized) is undefined.
 *
 * Example:
 *   size_t len;
 *   char *token;
 *   struct tok_itr itr;
 *   char str[] = "/bin:/usr/bin:/usr/local/sbin";
 *
 *   for(tok_itr_init(&itr, str, ':'); !tok_itr_end(&itr); tok_itr_next(&itr) ) {
 *     len = (tok_itr_val_len(&itr) + 1);
 *     token = malloc(sizeof(char) * len);
 *     tok_itr_val(&itr, token, len);
 *     printf("token = %s\n", token);
 *     free(token);
 *   }
 */
size_t tok_itr_val(const struct tok_itr *itr, char *val, size_t size);

#endif /* CCAN_TOK_ITR_H */

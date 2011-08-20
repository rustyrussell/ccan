#include <ccan/bdelta/bdelta.h>
#include <ccan/bdelta/bdelta.c>
#include <ccan/tap/tap.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Finds a pseudorandom 32-bit number from 0 to 2^32-1 .
 * Uses the BCPL linear congruential generator method.
 *
 * Used instead of system RNG to ensure tests are consistent.
 */
static uint32_t rand32(void)
{
#if 0
	/*
	 * Tests should be run with a different random function
	 * from time to time.  I've found that the method below
	 * sometimes behaves poorly for testing purposes.
	 * For example, rand32() % N might only return even numbers.
	 */
	assert(RAND_MAX == 2147483647);
	return ((random() & 0xFFFF) << 16) | (random() & 0xFFFF);
#else
	static uint32_t rand32_state = 0;
	rand32_state *= (uint32_t)0x7FF8A3ED;
	rand32_state += (uint32_t)0x2AA01D31;
	return rand32_state;
#endif
}

#define RSTRING_OK                0  /* Operation succeeded */
#define RSTRING_MEMORY            1  /* Memory allocation failed */
#define RSTRING_ZERO_CARDINALITY  2  /* Cardinality of byte range is zero */
#define RSTRING_RANGE_OVERLAP     3  /* Byte range contains overlapping characters */

struct rstring_byte_range {
	unsigned int cardinality;
	unsigned int multiplier;
	unsigned int offset;
};

static int check_range(const struct rstring_byte_range *range)
{
	unsigned int i;
	unsigned int c;
	char set[256];
	
	if (range == NULL)
		return RSTRING_OK;
	
	if (range->cardinality == 0)
		return RSTRING_ZERO_CARDINALITY;
	
	memset(set, 0, sizeof(set));
	
	c = range->offset & 0xFF;
	for (i = 0; i < range->cardinality; i++) {
		if (set[c])
			return RSTRING_RANGE_OVERLAP;
		set[c] = 1;
		c += range->multiplier;
		c &= 0xFF;
	}
	
	return RSTRING_OK;
}

/*
 * Generate a pseudorandom string of the given length,
 * writing it into a user-supplied buffer.
 */
static uint8_t *random_string_into(uint8_t *str, uint32_t size, const struct rstring_byte_range *range)
{
	uint32_t i;
	uint32_t r;
	
	if (range == NULL) {
		for (i = 0; size - i >= 4; ) {
			r = rand32();
			str[i++] = r & 0xFF;
			r >>= 8;
			str[i++] = r & 0xFF;
			r >>= 8;
			str[i++] = r & 0xFF;
			r >>= 8;
			str[i++] = r & 0xFF;
		}
		
		if (i < size) {
			r = rand32();
			do {
				str[i++] = r & 0xFF;
				r >>= 8;
			} while (i < size);
		}
	} else {
		for (i = 0; i < size; )
			str[i++] = ((rand32() % range->cardinality) * range->multiplier + range->offset) & 0xFF;
	}
	
	return str;
}

/*
 * Generate a pseudorandom string of the given length.
 */
static uint8_t *random_string(uint32_t size, const struct rstring_byte_range *range)
{
	uint8_t *str;
	
	if (check_range(range) != RSTRING_OK) {
		fprintf(stderr, "Invalid byte range for random string generation\n");
		exit(EXIT_FAILURE);
	}
	
	str = malloc(size);
	if (str == NULL)
		return NULL;
	
	return random_string_into(str, size, range);
}

/*
 * Generate two byte strings:
 *
 *  An "old" string, of length @old_size
 *  A "new" string, differing from "old" by at most @diff_size bytes
 *  (where modifying a character is considered one change).
 *
 * Returns RSTRING_OK on success, RSTRING_MEMORY if memory allocation fails.
 */
static int random_string_pair(
	uint32_t old_size, uint32_t diff_size, const struct rstring_byte_range *range,
	uint8_t **old_out, uint8_t **new_out, uint32_t *new_size_out)
{
	uint8_t *old;
	uint8_t *new_;
	uint8_t *nptr;
	uint32_t new_size;
	uint32_t i;
	uint32_t j;
	
	/* insertions[i] is the number of characters to insert before position i. */
	uint32_t *insertions;
	
	/*
	 * changes[i] indicates what to do to the character at position i:
	 *
	 *  0: Leave it as-is.
	 *  1: Delete it.
	 *  2: Change it to another byte (possibly the same byte).
	 */
	char *changes;
	
	/* String of new bytes to use for insertions and changes. */
	uint8_t *new_bytes;
	uint8_t *new_bytes_ptr;
	uint32_t new_bytes_size;
	
	old = random_string(old_size, range);
	if (old == NULL)
		goto oom0;
	
	insertions = calloc(old_size + 1, sizeof(*insertions));
	if (insertions == NULL)
		goto oom1;
	
	changes = calloc(old_size, sizeof(*changes));
	if (changes == NULL)
		goto oom2;
	
	/*
	 * Generate a collection of edits, which will be
	 * applied to the old string "simultaneously".
	 */
	new_size = old_size;
	new_bytes_size = 0;
	for (i = 0; i < diff_size; i++) {
		uint32_t pos;
		
		switch (rand32() % 3) {
			case 0: /* Insert a byte. */
				pos = rand32() % (old_size + 1);
				insertions[pos]++;
				new_size++;
				new_bytes_size++;
				break;
			
			case 1: /* Delete a byte. */
				pos = rand32() % old_size;
				if (changes[pos]) {
					/*
					 * This character is already deleted or changed.
					 * Do something else instead.
					 *
					 * For the observative: i will overflow if it's 0.
					 * However, overflow of unsigned integers is well-defined
					 * by the C standard.  i will wrap around to UINT32_MAX,
					 * then the for-loop increment above will wrap it back to 0.
					 */
					i--;
				} else {
					changes[pos] = 1;
					new_size--;
				}
				break;
			
			default: /* Change a byte. */
				pos = rand32() % old_size;
				if (changes[pos]) {
					/*
					 * This character is already deleted or changed.
					 * Do something else instead.
					 */
					i--;
				} else {
					changes[pos] = 2;
					new_bytes_size++;
				}
				break;
		}
	}
	
	new_bytes = malloc(new_bytes_size);
	if (new_bytes == NULL)
		goto oom3;
	random_string_into(new_bytes, new_bytes_size, range);
	
	new_ = malloc(new_size);
	if (new_ == NULL)
		goto oom4;
	
	/* Apply the insertions and changes generated above. */
	nptr = new_;
	new_bytes_ptr = new_bytes;
	for (i = 0; i < old_size; i++) {
		for (j = 0; j < insertions[i]; j++)
			*nptr++ = *new_bytes_ptr++;
		
		switch (changes[i]) {
			case 0: /* No change */
				*nptr++ = old[i];
				break;
			
			case 1: /* Delete */
				break;
			
			default: /* Change value */
				*nptr++ = *new_bytes_ptr++;
				break;
		}
	}
	for (j = 0; j < insertions[i]; j++)
		*nptr++ = *new_bytes_ptr++;
	assert((size_t)(nptr - new_) == new_size);
	assert((size_t)(new_bytes_ptr - new_bytes) == new_bytes_size);
	
	free(new_bytes);
	free(changes);
	free(insertions);
	
	if (old_out)
		*old_out = old;
	else
		free(old);
	if (new_out)
		*new_out = new_;
	else
		free(new_);
	if (new_size_out)
		*new_size_out = new_size;
	
	return RSTRING_OK;
	
oom4:
	free(new_bytes);
oom3:
	free(changes);
oom2:
	free(insertions);
oom1:
	free(old);
oom0:
	if (old_out)
		*old_out = NULL;
	if (new_out)
		*new_out = NULL;
	if (new_size_out)
		*new_size_out = 0;
	return RSTRING_MEMORY;
}

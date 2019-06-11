/* Licensed under GPLv2+ - see LICENSE file for details */
#ifndef CCAN_CRC_H
#define CCAN_CRC_H
#include <stdint.h>
#include <stdlib.h>

/* Note: the crc32c function has been *fixed* (it gave wrong results!) and
 * moved to the ccan/crc32c module.  You probably want that if you're
 * just after a "good" crc function. */

/**
 * crc32_ieee - IEEE 802.3 32 bit crc of string of bytes
 * @start_crc: the initial crc (usually 0)
 * @buf: pointer to bytes
 * @size: length of buffer
 *
 * 32 bit CRC checksum using polynomial
 * X^32+X^26+X^23+X^22+X^16+X^12+X^11+X^10+X^8+X^7+X^5+X^4+X^2+X^1+X^0.
 *
 * See crc32c() for details.
 */
uint32_t crc32_ieee(uint32_t start_crc, const void *buf, size_t size);

/**
 * crc32_ieee_table - Get the IEEE 802.3 CRC table
 *
 * See crc32c_table() for details.
 */
const uint32_t *crc32_ieee_table(void);

/**
 * crc64_iso - ISO 3309
 * @start_crc: the initial crc (usually 0)
 * @buf: pointer to bytes
 * @size: length of buffer
 *
 * 64 bit CRC checksum using polynomial
 * X^64 + X^4 + X^3 + X^1 + X^0
 *
 * See crc32c() for details.
 */
uint64_t crc64_iso(uint64_t start_crc, const void *buf, size_t size);

/**
 * crc64_iso_table - Get the ISO 3309 CRC table
 *
 * See crc32c_table() for details.
 */
const uint64_t *crc64_iso_table(void);

#endif /* CCAN_CRC_H */

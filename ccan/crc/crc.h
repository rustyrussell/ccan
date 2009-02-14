#ifndef CCAN_CRC_H
#define CCAN_CRC_H
#include <stdint.h>
#include <stdlib.h>

/**
 * crc32 - 32 bit crc of string of bytes
 * @buf: pointer to bytes
 * @size: length of buffer
 *
 * 32 bit CRC checksum using polynomial
 * X^32+X^26+X^23+X^22+X^16+X^12+X^11+X^10+X^8+X^7+X^5+X^4+X^2+X^1+X^0.
 */
uint32_t crc32(const void *buf, size_t size);
#endif /* CCAN_CRC_H */

#include <ccan/endian/endian.h>

struct foo {
	char one[BSWAP_16_CONST(0xFF00)];
	char two[BSWAP_32_CONST(0xFF000000)];
	char three[BSWAP_64_CONST(0xFF00000000000000ULL)];
};

int main(void)
{
	return 0;
}

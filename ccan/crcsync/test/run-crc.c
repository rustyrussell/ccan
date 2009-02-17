#include "crcsync/crcsync.h"
#include "crcsync/crcsync.c"
#include "tap/tap.h"

int main(int argc, char *argv[])
{
	char buffer[1024];
	unsigned int i, j;
	uint32_t crcs[12] = { 0xFFFFF, 0xdeadf00d };

	plan_tests(3 + 8192);

	/* Simple test (we know currently crc of 0s is 0) */
	memset(buffer, 0, sizeof(buffer));
	crc_of_blocks(buffer, sizeof(buffer), sizeof(buffer), 32, crcs);
	ok1(crcs[0] == 0);
	crc_of_blocks(buffer, sizeof(buffer), sizeof(buffer)/2, 32, crcs);
	ok1(crcs[0] == 0);
	ok1(crcs[1] == 0);

	/* We know they're using crc32c. */
	for (i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i;
		crc_of_blocks(buffer, sizeof(buffer), sizeof(buffer)/7,
			      32, crcs);
		for (j = 0; j < sizeof(buffer); j += sizeof(buffer)/7) {
			unsigned int len = sizeof(buffer)/7;
			if (j + len > sizeof(buffer))
				len = sizeof(buffer) - j;
			
			ok1(crc32c(0, buffer + j, len) == crcs[j/(sizeof(buffer)/7)]);
		}
	}

	return exit_status();
}

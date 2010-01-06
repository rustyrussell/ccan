#include <ccan/hash/hash.h>
#include <ccan/tap/tap.h>
#include <stdbool.h>
#include <string.h>

#define ARRAY_WORDS 5

int main(int argc, char *argv[])
{
	unsigned int i;
	uint8_t u8array[ARRAY_WORDS];
	uint16_t u16array[ARRAY_WORDS];
	uint32_t u32array[ARRAY_WORDS];
	uint64_t u64array[ARRAY_WORDS];

	/* Initialize arrays. */
	for (i = 0; i < ARRAY_WORDS; i++) {
		u8array[i] = i;
		u16array[i] = i;
		u32array[i] = i;
		u64array[i] = i;
	}

	plan_tests(132);

	/* hash_stable is API-guaranteed. */
	ok1(hash_stable(u8array, ARRAY_WORDS, 0) == 0x1d4833cc);
	ok1(hash_stable(u8array, ARRAY_WORDS, 1) == 0x37125e2 );
	ok1(hash_stable(u8array, ARRAY_WORDS, 2) == 0x330a007a);
	ok1(hash_stable(u8array, ARRAY_WORDS, 4) == 0x7b0df29b);
	ok1(hash_stable(u8array, ARRAY_WORDS, 8) == 0xe7e5d741);
	ok1(hash_stable(u8array, ARRAY_WORDS, 16) == 0xaae57471);
	ok1(hash_stable(u8array, ARRAY_WORDS, 32) == 0xc55399e5);
	ok1(hash_stable(u8array, ARRAY_WORDS, 64) == 0x67f21f7 );
	ok1(hash_stable(u8array, ARRAY_WORDS, 128) == 0x1d795b71);
	ok1(hash_stable(u8array, ARRAY_WORDS, 256) == 0xeb961671);
	ok1(hash_stable(u8array, ARRAY_WORDS, 512) == 0xc2597247);
	ok1(hash_stable(u8array, ARRAY_WORDS, 1024) == 0x3f5c4d75);
	ok1(hash_stable(u8array, ARRAY_WORDS, 2048) == 0xe65cf4f9);
	ok1(hash_stable(u8array, ARRAY_WORDS, 4096) == 0xf2cd06cb);
	ok1(hash_stable(u8array, ARRAY_WORDS, 8192) == 0x443041e1);
	ok1(hash_stable(u8array, ARRAY_WORDS, 16384) == 0xdfc618f5);
	ok1(hash_stable(u8array, ARRAY_WORDS, 32768) == 0x5e3d5b97);
	ok1(hash_stable(u8array, ARRAY_WORDS, 65536) == 0xd5f64730);
	ok1(hash_stable(u8array, ARRAY_WORDS, 131072) == 0x372bbecc);
	ok1(hash_stable(u8array, ARRAY_WORDS, 262144) == 0x7c194c8d);
	ok1(hash_stable(u8array, ARRAY_WORDS, 524288) == 0x16cbb416);
	ok1(hash_stable(u8array, ARRAY_WORDS, 1048576) == 0x53e99222);
	ok1(hash_stable(u8array, ARRAY_WORDS, 2097152) == 0x6394554a);
	ok1(hash_stable(u8array, ARRAY_WORDS, 4194304) == 0xd83a506d);
	ok1(hash_stable(u8array, ARRAY_WORDS, 8388608) == 0x7619d9a4);
	ok1(hash_stable(u8array, ARRAY_WORDS, 16777216) == 0xfe98e5f6);
	ok1(hash_stable(u8array, ARRAY_WORDS, 33554432) == 0x6c262927);
	ok1(hash_stable(u8array, ARRAY_WORDS, 67108864) == 0x3f0106fd);
	ok1(hash_stable(u8array, ARRAY_WORDS, 134217728) == 0xc91e3a28);
	ok1(hash_stable(u8array, ARRAY_WORDS, 268435456) == 0x14229579);
	ok1(hash_stable(u8array, ARRAY_WORDS, 536870912) == 0x9dbefa76);
	ok1(hash_stable(u8array, ARRAY_WORDS, 1073741824) == 0xb05c0c78);
	ok1(hash_stable(u8array, ARRAY_WORDS, 2147483648U) == 0x88f24d81);

	ok1(hash_stable(u16array, ARRAY_WORDS, 0) == 0xecb5f507);
	ok1(hash_stable(u16array, ARRAY_WORDS, 1) == 0xadd666e6);
	ok1(hash_stable(u16array, ARRAY_WORDS, 2) == 0xea0f214c);
	ok1(hash_stable(u16array, ARRAY_WORDS, 4) == 0xae4051ba);
	ok1(hash_stable(u16array, ARRAY_WORDS, 8) == 0x6ed28026);
	ok1(hash_stable(u16array, ARRAY_WORDS, 16) == 0xa3917a19);
	ok1(hash_stable(u16array, ARRAY_WORDS, 32) == 0xf370f32b);
	ok1(hash_stable(u16array, ARRAY_WORDS, 64) == 0x807af460);
	ok1(hash_stable(u16array, ARRAY_WORDS, 128) == 0xb4c8cd83);
	ok1(hash_stable(u16array, ARRAY_WORDS, 256) == 0xa10cb5b0);
	ok1(hash_stable(u16array, ARRAY_WORDS, 512) == 0x8b7d7387);
	ok1(hash_stable(u16array, ARRAY_WORDS, 1024) == 0x9e49d1c );
	ok1(hash_stable(u16array, ARRAY_WORDS, 2048) == 0x288830d1);
	ok1(hash_stable(u16array, ARRAY_WORDS, 4096) == 0xbe078a43);
	ok1(hash_stable(u16array, ARRAY_WORDS, 8192) == 0xa16d5d88);
	ok1(hash_stable(u16array, ARRAY_WORDS, 16384) == 0x46839fcd);
	ok1(hash_stable(u16array, ARRAY_WORDS, 32768) == 0x9db9bd4f);
	ok1(hash_stable(u16array, ARRAY_WORDS, 65536) == 0xedff58f8);
	ok1(hash_stable(u16array, ARRAY_WORDS, 131072) == 0x95ecef18);
	ok1(hash_stable(u16array, ARRAY_WORDS, 262144) == 0x23c31b7d);
	ok1(hash_stable(u16array, ARRAY_WORDS, 524288) == 0x1d85c7d0);
	ok1(hash_stable(u16array, ARRAY_WORDS, 1048576) == 0x25218842);
	ok1(hash_stable(u16array, ARRAY_WORDS, 2097152) == 0x711d985c);
	ok1(hash_stable(u16array, ARRAY_WORDS, 4194304) == 0x85470eca);
	ok1(hash_stable(u16array, ARRAY_WORDS, 8388608) == 0x99ed4ceb);
	ok1(hash_stable(u16array, ARRAY_WORDS, 16777216) == 0x67b3710c);
	ok1(hash_stable(u16array, ARRAY_WORDS, 33554432) == 0x77f1ab35);
	ok1(hash_stable(u16array, ARRAY_WORDS, 67108864) == 0x81f688aa);
	ok1(hash_stable(u16array, ARRAY_WORDS, 134217728) == 0x27b56ca5);
	ok1(hash_stable(u16array, ARRAY_WORDS, 268435456) == 0xf21ba203);
	ok1(hash_stable(u16array, ARRAY_WORDS, 536870912) == 0xd48d1d1 );
	ok1(hash_stable(u16array, ARRAY_WORDS, 1073741824) == 0xa542b62d);
	ok1(hash_stable(u16array, ARRAY_WORDS, 2147483648U) == 0xa04c7058);

	ok1(hash_stable(u32array, ARRAY_WORDS, 0) == 0x13305f8c);
	ok1(hash_stable(u32array, ARRAY_WORDS, 1) == 0x171abf74);
	ok1(hash_stable(u32array, ARRAY_WORDS, 2) == 0x7646fcc7);
	ok1(hash_stable(u32array, ARRAY_WORDS, 4) == 0xa758ed5);
	ok1(hash_stable(u32array, ARRAY_WORDS, 8) == 0x2dedc2e4);
	ok1(hash_stable(u32array, ARRAY_WORDS, 16) == 0x28e2076b);
	ok1(hash_stable(u32array, ARRAY_WORDS, 32) == 0xb73091c5);
	ok1(hash_stable(u32array, ARRAY_WORDS, 64) == 0x87daf5db);
	ok1(hash_stable(u32array, ARRAY_WORDS, 128) == 0xa16dfe20);
	ok1(hash_stable(u32array, ARRAY_WORDS, 256) == 0x300c63c3);
	ok1(hash_stable(u32array, ARRAY_WORDS, 512) == 0x255c91fc);
	ok1(hash_stable(u32array, ARRAY_WORDS, 1024) == 0x6357b26);
	ok1(hash_stable(u32array, ARRAY_WORDS, 2048) == 0x4bc5f339);
	ok1(hash_stable(u32array, ARRAY_WORDS, 4096) == 0x1301617c);
	ok1(hash_stable(u32array, ARRAY_WORDS, 8192) == 0x506792c9);
	ok1(hash_stable(u32array, ARRAY_WORDS, 16384) == 0xcd596705);
	ok1(hash_stable(u32array, ARRAY_WORDS, 32768) == 0xa8713cac);
	ok1(hash_stable(u32array, ARRAY_WORDS, 65536) == 0x94d9794);
	ok1(hash_stable(u32array, ARRAY_WORDS, 131072) == 0xac753e8);
	ok1(hash_stable(u32array, ARRAY_WORDS, 262144) == 0xcd8bdd20);
	ok1(hash_stable(u32array, ARRAY_WORDS, 524288) == 0xd44faf80);
	ok1(hash_stable(u32array, ARRAY_WORDS, 1048576) == 0x2547ccbe);
	ok1(hash_stable(u32array, ARRAY_WORDS, 2097152) == 0xbab06dbc);
	ok1(hash_stable(u32array, ARRAY_WORDS, 4194304) == 0xaac0e882);
	ok1(hash_stable(u32array, ARRAY_WORDS, 8388608) == 0x443f48d0);
	ok1(hash_stable(u32array, ARRAY_WORDS, 16777216) == 0xdff49fcc);
	ok1(hash_stable(u32array, ARRAY_WORDS, 33554432) == 0x9ce0fd65);
	ok1(hash_stable(u32array, ARRAY_WORDS, 67108864) == 0x9ddb1def);
	ok1(hash_stable(u32array, ARRAY_WORDS, 134217728) == 0x86096f25);
	ok1(hash_stable(u32array, ARRAY_WORDS, 268435456) == 0xe713b7b5);
	ok1(hash_stable(u32array, ARRAY_WORDS, 536870912) == 0x5baeffc5);
	ok1(hash_stable(u32array, ARRAY_WORDS, 1073741824) == 0xde874f52);
	ok1(hash_stable(u32array, ARRAY_WORDS, 2147483648U) == 0xeca13b4e);

	ok1(hash_stable(u64array, ARRAY_WORDS, 0) == 0x12ef6302);
	ok1(hash_stable(u64array, ARRAY_WORDS, 1) == 0xe9aeb406);
	ok1(hash_stable(u64array, ARRAY_WORDS, 2) == 0xc4218ceb);
	ok1(hash_stable(u64array, ARRAY_WORDS, 4) == 0xb3d11412);
	ok1(hash_stable(u64array, ARRAY_WORDS, 8) == 0xdafbd654);
	ok1(hash_stable(u64array, ARRAY_WORDS, 16) == 0x9c336cba);
	ok1(hash_stable(u64array, ARRAY_WORDS, 32) == 0x65059721);
	ok1(hash_stable(u64array, ARRAY_WORDS, 64) == 0x95b5bbe6);
	ok1(hash_stable(u64array, ARRAY_WORDS, 128) == 0xe7596b84);
	ok1(hash_stable(u64array, ARRAY_WORDS, 256) == 0x503622a2);
	ok1(hash_stable(u64array, ARRAY_WORDS, 512) == 0xecdcc5ca);
	ok1(hash_stable(u64array, ARRAY_WORDS, 1024) == 0xc40d0513);
	ok1(hash_stable(u64array, ARRAY_WORDS, 2048) == 0xaab25e4d);
	ok1(hash_stable(u64array, ARRAY_WORDS, 4096) == 0xcc353fb9);
	ok1(hash_stable(u64array, ARRAY_WORDS, 8192) == 0x18e2319f);
	ok1(hash_stable(u64array, ARRAY_WORDS, 16384) == 0xfddaae8d);
	ok1(hash_stable(u64array, ARRAY_WORDS, 32768) == 0xef7976f2);
	ok1(hash_stable(u64array, ARRAY_WORDS, 65536) == 0x86359fc9);
	ok1(hash_stable(u64array, ARRAY_WORDS, 131072) == 0x8b5af385);
	ok1(hash_stable(u64array, ARRAY_WORDS, 262144) == 0x80d4ee31);
	ok1(hash_stable(u64array, ARRAY_WORDS, 524288) == 0x42f5f85b);
	ok1(hash_stable(u64array, ARRAY_WORDS, 1048576) == 0x9a6920e1);
	ok1(hash_stable(u64array, ARRAY_WORDS, 2097152) == 0x7b7c9850);
	ok1(hash_stable(u64array, ARRAY_WORDS, 4194304) == 0x69573e09);
	ok1(hash_stable(u64array, ARRAY_WORDS, 8388608) == 0xc942bc0e);
	ok1(hash_stable(u64array, ARRAY_WORDS, 16777216) == 0x7a89f0f1);
	ok1(hash_stable(u64array, ARRAY_WORDS, 33554432) == 0x2dd641ca);
	ok1(hash_stable(u64array, ARRAY_WORDS, 67108864) == 0x89bbd391);
	ok1(hash_stable(u64array, ARRAY_WORDS, 134217728) == 0xbcf88e31);
	ok1(hash_stable(u64array, ARRAY_WORDS, 268435456) == 0xfa7a3460);
	ok1(hash_stable(u64array, ARRAY_WORDS, 536870912) == 0x49a37be0);
	ok1(hash_stable(u64array, ARRAY_WORDS, 1073741824) == 0x1b346394);
	ok1(hash_stable(u64array, ARRAY_WORDS, 2147483648U) == 0x6c3a1592);

	return exit_status();
}

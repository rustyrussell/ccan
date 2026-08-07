#include <ccan/ciniparser/ciniparser.h>
#include <ccan/ciniparser/ciniparser.c>
#include <ccan/ciniparser/dictionary.h>
#include <ccan/ciniparser/dictionary.c>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <ccan/tap/tap.h>

#define NUM_TESTS 12

int main(int argc, char * argv[])
{
	dictionary *ini;
	char *ini_name;
	char *stmp, *stmp1, *stmp2, *stmp3;
	int itmp, itmp1, i;
	bool btmp;
	double dtmp;

	if (argc < 2)
		ini_name = "test/test.ini";
	else
		ini_name = argv[1] ;

	plan_tests(NUM_TESTS);

	ok(ini = ciniparser_load(ini_name),
		"ciniparser_load()      : loading %s", ini_name);
	ok(itmp = ciniparser_getnsec(ini),
		"ciniparser_getnsec()   : %d entries in dictionary %s",
			itmp, ini_name);
	ok(stmp = ciniparser_getsecname(ini, itmp),
		"ciniparser_getsecname(): last dict entry (%d) is %s", itmp, stmp);
	ok(stmp2 = ciniparser_getsecname(ini, 1),
		"ciniparser_getsecname(): first dict entry is %s", stmp2);
	ok(i = ciniparser_find_entry(ini, "Foo:shemp"),
		"ciniparser_find_entry(): checking if Foo:shemp exists (%s)",
			i ? "yes" : "no");
	ok(stmp1 = ciniparser_getstring(ini, "Wine:Grape", NULL),
		"ciniparser_getstring() : Wine:Grape = %s", stmp1);
	ok(!ciniparser_set(ini, "Wine:Grape", "Grape Ape"),
		"ciniparser_set()       : Wine:Grape is now Grape Ape");
	ok(stmp2 = ciniparser_getstring(ini, "Wine:Grape", NULL),
		"ciniparser_getstring() : Wine:Grape = %s", stmp2);
	ciniparser_unset(ini, "Wine:Grape");
	ok(! (stmp3 = ciniparser_getstring(ini, "Wine:Grape", NULL)),
		"ciniparser_unset()     : Wine:Grape should be unset if "
		"stmp3 is uninitialized (%s)",
			stmp3 == NULL ? "Yes" : "no");
	ok(itmp1 = ciniparser_getint(ini, "Pizza:Capres", 0),
		"ciniparser_getint()    : Pizza:Capres = %d", itmp1);
	ok(btmp = ciniparser_getboolean(ini, "Pizza:Mushrooms", 0),
		"ciniparser_getboolean(): Pizza:Capres = %s", btmp ? "true" : "false");
	ok(dtmp = ciniparser_getdouble(ini, "Wine:Alcohol", 0.00),
		"ciniparser_getdouble() : Wine:Alcohol = %-2.1f", dtmp);

	/* Just make sure we don't segfault here */

	ciniparser_dump(ini, stdout);
	ciniparser_dump_ini(ini, stdout);

	ciniparser_freedict(ini);

	return exit_status();
}

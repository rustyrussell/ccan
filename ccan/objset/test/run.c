#include <ccan/objset/objset.h>
#include <ccan/tap/tap.h>

struct objset_charp {
	OBJSET_MEMBERS(char *);
};

struct objset_int {
	OBJSET_MEMBERS(int *);
};

int main(void)
{
	struct objset_charp osetc;
	struct objset_int oseti;
	struct objset_iter i;
	int i1 = 1, i2 = 2;
	char c1 = 1, c2 = 2;

	/* This is how many tests you plan to run */
	plan_tests(46);

	objset_init(&osetc);
	objset_init(&oseti);
	ok1(objset_empty(&osetc));
	ok1(objset_empty(&oseti));
	ok1(objset_get(&oseti, &i1) == NULL);
	ok1(objset_get(&oseti, &i2) == NULL);
	ok1(objset_get(&osetc, &c1) == NULL);
	ok1(objset_get(&osetc, &c2) == NULL);

	ok1(!objset_del(&oseti, &i1));
	ok1(!objset_del(&oseti, &i2));
	ok1(!objset_del(&osetc, &c1));
	ok1(!objset_del(&osetc, &c2));

	objset_add(&oseti, &i1);
	ok1(!objset_empty(&oseti));
	ok1(objset_get(&oseti, &i1) == &i1);
	ok1(objset_get(&oseti, &i2) == NULL);

	objset_add(&osetc, &c1);
	ok1(!objset_empty(&osetc));
	ok1(objset_get(&osetc, &c1) == &c1);
	ok1(objset_get(&osetc, &c2) == NULL);

	objset_add(&oseti, &i2);
	ok1(!objset_empty(&oseti));
	ok1(objset_get(&oseti, &i1) == &i1);
	ok1(objset_get(&oseti, &i2) == &i2);

	objset_add(&osetc, &c2);
	ok1(!objset_empty(&osetc));
	ok1(objset_get(&osetc, &c1) == &c1);
	ok1(objset_get(&osetc, &c2) == &c2);

	ok1((objset_first(&oseti, &i) == &i1
	     && objset_next(&oseti, &i) == &i2)
	    || (objset_first(&oseti, &i) == &i2
		&& objset_next(&oseti, &i) == &i1));
	ok1(objset_next(&oseti, &i) == NULL);

	ok1((objset_first(&osetc, &i) == &c1
	     && objset_next(&osetc, &i) == &c2)
	    || (objset_first(&osetc, &i) == &c2
		&& objset_next(&osetc, &i) == &c1));
	ok1(objset_next(&osetc, &i) == NULL);

	ok1(objset_del(&oseti, &i1));
	ok1(!objset_del(&oseti, &i1));
	ok1(objset_del(&osetc, &c1));
	ok1(!objset_del(&osetc, &c1));

	ok1(objset_first(&oseti, &i) == &i2);
	ok1(objset_next(&oseti, &i) == NULL);
	ok1(objset_first(&osetc, &i) == &c2);
	ok1(objset_next(&osetc, &i) == NULL);

	objset_clear(&oseti);
	ok1(objset_first(&oseti, &i) == NULL);
	ok1(objset_empty(&oseti));
	ok1(objset_get(&oseti, &i1) == NULL);
	ok1(objset_get(&oseti, &i2) == NULL);
	ok1(!objset_del(&oseti, &i1));
	ok1(!objset_del(&oseti, &i2));

	objset_clear(&osetc);
	ok1(objset_first(&osetc, &i) == NULL);
	ok1(objset_empty(&osetc));
	ok1(objset_get(&osetc, &c1) == NULL);
	ok1(objset_get(&osetc, &c2) == NULL);
	ok1(!objset_del(&osetc, &c1));
	ok1(!objset_del(&osetc, &c2));

	/* This exits depending on whether all tests passed */
	return exit_status();
}

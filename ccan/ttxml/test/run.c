#include <ccan/ttxml/ttxml.h>
/* Include the C files directly. */

#define BUFFER 40	/* use a stupidly small buffer to stomp out bugs */

#include <ccan/ttxml/ttxml.c>
#include <ccan/tap/tap.h>


static int test_load(const char * filename)
{
	XmlNode *xml = xml_load(filename);
	if(!xml) return 0;

	xml_free(xml);
	return 1;
}

int main(void)
{
	XmlNode *x, *t;
	/* This is how many tests you plan to run */
	plan_tests(12);

	ok1(x = xml_load("./test/test.xml1"));
	ok1(!xml_find(x, "Doesn't Exist"));
	ok1(t = xml_find(x, "one"));
	ok1(xml_find(t, "two"));
	ok1(!xml_attr(t, "foobar"));
	ok1(!xml_attr(t, "Doesn't Exist"));
	ok1(xml_attr(t, "barfoo"));
	xml_free(x);
	/* Simple thing we expect to succeed */
	ok1(!test_load("does not exist")); /* A file that doesn't exist */
	ok1(test_load("./test/test.xml1")); /* A basic xml file. */
	ok1(test_load("./test/test.xml2")); /* Very small well-formed xml file. */
	ok1(test_load("./test/test.xml3")); /* Smallest well-formed xml file. */
	ok1(test_load("./test/test.xml4")); /* A single unclosed tag. */
	/* Same, with an explicit description of the test. */
//	ok(some_test(), "%s with no args should return 1", "some_test")
	/* How to print out messages for debugging. */
//	diag("Address of some_test is %p", &some_test)
	/* Conditional tests must be explicitly skipped. */

	/* This exits depending on whether all tests passed */
	return exit_status();
}

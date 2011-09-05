#include <ccan/ttxml/ttxml.h>
/* Include the C files directly. */
#include <ccan/ttxml/ttxml.c>
#include <ccan/tap/tap.h>

/* print out the heirarchy of an XML file, useful for debugging */

static void xp(XmlNode *x, int level, int max)
{
	int i;
	char text[] = "text";
	char *name = text;
	if(level > max)return;
	if(!x)return;
	if(x->name)name = x->name;
	for(i=0; i<level; i++)printf("    ");
	printf("%s:", name);
	if(x->name)
	for(i=0; i<x->nattrib; i++)
		printf("%s=\"%s\",", x->attrib[i*2], x->attrib[i*2+1]);
	else printf("%s", x->attrib[0]);
	printf("\n");
	if(x->child)xp(x->child, level+1, max);
	if(x->next)xp(x->next, level, max);
}



static int test_load(const char * filename)
{
	XmlNode *xml = xml_load(filename);
	if(!xml) return 0;

	xml_free(xml);
	return 1;
}

static int test_find(void)
{
	char *ctmp;
	XmlNode *xtmp, *xml = xml_load("./test/test.xml2");
	if(!xml)return 0;

	xp(xml, 0, 20);


	xtmp = xml_find(xml, "one");
	if(!xtmp)
	{
		printf("Failed to find node\n");	
		return 0;
	}
	printf("node is...\n");
	xp(xtmp, 0, 20);

	ctmp = xml_attr(xtmp, "barfoo");
	if(!ctmp)
	{
		printf("Failed to find attribute\n");
		return 0;
	}

	return 1;
}

int main(void)
{
	XmlNode *x, *t;
	/* This is how many tests you plan to run */
	plan_tests(13);

	ok1(x = xml_load("./test/test.xml2"));
	ok1(!xml_find(x, "Doesn't Exist"));
	ok1(t = xml_find(x, "one"));
	ok1(xml_find(t, "two"));
	ok1(!xml_attr(t, "foobar"));
	ok1(!xml_attr(t, "Doesn't Exist"));
	ok1(xml_attr(t, "barfoo"));
	xml_free(x);
	/* Simple thing we expect to succeed */
	ok1(!test_load("does not exist")); /* A file that doesn't exist */
	ok1(test_load("./test/test.xml")); /* A very large xml file. */
	ok1(test_load("./test/test.xml2")); /* A basic xml file. */
	ok1(test_load("./test/test.xml3")); /* Very small well-formed xml file. */
	ok1(test_load("./test/test.xml4")); /* Smallest well-formed xml file. */
	ok1(test_load("./test/test.xml5")); /* A single unclosed tag. */
	/* Same, with an explicit description of the test. */
//	ok(some_test(), "%s with no args should return 1", "some_test")
	/* How to print out messages for debugging. */
//	diag("Address of some_test is %p", &some_test)
	/* Conditional tests must be explicitly skipped. */

	/* This exits depending on whether all tests passed */
	return exit_status();
}

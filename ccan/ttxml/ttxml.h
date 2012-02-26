#ifndef CCAN_TTXML_H
#define CCAN_TTXML_H

/**
 * ttxml - tiny XML library for parsing (trusted!) XML documents.
 *
 * This parses an XML file into a convenient data structure.
 *
 * Example:
 * #include <ccan/ttxml/ttxml.h>
 * #include <stdio.h>
 *
 * int main(int argc, char *argv[])
 * {
 *	XmlNode *xml, *tmp;
 *
 *	xml = xml_load("./test/test.xml2");
 *	if(!xml)return 1;
 *
 *	tmp = xml_find(xml, "childnode");
 *
 *	printf("%s: %s\n", xml->name, xml_attr(tmp, "attribute"));
 *
 *	xml_free(xml);
 *
 *	return 0;
 * }
 *
 * Licensed under GPL - see LICENSE file for details.
 * Author: Daniel Burke <dan.p.burke@gmail.com>
 */

/* Every node is one of these */
typedef struct XmlNode {
	char * name;
	char ** attrib;
	int nattrib;
	struct XmlNode * child;
	struct XmlNode * next;
} XmlNode;

/* It's all pretty straight forward except for the attrib.
 *
 * Attrib is an array of char*, that is 2x the size of nattrib.
 * Each pair of char* points to the attribute name & the attribute value,
 * if present.
 *
 * If it's a text node, then name = "text", and attrib[1] = the body of text.
 * This is the only case where there will be an attribute with a null name.
 */

XmlNode* xml_load(const char * filename);
void xml_free(XmlNode *target);
char* xml_attr(XmlNode *x, const char *name);
XmlNode * xml_find(XmlNode *xml, const char *name);

#endif /* CCAN_TTXML_H */


typedef struct XmlNode {
	char * name;
	char ** attrib;
	int nattrib;
	struct XmlNode * child;
	struct XmlNode * next;
} XmlNode;


XmlNode* xml_new(char * name);
XmlNode* xml_load(const char * filename);
void xml_free(XmlNode *target);
char* xml_attr(XmlNode *x, const char *name);
XmlNode * xml_find(XmlNode *xml, const char *name);


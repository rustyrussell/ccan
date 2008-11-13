#ifndef _DOC_EXTRACT_CORE_H
#define _DOC_EXTRACT_CORE_H
#include <stdbool.h>
#include <ccan/list/list.h>

struct doc_section {
	struct list_node list;
	const char *function;
	const char *type;
	unsigned int num_lines;
	char **lines;
};

struct list_head *extract_doc_sections(char **rawlines, unsigned int num);
#endif /* _DOC_EXTRACT_CORE_H */

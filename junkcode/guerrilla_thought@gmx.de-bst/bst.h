#ifndef BST_H
#define BST_H

#define bst_declare(name, type, idx_type) struct  {	\
	struct {					\
		idx_type idx;				\
		type elem;				\
		void *left;				\
		void *right;				\
	} *head;					\
} name

#define bst_init(name) do {	\
	(name).head = NULL;	\
} while (0)

#define bst_node_type(name) typeof(*((name).head))

#define bst_insert(name, _elem, _idx) do {					\
	bst_node_type(name) **cur = NULL;					\
										\
	for (cur = &((name).head); ;) {						\
		assert(cur != NULL);						\
										\
		if (*cur == NULL) {						\
			(*cur) = malloc(sizeof(bst_node_type(name)));		\
			assert(*cur != NULL);					\
			(*cur)->idx = _idx;					\
			(*cur)->elem = _elem;					\
			(*cur)->left = NULL;					\
			(*cur)->right = NULL;					\
			break;							\
		} else {							\
			assert((*cur)->idx != _idx);				\
										\
			if (_idx < (*cur)->idx) {				\
				cur = (bst_node_type(name) **)&((*cur)->left);	\
			} else {						\
				cur = (bst_node_type(name) **)&((*cur)->right);	\
			}							\
		}								\
	}									\
} while (0)

#define bst_find(name, _idx, _elemp) do {				\
	bst_node_type(name) *cur = NULL;				\
									\
	assert((_elemp) != NULL);					\
									\
	for (cur = (name).head; cur != NULL; ) {			\
		if (cur->idx == _idx) {					\
			break;						\
		}							\
									\
		if (_idx < cur->idx) {				\
			cur = (bst_node_type(name) *)(cur->left);	\
		} else {						\
			cur = (bst_node_type(name) *)(cur->right);	\
		}							\
	}								\
									\
	if (cur != NULL) {						\
		*(_elemp) = ((cur)->elem);				\
	} else {							\
		*(_elemp) = NULL;					\
	}								\
} while (0);

#if 0



else {					\
				cur = &((bst_node_type(name)((*cur)->right)));				\
			}							\
			(*cur).elem = elem;		\
			(*cur).idx = idx;		\
		}					
#endif

#endif /* BST_H */

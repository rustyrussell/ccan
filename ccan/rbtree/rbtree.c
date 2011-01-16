/*
   a talloc based red-black tree

   Copyright (C) Ronnie Sahlberg  2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <ccan/rbtree/rbtree.h>

static void
tree_destructor_traverse_node(trbt_node_t *node)
{
	talloc_set_destructor(node, NULL);
	if (node->left) {
		tree_destructor_traverse_node(node->left);
	}
	if (node->right) {
		tree_destructor_traverse_node(node->right);
	}
	talloc_free(node);
}

/*
  destroy a tree and remove all its nodes
 */
static int tree_destructor(trbt_tree_t *tree)
{
	trbt_node_t *node;

	if (tree == NULL) {
		return 0;
	}

	node=tree->root;
	if (node == NULL) {
		return 0;
	}

	/* traverse the tree and remove the node destructor then delete it.
	   we don't want to use the existing destructor for the node
	   since that will remove the nodes one by one from the tree.
	   since the entire tree will be completely destroyed we don't care
	   if it is inconsistent or unbalanced while freeing the
	   individual nodes
	*/
	tree_destructor_traverse_node(node);

	return 0;
}


/* create a red black tree */
trbt_tree_t *
trbt_create(TALLOC_CTX *memctx, uint32_t flags)
{
	trbt_tree_t *tree;

	tree = talloc_zero(memctx, trbt_tree_t);
	if (tree == NULL) {
		fprintf(stderr, "Failed to allocate memory for rb tree\n");
		return NULL;
	}

	/* If the tree is freed, we must walk over all entries and steal the
	   node from the stored data pointer and release the node.
	   Note, when we free the tree  we only free the tree and not any of
	   the data stored in the tree.
	*/
	talloc_set_destructor(tree, tree_destructor);
	tree->flags = flags;

	return tree;
}

static inline trbt_node_t *
trbt_parent(trbt_node_t *node)
{
	return node->parent;
}

static inline trbt_node_t *
trbt_grandparent(trbt_node_t *node)
{
	trbt_node_t *parent;

	parent=trbt_parent(node);
	if(parent){
		return parent->parent;
	}
	return NULL;
}

static inline trbt_node_t *
trbt_uncle(trbt_node_t *node)
{
	trbt_node_t *parent, *grandparent;

	parent=trbt_parent(node);
	if(!parent){
		return NULL;
	}
	grandparent=trbt_parent(parent);
	if(!grandparent){
		return NULL;
	}
	if(parent==grandparent->left){
		return grandparent->right;
	}
	return grandparent->left;
}


static inline void trbt_insert_case1(trbt_tree_t *tree, trbt_node_t *node);
static inline void trbt_insert_case2(trbt_tree_t *tree, trbt_node_t *node);

static inline void
trbt_rotate_left(trbt_node_t *node)
{
	trbt_tree_t *tree = node->tree;

	if(node->parent){
		if(node->parent->left==node){
			node->parent->left=node->right;
		} else {
			node->parent->right=node->right;
		}
	} else {
		tree->root=node->right;
	}
	node->right->parent=node->parent;
	node->parent=node->right;
	node->right=node->right->left;
	if(node->right){
		node->right->parent=node;
	}
	node->parent->left=node;
}

static inline void
trbt_rotate_right(trbt_node_t *node)
{
	trbt_tree_t *tree = node->tree;

	if(node->parent){
		if(node->parent->left==node){
			node->parent->left=node->left;
		} else {
			node->parent->right=node->left;
		}
	} else {
		tree->root=node->left;
	}
	node->left->parent=node->parent;
	node->parent=node->left;
	node->left=node->left->right;
	if(node->left){
		node->left->parent=node;
	}
	node->parent->right=node;
}

/* NULL nodes are black by definition */
static inline int trbt_get_color(trbt_node_t *node)
{
	if (node==NULL) {
		return TRBT_BLACK;
	}
	return node->rb_color;
}
static inline int trbt_get_color_left(trbt_node_t *node)
{
	if (node==NULL) {
		return TRBT_BLACK;
	}
	if (node->left==NULL) {
		return TRBT_BLACK;
	}
	return node->left->rb_color;
}
static inline int trbt_get_color_right(trbt_node_t *node)
{
	if (node==NULL) {
		return TRBT_BLACK;
	}
	if (node->right==NULL) {
		return TRBT_BLACK;
	}
	return node->right->rb_color;
}
/* setting a NULL node to black is a nop */
static inline void trbt_set_color(trbt_node_t *node, int color)
{
	if ( (node==NULL) && (color==TRBT_BLACK) ) {
		return;
	}
	node->rb_color = color;
}
static inline void trbt_set_color_left(trbt_node_t *node, int color)
{
	if ( ((node==NULL)||(node->left==NULL)) && (color==TRBT_BLACK) ) {
		return;
	}
	node->left->rb_color = color;
}
static inline void trbt_set_color_right(trbt_node_t *node, int color)
{
	if ( ((node==NULL)||(node->right==NULL)) && (color==TRBT_BLACK) ) {
		return;
	}
	node->right->rb_color = color;
}

static inline void
trbt_insert_case5(trbt_node_t *node)
{
	trbt_node_t *grandparent;
	trbt_node_t *parent;

	parent=trbt_parent(node);
	grandparent=trbt_parent(parent);
	parent->rb_color=TRBT_BLACK;
	grandparent->rb_color=TRBT_RED;
	if( (node==parent->left) && (parent==grandparent->left) ){
		trbt_rotate_right(grandparent);
	} else {
		trbt_rotate_left(grandparent);
	}
}

static inline void
trbt_insert_case4(trbt_node_t *node)
{
	trbt_node_t *grandparent;
	trbt_node_t *parent;

	parent=trbt_parent(node);
	grandparent=trbt_parent(parent);
	if(!grandparent){
		return;
	}
	if( (node==parent->right) && (parent==grandparent->left) ){
		trbt_rotate_left(parent);
		node=node->left;
	} else if( (node==parent->left) && (parent==grandparent->right) ){
		trbt_rotate_right(parent);
		node=node->right;
	}
	trbt_insert_case5(node);
}

static inline void
trbt_insert_case3(trbt_tree_t *tree, trbt_node_t *node)
{
	trbt_node_t *grandparent;
	trbt_node_t *parent;
	trbt_node_t *uncle;

	uncle=trbt_uncle(node);
	if(uncle && (uncle->rb_color==TRBT_RED)){
		parent=trbt_parent(node);
		parent->rb_color=TRBT_BLACK;
		uncle->rb_color=TRBT_BLACK;
		grandparent=trbt_grandparent(node);
		grandparent->rb_color=TRBT_RED;
		trbt_insert_case1(tree, grandparent);
	} else {
		trbt_insert_case4(node);
	}
}

static inline void
trbt_insert_case2(trbt_tree_t *tree, trbt_node_t *node)
{
	trbt_node_t *parent;

	parent=trbt_parent(node);
	/* parent is always non-NULL here */
	if(parent->rb_color==TRBT_BLACK){
		return;
	}
	trbt_insert_case3(tree, node);
}

static inline void
trbt_insert_case1(trbt_tree_t *tree, trbt_node_t *node)
{
	trbt_node_t *parent;

	parent=trbt_parent(node);
	if(!parent){
		node->rb_color=TRBT_BLACK;
		return;
	}
	trbt_insert_case2(tree, node);
}

static inline trbt_node_t *
trbt_sibling(trbt_node_t *node)
{
	trbt_node_t *parent;

	parent=trbt_parent(node);
	if(!parent){
		return NULL;
	}

	if (node == parent->left) {
		return parent->right;
	} else {
		return parent->left;
	}
}

static inline void
trbt_delete_case6(trbt_node_t *node)
{
	trbt_node_t *sibling, *parent;

	sibling = trbt_sibling(node);
	parent  = trbt_parent(node);

	trbt_set_color(sibling, parent->rb_color);
	trbt_set_color(parent, TRBT_BLACK);
	if (node == parent->left) {
		trbt_set_color_right(sibling, TRBT_BLACK);
		trbt_rotate_left(parent);
	} else {
		trbt_set_color_left(sibling, TRBT_BLACK);
		trbt_rotate_right(parent);
	}
}


static inline void
trbt_delete_case5(trbt_node_t *node)
{
	trbt_node_t *parent, *sibling;

	parent = trbt_parent(node);
	sibling = trbt_sibling(node);
	if ( (node == parent->left)
	   &&(trbt_get_color(sibling)        == TRBT_BLACK)
	   &&(trbt_get_color_left(sibling)   == TRBT_RED)
	   &&(trbt_get_color_right(sibling)  == TRBT_BLACK) ){
		trbt_set_color(sibling, TRBT_RED);
		trbt_set_color_left(sibling, TRBT_BLACK);
		trbt_rotate_right(sibling);
		trbt_delete_case6(node);
		return;
	}
	if ( (node == parent->right)
	   &&(trbt_get_color(sibling)        == TRBT_BLACK)
	   &&(trbt_get_color_right(sibling)  == TRBT_RED)
	   &&(trbt_get_color_left(sibling)   == TRBT_BLACK) ){
		trbt_set_color(sibling, TRBT_RED);
		trbt_set_color_right(sibling, TRBT_BLACK);
		trbt_rotate_left(sibling);
		trbt_delete_case6(node);
		return;
	}

	trbt_delete_case6(node);
}

static inline void
trbt_delete_case4(trbt_node_t *node)
{
	trbt_node_t *sibling;

	sibling = trbt_sibling(node);
	if ( (trbt_get_color(node->parent)   == TRBT_RED)
	   &&(trbt_get_color(sibling)        == TRBT_BLACK)
	   &&(trbt_get_color_left(sibling)   == TRBT_BLACK)
	   &&(trbt_get_color_right(sibling)  == TRBT_BLACK) ){
		trbt_set_color(sibling, TRBT_RED);
		trbt_set_color(node->parent, TRBT_BLACK);
	} else {
		trbt_delete_case5(node);
	}
}

static void trbt_delete_case1(trbt_node_t *node);

static inline void
trbt_delete_case3(trbt_node_t *node)
{
	trbt_node_t *sibling;

	sibling = trbt_sibling(node);
	if ( (trbt_get_color(node->parent)   == TRBT_BLACK)
	   &&(trbt_get_color(sibling)        == TRBT_BLACK)
	   &&(trbt_get_color_left(sibling)   == TRBT_BLACK)
	   &&(trbt_get_color_right(sibling)  == TRBT_BLACK) ){
		trbt_set_color(sibling, TRBT_RED);
		trbt_delete_case1(node->parent);
	} else {
		trbt_delete_case4(node);
	}
}
	
static inline void
trbt_delete_case2(trbt_node_t *node)
{
	trbt_node_t *sibling;

	sibling = trbt_sibling(node);
	if (trbt_get_color(sibling) == TRBT_RED) {
		trbt_set_color(node->parent, TRBT_RED);
		trbt_set_color(sibling, TRBT_BLACK);
		if (node == node->parent->left) {
			trbt_rotate_left(node->parent);
		} else {
			trbt_rotate_right(node->parent);
		}
	}
	trbt_delete_case3(node);
}

static void
trbt_delete_case1(trbt_node_t *node)
{
	if (!node->parent) {
		return;
	} else {
		trbt_delete_case2(node);
	}
}

static void
delete_node(trbt_node_t *node)
{
	trbt_node_t *parent, *child, dc;
	trbt_node_t *temp = NULL;

	/* This node has two child nodes, then just copy the content
	   from the next smaller node with this node and delete the
	   predecessor instead.
	   The predecessor is guaranteed to have at most one child
	   node since its right arm must be NULL
	   (It must be NULL since we are its sucessor and we are above
	    it in the tree)
	 */
	if (node->left != NULL && node->right != NULL) {
		/* This node has two children, just copy the data */
		/* find the predecessor */
		temp = node->left;

		while (temp->right != NULL) {
			temp = temp->right;
		}

		/* swap the predecessor data and key with the node to
		   be deleted.
		 */
		node->key32 = temp->key32;
		node->data  = temp->data;
		/* now we let node hang off the new data */
		talloc_steal(node->data, node);
	
		temp->data  = NULL;
		temp->key32 = -1;
		/* then delete the temp node.
		   this node is guaranteed to have at least one leaf
		   child */
		delete_node(temp);
		goto finished;
	}


	/* There is at most one child to this node to be deleted */
	child = node->left;
	if (node->right) {
		child = node->right;
	}

	/* If the node to be deleted did not have any child at all we
	   create a temporary dummy node for the child and mark it black.
	   Once the delete of the node is finished, we remove this dummy
	   node, which is simple to do since it is guaranteed that it will
	   still not have any children after the delete operation.
	   This is because we don't represent the leaf-nodes as actual nodes
	   in this implementation.
	 */
	if (!child) {
		child = &dc;
		child->tree = node->tree;
		child->left=NULL;
		child->right=NULL;
		child->rb_color=TRBT_BLACK;
		child->data=NULL;
	}

	/* replace node with child */
	parent = trbt_parent(node);
	if (parent) {
		if (parent->left == node) {
			parent->left = child;
		} else {
			parent->right = child;
		}
	} else {
		node->tree->root = child;
	}
	child->parent = node->parent;


	if (node->rb_color == TRBT_BLACK) {
		if (trbt_get_color(child) == TRBT_RED) {
			child->rb_color = TRBT_BLACK;
		} else {
			trbt_delete_case1(child);
		}
	}

	/* If we had to create a temporary dummy node to represent a black
	   leaf child we now has to delete it.
	   This is simple since this dummy node originally had no children
	   and we are guaranteed that it will also not have any children
	   after the node has been deleted and any possible rotations
	   have occurred.

	   The only special case is if this was the last node of the tree
	   in which case we have to reset the root to NULL as well.
	   Othervise it is enough to just unlink the child from its new
	   parent.
	 */
	if (child == &dc) {
		if (child->parent == NULL) {
			node->tree->root = NULL;
		} else if (child == child->parent->left) {
			child->parent->left = NULL;
		} else {
			child->parent->right = NULL;
		}
	}

finished:
	/* if we came from a destructor and temp!=NULL  this means we
	   did the node-swap but now the tree still contains the old
	   node  which was freed in the destructor. Not good.
	*/
	if (temp) {
		temp->key32    = node->key32;
		temp->rb_color = node->rb_color;

		temp->data = node->data;
		talloc_steal(temp->data, temp);

		temp->parent = node->parent;
		if (temp->parent) {
			if (temp->parent->left == node) {
				temp->parent->left = temp;
			} else {
				temp->parent->right = temp;
			}
		}

		temp->left = node->left;
		if (temp->left) {
			temp->left->parent = temp;
		}
		temp->right = node->right;
		if (temp->right) {
			temp->right->parent = temp;
		}

		if (temp->tree->root == node) {
			temp->tree->root = temp;
		}
	}

	if ( (node->tree->flags & TRBT_AUTOFREE)
	&&   (node->tree->root == NULL) ) {
		talloc_free(node->tree);
	}

	return;
}

/*
  destroy a node and remove it from its tree
 */
static int node_destructor(trbt_node_t *node)
{
	delete_node(node);

	return 0;
}

static inline trbt_node_t *
trbt_create_node(trbt_tree_t *tree, trbt_node_t *parent, uint32_t key, void *data)
{
	trbt_node_t *node;

	node=talloc_zero(tree, trbt_node_t);
	if (node == NULL) {
		fprintf(stderr, "Failed to allocate memory for rb node\n");
		return NULL;
	}

	node->tree=tree;
	node->rb_color=TRBT_BLACK;
	node->parent=parent;
	node->left=NULL;
	node->right=NULL;
	node->key32=key;
	node->data = data;

	/* let this node hang off data so that it is removed when
	   data is freed
	 */
	talloc_steal(data, node);
	talloc_set_destructor(node, node_destructor);

	return node;
}

/* insert a new node in the tree.
   if there is already a node with a matching key in the tree
   we replace it with the new data and return a pointer to the old data
   in case the caller wants to take any special action
 */
void *
trbt_insert32(trbt_tree_t *tree, uint32_t key, void *data)
{
	trbt_node_t *node;

	node=tree->root;

	/* is this the first node ?*/
	if(!node){
		node = trbt_create_node(tree, NULL, key, data);

		tree->root=node;
		return NULL;
	}

	/* it was not the new root so walk the tree until we find where to
	 * insert this new leaf.
	 */
	while(1){
		/* this node already exists, replace data and return the
		   old data
		 */
		if(key==node->key32){
			void *old_data;

			old_data = node->data;
			node->data  = data;
			/* Let the node now be owned by the new data
			   so the node is freed when the enw data is released
			*/
			talloc_steal(node->data, node);

			return old_data;
		}
		if(key<node->key32) {
			if(!node->left){
				/* new node to the left */
				trbt_node_t *new_node;

				new_node = trbt_create_node(tree, node, key, data);
				if (!new_node)
					return NULL;
				node->left=new_node;
				node=new_node;

				break;
			}
			node=node->left;
			continue;
		}
		if(key>node->key32) {
			if(!node->right){
				/* new node to the right */
				trbt_node_t *new_node;

				new_node = trbt_create_node(tree, node, key, data);
				if (!new_node)
					return NULL;
				node->right=new_node;
				node=new_node;
				break;
			}
			node=node->right;
			continue;
		}
	}

	/* node will now point to the newly created node */
	node->rb_color=TRBT_RED;
	trbt_insert_case1(tree, node);
	return NULL;
}

void *
trbt_lookup32(trbt_tree_t *tree, uint32_t key)
{
	trbt_node_t *node;

	node=tree->root;

	while(node){
		if(key==node->key32){
			return node->data;
		}
		if(key<node->key32){
			node=node->left;
			continue;
		}
		if(key>node->key32){
			node=node->right;
			continue;
		}
	}
	return NULL;
}


/* This deletes a node from the tree.
   Note that this does not release the data that the node points to
*/
void
trbt_delete32(trbt_tree_t *tree, uint32_t key)
{
	trbt_node_t *node;

	node=tree->root;

	while(node){
		if(key==node->key32){
			talloc_free(node);
			return;
		}
		if(key<node->key32){
			node=node->left;
			continue;
		}
		if(key>node->key32){
			node=node->right;
			continue;
		}
	}
}


void
trbt_insert32_callback(trbt_tree_t *tree, uint32_t key, void *(*callback)(void *param, void *data), void *param)
{
	trbt_node_t *node;

	node=tree->root;

	/* is this the first node ?*/
	if(!node){
		node = trbt_create_node(tree, NULL, key,
				callback(param, NULL));

		tree->root=node;
		return;
	}

	/* it was not the new root so walk the tree until we find where to
	 * insert this new leaf.
	 */
	while(1){
		/* this node already exists, replace it
		 */
		if(key==node->key32){
			node->data  = callback(param, node->data);
			talloc_steal(node->data, node);

			return;
		}
		if(key<node->key32) {
			if(!node->left){
				/* new node to the left */
				trbt_node_t *new_node;

				new_node = trbt_create_node(tree, node, key,
						callback(param, NULL));
				node->left=new_node;
				node=new_node;

				break;
			}
			node=node->left;
			continue;
		}
		if(key>node->key32) {
			if(!node->right){
				/* new node to the right */
				trbt_node_t *new_node;

				new_node = trbt_create_node(tree, node, key,
						callback(param, NULL));
				node->right=new_node;
				node=new_node;
				break;
			}
			node=node->right;
			continue;
		}
	}

	/* node will now point to the newly created node */
	node->rb_color=TRBT_RED;
	trbt_insert_case1(tree, node);
	return;
}


struct trbt_array_param {
	void *(*callback)(void *param, void *data);
	void *param;
	uint32_t keylen;
	uint32_t *key;
	trbt_tree_t *tree;
};
static void *array_insert_callback(void *p, void *data)
{
	struct trbt_array_param *param = (struct trbt_array_param *)p;
	trbt_tree_t *tree = NULL;


	/* if keylen has reached 0 we are done and can call the users
	   callback function with the users parameters
	*/
	if (param->keylen == 0) {
		return param->callback(param->param, data);
	}


	/* keylen is not zero yes so we must create/process more subtrees */
	/* if data is NULL this means we did not yet have a subtree here
	   and we must create one.
	*/
	if (data == NULL) {
		/* create a new subtree and hang it off our current tree
		   set it to autofree so that the tree is freed when
		   the last node in it has been released.
		*/
		tree = trbt_create(param->tree, TRBT_AUTOFREE);
	} else {
		/* we already have a subtree for this path */
		tree = (trbt_tree_t *)data;
	}
		
	trbt_insertarray32_callback(tree, param->keylen, param->key, param->callback, param->param);

	/* now return either the old tree we got in *data or the new tree
	   we created to our caller so he can update his pointer in his
	   tree to point to our subtree
	*/
	return tree;
}



/* insert into the tree using an array of uint32 as a key */
void
trbt_insertarray32_callback(trbt_tree_t *tree, uint32_t keylen, uint32_t *key, void *(*cb)(void *param, void *data), void *pm)
{
	struct trbt_array_param tap;

	/* keylen-1 and key[1]  since the call to insert32 will consume the
	   first part of the key.
	*/
	tap.callback= cb;
	tap.param   = pm;
	tap.keylen  = keylen-1;
	tap.key     = &key[1];
	tap.tree    = tree;

	trbt_insert32_callback(tree, key[0], array_insert_callback, &tap);
}

/* lookup the tree using an array of uint32 as a key */
void *
trbt_lookuparray32(trbt_tree_t *tree, uint32_t keylen, uint32_t *key)
{
	/* if keylen is 1 we can do a regular lookup and return this to the
	   user
	*/
	if (keylen == 1) {
		return trbt_lookup32(tree, key[0]);
	}

	/* we need to lookup the next subtree */
	tree = trbt_lookup32(tree, key[0]);
	if (tree == NULL) {
		/* the key does not exist, return NULL */
		return NULL;
	}

	/* now lookup the next part of the key in our new tree */
	return trbt_lookuparray32(tree, keylen-1, &key[1]);
}


/* traverse a tree starting at node */
static void
trbt_traversearray32_node(trbt_node_t *node, uint32_t keylen,
	void (*callback)(void *param, void *data),
	void *param)
{
	if (node->left) {
		trbt_traversearray32_node(node->left, keylen, callback, param);
	}

	/* this is the smallest node in this subtree
	   if keylen is 0 this means we can just call the callback
	   otherwise we must pull the next subtree and traverse that one as well
	*/
	if (keylen == 0) {
		callback(param, node->data);
	} else {
		trbt_traversearray32(node->data, keylen, callback, param);
	}

	if (node->right) {
		trbt_traversearray32_node(node->right, keylen, callback, param);
	}
}
	

/* traverse the tree using an array of uint32 as a key */
void
trbt_traversearray32(trbt_tree_t *tree, uint32_t keylen,
	void (*callback)(void *param, void *data),
	void *param)
{
	trbt_node_t *node;

	if (tree == NULL) {
		return;
	}

	node=tree->root;
	if (node == NULL) {
		return;
	}

	trbt_traversearray32_node(node, keylen-1, callback, param);
}


/* this function will return the first node in a tree where
   the key is an array of uint32_t
*/
void *
trbt_findfirstarray32(trbt_tree_t *tree, uint32_t keylen)
{
	trbt_node_t *node;

	if (keylen < 1) {
		return NULL;
	}
	
	if (tree == NULL) {
		return NULL;
	}

	node=tree->root;
	if (node == NULL) {
		return NULL;
	}

	while (node->left) {
		node = node->left;
	}

	/* we found our node so return the data */
	if (keylen == 1) {
		return node->data;
	}

	/* we are still traversing subtrees so find the first node in the
	   next level of trees
	*/
	return trbt_findfirstarray32(node->data, keylen-1);
}



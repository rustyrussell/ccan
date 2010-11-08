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
#ifndef CCAN_RBTREE_H
#define CCAN_RBTREE_H
#include <stdint.h>
#include <ccan/talloc/talloc.h>

#define TRBT_RED		0x00
#define TRBT_BLACK		0x01
typedef struct trbt_node {
	struct trbt_tree *tree;
	struct trbt_node *parent;
	struct trbt_node *left;
	struct trbt_node *right;
	uint32_t rb_color;
	uint32_t key32;
	void *data;
} trbt_node_t;

typedef struct trbt_tree {
	trbt_node_t *root;
/* automatically free the tree when the last node has been deleted */
#define TRBT_AUTOFREE		0x00000001
	uint32_t flags;
} trbt_tree_t;



/* Create a RB tree */
trbt_tree_t *trbt_create(TALLOC_CTX *memctx, uint32_t flags);

/* Lookup a node in the tree and return a pointer to data or NULL */
void *trbt_lookup32(trbt_tree_t *tree, uint32_t key);

/* Insert a new node into the tree. If there was already a node with this
   key the pointer to the previous data is returned.
   The tree will talloc_steal() the data inserted into the tree .
*/
void *trbt_insert32(trbt_tree_t *tree, uint32_t key, void *data);

/* Insert a new node into the tree.
   If this is a new node:
     callback is called with data==NULL and param=param
     the returned value from the callback is talloc_stolen and inserted in the
     tree.
   If a node already exists for this key then:
     callback is called with data==existing data and param=param
     the returned calue is talloc_stolen and inserted in the tree
*/
void trbt_insert32_callback(trbt_tree_t *tree, uint32_t key, void *(*callback)(void *param, void *data), void *param);

/* Delete a node from the tree and free all data associated with it */
void trbt_delete32(trbt_tree_t *tree, uint32_t key);


/* insert into the tree with a key based on an array of uint32 */
void trbt_insertarray32_callback(trbt_tree_t *tree, uint32_t keylen, uint32_t *key, void *(*callback)(void *param, void *data), void *param);

/* Lookup a node in the tree with a key based on an array of uint32
   and return a pointer to data or NULL */
void *trbt_lookuparray32(trbt_tree_t *tree, uint32_t keylen, uint32_t *key);

/* Traverse a tree with a key based on an array of uint32 */
void trbt_traversearray32(trbt_tree_t *tree, uint32_t keylen, void (*callback)(void *param, void *data), void *param);

/* Lookup the first node in the tree with a key based on an array of uint32
   and return a pointer to data or NULL */
void *trbt_findfirstarray32(trbt_tree_t *tree, uint32_t keylen);

#endif /* CCAN_RBTREE_H */

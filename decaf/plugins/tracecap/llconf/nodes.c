/* -*- linux-c -*- */
/*
    This file is part of llconf2

    Copyright (C) 2004-2007  Oliver Kurth <oku@debian.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nodes.h"

/** @file nodes.c Low level functions to access a cnfnode tree.
 * Functions here provide a low level access to the cnfnode tree. Still, you can use
 * these for functionality not implemented on the higher level.
 * @sa entry.c
 */

/** creates an empty node.
 * a node will be allocated, and its name set. The allocated structure will be returned.
 * @param name the name of the node
 * @return the pointer to the new node
 */
struct cnfnode *create_cnfnode(const char *name)
{
	struct cnfnode *cn = NULL;

	cn = (struct cnfnode *)malloc(sizeof(struct cnfnode));
	if(cn){
		memset(cn, 0, sizeof(struct cnfnode));
    
		cn->name = strdup(name);
	}
	return cn;
}

/** copies a node.
 * a new node will be allocated, and the name and value of cn copied.
 * The node will not be linked into the tree.
 * @param cn the pointer to the node to be copied
 * @return the pointer to the copied node
 */
struct cnfnode *clone_cnfnode(const struct cnfnode *cn)
{
	struct cnfnode *new_cn = NULL;

	new_cn = (struct cnfnode *)malloc(sizeof(struct cnfnode));
	if(new_cn){
		memset(new_cn, 0, sizeof(struct cnfnode));
		new_cn->name = strdup(cn->name);
		if(cn->value)
			new_cn->value = (strdup(cn->value));
	}
	return new_cn;
}

/* the behviour of strcmp() is undefined if one of its arguments is
   NULL. This takes care of that.
   returns 0 if both arguments are NULL. If one arg is NULL, NULL is regarded as
   less than the other.
*/
static int _strcmp_null(const char *s1, const char *s2)
{
	if(s1 && s2)
		return(strcmp(s1, s2));
	else{
		if(s2) return -1;
		else if(s1) return 1;
		else return 0;
	}
}	

/** compare two nodes.
 * compares two nodes by name and then value. A NULL value is less than any string.
 * @param cn1 first node
 * @param cn2 second node
 * @return 0 if name and value match
 * @return -2 or 2 if the names do not match
 * @return -1 or 1 if the values do not match
 */
int compare_cnfnode(const struct cnfnode *cn1, const struct cnfnode *cn2)
{
	int ret;

	/* cn->name should never be NULL... */
	if((ret = strcmp(cn1->name, cn2->name)) != 0){
		return ret > 0 ? 2 : -2;
	}
	if((ret = _strcmp_null(cn1->value, cn2->value)) != 0){
		return ret > 0 ? 1 : -1;
	}
	return 0;
}

/** gets the value of a node.
 * @param cn pointer to a node
 */
inline
const char *cnfnode_getval(const struct cnfnode *cn)
{
	return cn->value;
}

/** gets the name of a node.
 * @param cn pointer to a node
 */
inline
const char *cnfnode_getname(const struct cnfnode *cn)
{
	return cn->name;
}

/** sets the value of a node.
 * if the node already has a value, that value will be free'ed.
 * @param cn pointer to a node
 * @param value pointer to the new value
 */
void cnfnode_setval(struct cnfnode *cn, const char *value)
{
	if(cn){
		if(cn->value) free(cn->value);
		if(value)
			cn->value = strdup(value);
		else
			cn->value = NULL;
	}
}

/** sets the name of a node.
 * if the node already has a name, that name will be free'ed.
 * @param cn pointer to a node
 * @param name pointer to the new name
 */
void cnfnode_setname(struct cnfnode *cn, const char *name)
{
	if(cn){
		if(cn->name) free(cn->name);
		if(name)
			cn->name = strdup(name);
		else
			cn->name = NULL;
	}
}

/** frees a node structure.
 * the memory of the node structure will be freed. Any memory used by the name or value will be freed as well.
 * @param cn pointer to a node
 */
void destroy_cnfnode(struct cnfnode *cn)
{
	if(cn){
		if(cn->name) free(cn->name);
		if(cn->value) free(cn->value);
		free(cn);
	}
}

/** frees a whole node tree.
 * recursively frees memory of the tree pointed to by cn, by recursively calling destroy_cnfnode()
 * @param cn_root pointer to the root of the tree.
 */
void destroy_cnftree(struct cnfnode *cn_root)
{
	struct cnfnode *cn = NULL, *cn_next;

	for(cn = cn_root->first_child; cn; cn = cn_next){
		cn_next = cn->next;
		destroy_cnftree(cn);
	}
	destroy_cnfnode(cn_root);
}

/** appends a node to the list of children.
 * @param cn_parent the parent node
 * @param cn pointer to the node to be appended
 */
void append_node(struct cnfnode *cn_parent, struct cnfnode *cn)
{
	struct cnfnode **cnp;

	for(cnp = &(cn_parent->first_child); *cnp; cnp = &((*cnp)->next));
	*cnp = cn;
	cn->parent = cn_parent;
	cn->next = NULL;
}

/** inserts a node to the list of children before another node.
 * @param cn_before pointer to the node before which the node shall be inserted.
 * @param cn pointer to the node to be inserted
 */
void insert_node_before(struct cnfnode *cn_before, struct cnfnode *cn)
{
	struct cnfnode **cnp;
	struct cnfnode *cn_parent = cn_before->parent;

	for(cnp = &(cn_parent->first_child); *cnp && *cnp != cn_before; cnp = &((*cnp)->next));
	*cnp = cn;
	cn->parent = cn_parent;
	cn->next = cn_before;
}

/** unlink a node from its list.
 * removes a node from its list. The memory will not be freed, you have to call
 * destroy_cnfnode() or destroy_cnftree() separately.
 * @param cn pointer to the node to be unlinked.
 */
void unlink_node(struct cnfnode *cn)
{
	struct cnfnode **pcn;

	for(pcn = &(cn->parent->first_child); *pcn && *pcn != cn; pcn = &((*pcn)->next));
	if(*pcn)
		*pcn = cn->next;
}

/** search for a node.
 * searches in the list beginning with cn_list. This function does not search
 * recursively in the children's lists.
 * @param cn_list the first node to be searched
 * @param name the name to search for
 * @return the pointer to the matching node
 */
struct cnfnode *find_node(struct cnfnode *cn_list, const char *name)
{
	struct cnfnode *cn;

	for(cn = cn_list; cn; cn = cn->next){
		if(strcmp(cn->name, name) == 0)
			return cn;
	}
	return NULL;
}

/** copy a whole tree.
 * recursively copies a whole tree by calling clone_cnfnode()
 * @param cn_root pointer to the root of the tree.
 * @return the pointer to the root of the copied tree
 */
struct cnfnode *clone_cnftree(const struct cnfnode *cn_root)
{
	struct cnfnode *cn_root_new, *cn_new;
	const struct cnfnode *cn;

	cn_root_new = clone_cnfnode(cn_root);

	for(cn = cn_root->first_child; cn; cn = cn->next){
		cn_new = clone_cnftree(cn);
		append_node(cn_root_new, cn_new);
	}
	return cn_root_new;
}

/** compare two trees.
 * Recursively compare two trees if they are the same, by comparing the names, values and subtrees.
 * The return value is pretty much useless, except for the fact that it indicates
 * if the trees match or not, because this function returns as soon as it finds
 * some difference.
 * @param cn_root1 the first tree
 * @param cn_root2 the second tree
 * @return 0 if names and values and children match
 * @return -3/3 if cn_root1 has less/more descendants than cn_root2
 * @return the result of compare_cnfnode() if names or values do not match
 * @sa compare_cnfnode
 */
int compare_cnftree(const struct cnfnode *cn_root1, const struct cnfnode *cn_root2)
{
	const struct cnfnode *cn1, *cn2;
	int ret;

	if(cn_root1 && cn_root2){
		if((ret = compare_cnfnode(cn_root1, cn_root2)) != 0)
			return ret;
	}else{
		if((cn_root1 == NULL) && (cn_root2 == NULL))
			return 0;
		else{
			if(cn_root1 == NULL)
				return -3;
			else
				return 3;
		}
	}
	for(cn1 = cn_root1->first_child, cn2 = cn_root2->first_child; cn1 || cn2; cn1 = cn1->next, cn2 = cn2->next){
		if((ret = compare_cnftree(cn1, cn2)) != 0)
			return ret;
	}

	return 0;
}

/** compare two trees.
 * Recursively compare two trees if they are the same, by comparing the names, values and subtrees,
 * except the names and values of the two rot nodes.
 * The return value is pretty much useless, except for the fact that it indicates
 * if the trees match or not, because this function returns as soon as it finds
 * some difference.
 * @param cn_root1 the first tree
 * @param cn_root2 the second tree
 * @return 0 if names and values and children match
 * @return -3/3 if cn_root1 has less/more descendants than cn_root2
 * @return the result of compare_cnfnode() if names or values do not match
 * @sa compare_cnfnode
 */
int compare_cnftree_children(const struct cnfnode *cn_root1, const struct cnfnode *cn_root2)
{
	const struct cnfnode *cn1, *cn2;
	int ret;

	if(!(cn_root1 && cn_root2)){
		if((cn_root1 == NULL) && (cn_root2 == NULL))
			return 0;
		else{
			if(cn_root1 == NULL)
				return -3;
			else
				return 3;
		}
	}
	for(cn1 = cn_root1->first_child, cn2 = cn_root2->first_child; cn1 || cn2; cn1 = cn1->next, cn2 = cn2->next){
		if((ret = compare_cnftree(cn1, cn2)) != 0)
			return ret;
	}

	return 0;
}

struct cnfnode *cnfnode_walk_step(struct cnfnode *cn)
{
	if(cn->first_child)
		return cn->first_child;
	else if(cn->next)
		return cn->next;
	else if(cn->parent){
		while(cn->parent && cn->next == NULL)
			cn = cn->parent;
		return cn->next;
	}
	return NULL;
}

char *cnfnode_path(struct cnfnode *cn)
{
	struct cnfnode *cn_parent;
	int size = 0;
	char *path, *q0;

	for(cn_parent = cn; cn_parent; cn_parent = cn_parent->parent){
		size += strlen(cnfnode_getname(cn_parent)) + 1;
	}

	path = malloc(size);
	q0 = path + size - 1;
	*q0 = 0;

	for(cn_parent = cn; cn_parent; cn_parent = cn_parent->parent){
		char *p, *q;

		q0 -= strlen(cn_parent->name);
		q = q0;
		if(cn_parent->parent){
			q0--;
			*q0 = '/';
		}
		p = cn_parent->name;
		while(*p) *q++ = *p++;
	}
	return path;
}


void dump_nodes(struct cnfnode *cn_root, int level)
{
	int i;
	struct cnfnode *cn;

	for(i = 0; i < level; i++)
		putchar('\t');
	printf("%s", cn_root->name);
	if(cn_root->value)
		printf(" = '%s'", cn_root->value);
	putchar('\n');

	for(cn = cn_root->first_child; cn; cn = cn->next)
		dump_nodes(cn, level+1);
}


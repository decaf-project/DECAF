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

#ifndef __LLCONF_NODES_H_INCLUDED
#define __LLCONF_NODES_H_INCLUDED

/** A node in a parsed configuration tree. */
struct cnfnode{
	struct cnfnode *next; /**< The next sibling of this node. */
	char *name; /**< The name of this node; may be NULL. */
	char *value; /**< The value at this leaf node; usually NULL for non-leaf nodes, but not necessarily. */
	struct cnfnode *first_child; /**< The first child underneath this node. */
	struct cnfnode *parent; /**< The parent node of this node. */
};

#ifdef __cplusplus
extern "C" {
#endif

struct cnfnode *create_cnfnode(const char *name);

struct cnfnode *clone_cnfnode(const struct cnfnode *cn);
struct cnfnode *clone_cnftree(const struct cnfnode *cn_root);

const char *cnfnode_getval(const struct cnfnode *cn);

const char *cnfnode_getname(const struct cnfnode *cn);

void cnfnode_setval(struct cnfnode *cn, const char *value);
void cnfnode_setname(struct cnfnode *cn, const char *name);

/* free memory of node, leaving children intact */
void destroy_cnfnode(struct cnfnode *cn);

/* free recursively all children, then cn */
void destroy_cnftree(struct cnfnode *cn);

/* append a node */
void append_node(struct cnfnode *cn_parent, struct cnfnode *cn);

/* insert a node */
void insert_node_before(struct cnfnode *cn_before, struct cnfnode *cn);

/* remove a node from tree list (but do not destroy it) */
void unlink_node(struct cnfnode *cn);

/* walks through list starting with cnf_list,
   and returns 1st node with matching name */
struct cnfnode *find_node(struct cnfnode *cn_list, const char *name);

int compare_cnfnode(const struct cnfnode *cn1, const struct cnfnode *cn2);
int compare_cnftree(const struct cnfnode *cn_root1, const struct cnfnode *cn_root2);
int compare_cnftree_children(const struct cnfnode *cn_root1, const struct cnfnode *cn_root2);

void dump_nodes(struct cnfnode *cn_root, int level);

#ifdef __cplusplus
}
#endif

#endif // __LLCONF_NODES_H_INCLUDED

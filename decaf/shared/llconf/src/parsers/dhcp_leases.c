/* -*- linux-c -*- */
/*
    This file is part of llconf2

    Copyright (C) 2005  Nigel Cunningham <nigel.cunningham@cyclades.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "strutils.h"
#include "nodes.h"
#include "lines.h"
#include "modules.h"

#include "dhcp_leases.h"

static
struct cnfmodule this_module = {
	name: "dhcp_leases",
	parser: parse_dhcp_leases,
	unparser: unparse_dhcp_leases
};

struct cnfnode *current_head = NULL;

struct simple_nodes_data {
	char *name;
	int next_word_only;
};

static struct simple_nodes_data node_data[] = {
	{ "hardware ethernet", 1 },
	{ "client-hostname", 1 },
	{ "binding state", 1 },
	{ "next binding state", 1 },
	{ "uid", 1 },
	{ "starts", 0 },
	{ "ends", 0 },
	{ "tstp", 0 },
	{ "", 0 },
};

#define node_data_entries (sizeof(node_data) / sizeof(struct simple_nodes_data))

static const char *end_of_word(const char *start_of_word)
{
	const char *posn = start_of_word;

	while (*posn && !isspace(*posn))
		posn++;

	return posn;
}

static void get_next_word_to_buf(char *buf, const char *start_of_word)
{
	int word_len = end_of_word(start_of_word) - start_of_word;

	strncpy(buf, start_of_word, word_len);
	buf[word_len] = 0;
}

static
void _parse_dhcp_leases(struct cnfnode *cn_root, struct confline *cl)
{
	char buf[1024];
	const char *p;
	struct cnfnode *node = NULL;
	int i;

	/* Where are we appending nodes? */
	if (!current_head)
		current_head = cn_root;

	p = cl->line;

	while(*p && isspace(*p)) p++;

	if (!strncmp(p, "lease", 5)) {

		/*
		 * A lease file may contain the same IP address more than once.
		 * We are only interested in the last one.
		 */
		if (current_head != cn_root)
			printf("Malformed leases file - lease without closing '}'");

		get_next_word_to_buf(buf, p + 6);
		if (cn_root->first_child)
			node = find_node(cn_root->first_child, buf);	
		if (node) {
			unlink_node(node);
			destroy_cnftree(node);
		}
		node = create_cnfnode(buf);
		append_node(cn_root, node);
		current_head = node;

	} else if (!strncmp(p, "}", 1)) {
		/* End of node */

		current_head = cn_root;
	} else {
		if(*p == '#'){
			dup_next_line_b(&p, buf, sizeof(buf));
			node = create_cnfnode(".comment");
			cnfnode_setval(node, buf);
			append_node(current_head, node);
			return;
		} else if (!*p) {
			dup_next_line_b(&p, buf, sizeof(buf));
			node = create_cnfnode(".empty");
			cnfnode_setval(node, buf);
			append_node(current_head, node);
			return;
		}

		if (current_head == cn_root) {
			dup_next_line_b(&p, buf, sizeof(buf));
			node = create_cnfnode(".syntax-error");
			cnfnode_setval(node, buf);
			append_node(current_head, node);
			return;
		}

		for (i = 0; i < node_data_entries; i++) {
			if (!strncmp(p, node_data[i].name, strlen(node_data[i].name)) ||
				i == (node_data_entries - 1)) {
				const char *val_start = p + strlen(node_data[i].name) + 1,
				      *val_end = end_of_word(val_start);
		
				if (node_data[i].next_word_only) {
					strncpy(buf, val_start, val_end - val_start + 1);
					buf[val_end - val_start - 1] = 0;
				} else {
					strcpy(buf, val_start);
					buf[strlen(buf) - 2] = 0;
				}

				if (i == (node_data_entries - 1))
					node = create_cnfnode(".unparsed");
				else
					node = create_cnfnode(node_data[i].name);
				cnfnode_setval(node, buf);
				append_node(current_head, node);
				return;
			}
		}
	}
}

int unparse_dhcp_leases(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	fprintf(fptr, "(not yet implemented)\n");
	return 0;
}

struct cnfnode *parse_dhcp_leases(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);
	
	cn_root = create_cnfnode("(root)");

	current_head = NULL;
	
	for(cl = cl_root; cl; cl= cl->next){
		_parse_dhcp_leases(cn_root, cl);
	}

	destroy_confline_list(cl_root);
	return cn_root;
}

void register_dhcp_leases(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

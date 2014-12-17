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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "strutils.h"
#include "nodes.h"
#include "lines.h"
#include "modules.h"
#include "pair.h"

/** @file pair.c 
 * Parser module for configuration in the style:
 * "variable value"
 * This file also contains a few functions that can be used for other parsers.
 */

static
struct cnfmodule this_module = {
	name: "pair",
	default_file: NULL,
	parser: parse_pair,
	unparser: unparse_pair
};

struct cnfnode *parse_pair_line_sep(const char **pp, int sep)
{
	struct cnfnode *cn_top = NULL;
	const char *p = *pp;

	while(*p && isspace(*p)) p++;
	
	if(*p){
		char buf[256];

		if(sep == -1){
			cn_top = create_cnfnode(dup_next_word_b(&p, buf, sizeof(buf)-1));
			while(*p && isspace(*p)) p++;
		}else{
			cn_top = create_cnfnode(dup_line_until_b(&p, sep, buf, sizeof(buf)-1));
			if(*p == sep) p++;
			while(*p && isspace(*p)) p++;
		}

		dup_line_until_b(&p, '#', buf, 255);
		
		if(*p){
			cnfnode_setval(cn_top, buf);
		}
	
		while(*p && isspace(*p)) p++;
	
		if(*p == '#'){
			struct cnfnode *cn_cmt;

			cn_cmt = create_cnfnode(".comment");
			cnfnode_setval(cn_cmt, dup_next_line_b(&p, buf, 255));
			append_node(cn_top, cn_cmt);
		}
	}

	*pp = p;
	return cn_top;
}

/** parses a line of the style
 * "variable value"
 * and returns a node with name set to variable and value set to value. If the line
 * has a comment at the end (marked by a #), that comment will be added as a child
 * to the node.
 * This function can be used by other parsers.
 * @param pp pointer to the buffer, will point to the end of the line on return.
 * @return the new node
 */

struct cnfnode *parse_pair_line(const char **pp)
{
	return parse_pair_line_sep(pp, -1);
}

static
int parse_pair_options(struct cnfnode *opt_root, int *psep)
{
	struct cnfnode *cn;
	int i = 0;

	if(opt_root == NULL) return -1;
	if(opt_root->first_child == NULL) return -1;

	for(cn = opt_root->first_child; cn; cn = cn->next){
		if(strcmp(cn->name, "sep") == 0){
			*psep = (unsigned char)(cn->value[0]);
		}
	}

	return i;
}

struct cnfnode *parse_pair(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn = NULL;
	struct confline *cl, *cl_root;
	int sep = -1;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	if(cm && cm->opt_root){
		parse_pair_options(cm->opt_root, &sep);
	}

	for(cl = cl_root; cl; cl = cl->next){
		char buf[256];
		const char *p = cl->line;

		while(*p && isspace(*p)) p++;
		if(*p){
			if(*p != '#'){
				cn = parse_pair_line_sep(&p, sep);
				if(cn)
					append_node(cn_root, cn);

			}else if(*p == '#'){
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
				append_node(cn_root, cn);
			}else{
				cn = create_cnfnode(".unparsed");
				cnfnode_setval(cn, cl->line);
				append_node(cn_root, cn);
			}
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
		}
      
	}
	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_pair_line_sep(FILE *fptr, struct cnfnode *cn_top, int sep)
{
	if(cn_top->value){
		if(sep == -1)
			fprintf(fptr, "%s %s", cn_top->name, cn_top->value);
		else
			fprintf(fptr, "%s%c%s", cn_top->name, sep, cn_top->value);
	}else
		fprintf(fptr, "%s", cn_top->name);

	if(cn_top->first_child){
		struct cnfnode *cn_cmt = cn_top->first_child;
		if(cn_cmt->value)
			fprintf(fptr, "%s", cn_cmt->value);
	}
	fprintf(fptr, "\n");
	
	return 0;
}

int unparse_pair_line(FILE *fptr, struct cnfnode *cn_top)
{
	return unparse_pair_line_sep(fptr, cn_top, -1);
}

int unparse_pair(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;
	int sep = -1;

	if(cm && cm->opt_root){
		parse_pair_options(cm->opt_root, &sep);
	}

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s\n", cn_top->value);
		}else{
			unparse_pair_line_sep(fptr, cn_top, sep);
		}
	}

	return 0;
}

void register_pair(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

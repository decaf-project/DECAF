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
#include "table.h"

static
struct cnfmodule this_module = {
	name: "table",
	default_file: NULL,
	parser: parse_table,
	unparser: unparse_table
};


static
int parse_table_options(struct cnfnode *opt_root, char **cols, int n, int *psep)
{
	struct cnfnode *cn;
	const char *p;
	char *q, buf[256];
	int i = 0;


	if(opt_root == NULL) return -1;
	if(opt_root->first_child == NULL) return -1;

	for(cn = opt_root->first_child; cn; cn = cn->next){
		if(strcmp(cn->name, "cols") == 0){
			p = cn->value;
			while(*p && i < 31){
				q = buf;
				while(*p && *p != ':' && q < buf+255) *q++ = *p++;
				*q = 0;
				cols[i++] = strdup(buf);
				if(*p) p++;
			}
			cols[i] = NULL;
		}else if(strcmp(cn->name, "sep") == 0){
			*psep = (unsigned char)(cn->value[0]);
		}
	}

	return i;
}

struct cnfnode *parse_table_line(const char **pp, char **cols, int ncols, int sep)
{
	struct cnfnode *cn_top, *cn;
	int i = 0;
	const char *p = *pp;
	char buf[strlen(p)+1];

	if(sep == -1){
		cn_top = create_cnfnode(dup_next_word_b(&p, buf, sizeof(buf)-1));
		while(*p && isspace(*p)) p++;
	}else{
		cn_top = create_cnfnode(dup_line_until_b(&p, sep, buf, sizeof(buf)-1));
		if(*p) p++;
	}
	
	i = 1;
	while(*p && i < ncols-1){
		cn = create_cnfnode(cols[i++]);
		if(sep == -1){
			cnfnode_setval(cn, dup_next_word_b(&p, buf, sizeof(buf)-1));
			while(*p && isspace(*p)) p++;
		}else{
			cnfnode_setval(cn, dup_line_until_b(&p, sep, buf, sizeof(buf)-1));
			if(*p) p++;
		}
		append_node(cn_top, cn);
	}
	
	while(*p && i < ncols-1){
		cn = create_cnfnode(cols[i++]);
		if(sep == -1){
			cnfnode_setval(cn, dup_next_word_b(&p, buf, sizeof(buf)-1));
			while(*p && isspace(*p)) p++;
		}else{
			cnfnode_setval(cn, dup_line_until_b(&p, sep, buf, sizeof(buf)-1));
			if(*p) p++;
		}
		append_node(cn_top, cn);
	}

	if(*p){
		cn = create_cnfnode(cols[i++]);
		cnfnode_setval(cn, dup_next_line_b(&p, buf, sizeof(buf)-1));
		append_node(cn_top, cn);
	}

	*pp = p;
	return cn_top;
}

struct cnfnode *parse_table(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn_top, *cn;
	struct confline *cl, *cl_root;
	char *cols[32];
	int ncols, i;
	int sep = -1;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	if(cm && cm->opt_root){
		if((ncols = parse_table_options(cm->opt_root, cols, 32, &sep)) <= 0){
			fprintf(stderr, "need 'cols' option for table module\n");
			return NULL;
		}
	}else{
		fprintf(stderr, "need 'cols' option for table module\n");
		return NULL;
	}

	for(cl = cl_root; cl; cl = cl->next){
		const char *p = cl->line;
		char buf[strlen(p)+1];

		while(*p && isspace(*p)) p++;
		if(*p && *p != '#'){
			cn_top = parse_table_line(&p, cols, ncols, sep);
			if(cn_top)
				append_node(cn_root, cn_top);

		}else if(*p == '#'){
			cn = create_cnfnode(".comment");
			cnfnode_setval(cn, dup_next_line_b(&p, buf, sizeof(buf)-1));
			append_node(cn_root, cn);
		}else if(!*p){
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
		}
	}

	for(i = 0; i < ncols; i++)
		if(cols[i]) free(cols[i]);


	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_table_line(FILE *fptr, struct cnfnode *cn_top, char **cols, int ncols, int sep)
{
	struct cnfnode *cn;
	int i;
	
	fprintf(fptr, "%s", cn_top->name);
      
	for(i = 1; i < ncols; i++){
		cn = find_node(cn_top->first_child, cols[i]);
		if(cn && cn->value)
			fprintf(fptr, "%c%s", sep == -1 ? '\t' : (char)sep, cn->value);
		else if(sep != -1)
			fprintf(fptr, "%c", (char)sep);
	}
	fprintf(fptr, "\n");

	return 0;
}

int unparse_table(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;
	char *cols[32];
	int ncols, i;
	int sep = -1;

	if((ncols = parse_table_options(cm->opt_root, cols, 32, &sep)) <= 0){
		fprintf(stderr, "need 'cols' option for table module\n");
		return -1;
	}

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s\n", cn_top->value);
		}else{
			unparse_table_line(fptr, cn_top, cols, ncols, sep);
		}
	}

	for(i = 0; i < ncols; i++)
		if(cols[i]) free(cols[i]);

	return 0;
}

void register_table(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

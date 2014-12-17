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
#include "shell.h"

static
struct cnfmodule this_module = {
	name: "shell",
	default_file: NULL,
	parser: parse_shell,
	unparser: unparse_shell
};

/* not static, used by conserver */
struct cnfnode *parse_shell_line(const char **pp)
{
	char buf[256], *q;
	const char *p = *pp;
	struct cnfnode *cn = NULL;

	q = buf;
	while(*p && (isalnum(*p) || *p == '_') && q < buf+sizeof(buf)-1)
		*(q++) = *(p++);
	*q = 0;
	if(*p == '='){
		const char *p0;
		
		p++;
		cn = create_cnfnode(buf);
		q = buf;
		
		if(*p == '"' || *p == '\'')
			dup_next_quoted_b(&p, buf, sizeof(buf)-1, *p);
		else{
			while(*p && !isspace(*p) && *p != '#')
				*q++ = *p++;
			*q = 0;
		}
		
		cnfnode_setval(cn, buf);
		
		p0 = p;
		while(*p && isspace(*p)) p++;
		if(*p == '#'){
			struct cnfnode *cn_cmt;
			
			cn_cmt = create_cnfnode(".comment");
			cnfnode_setval(cn_cmt,
				       dup_next_line_b(&p0, buf, sizeof(buf)-1));
			append_node(cn, cn_cmt);
		}
	}else{
		cn = create_cnfnode(".unparsed");
		cnfnode_setval(cn, *pp);
	}

	*pp = p;
	return cn;
}

static
int parse_shell_options(struct cnfnode *opt_root, int *pquotes)
{
	struct cnfnode *cn;
	int i = 0;

	if(opt_root == NULL) return -1;
	if(opt_root->first_child == NULL) return -1;

	for(cn = opt_root->first_child; cn; cn = cn->next){
		if(strcmp(cn->name, "quotes") == 0){
			if(strcmp(cn->value, "no") == 0)
				*pquotes = 0;
		}
	}

	return i;
}

struct cnfnode *parse_shell(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		const char *p = cl->line;

		while(*p && isspace(*p)) p++;
		if(*p){
			if(isalpha(*p)){
				cn = parse_shell_line(&p);
				if(cn)
					append_node(cn_root, cn);
			}else if(*p == '#'){
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, cl->line);
				append_node(cn_root, cn);
			}else{
				cn = create_cnfnode(".unparsed");
				cnfnode_setval(cn, cl->line);
				append_node(cn_root, cn);
			}
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, cl->line);
			append_node(cn_root, cn);
		}
	}
	destroy_confline_list(cl_root);

	return cn_root;
}

static
int unparse_shell_line(FILE *fptr, struct cnfnode *cn_top, int do_quotes)
{
	if(do_quotes)
		fprintf(fptr, "%s=\"%s\"", cn_top->name, cn_top->value);
	else
		fprintf(fptr, "%s=%s", cn_top->name, cn_top->value);

	if(cn_top->first_child){
		struct cnfnode *cn_cmt = cn_top->first_child;
		if(cn_cmt->name[0] == '.')
			fprintf(fptr, "%s", cn_cmt->value);
	}
	fprintf(fptr, "\n");

	return 0;
}

int unparse_shell(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;
	int do_quotes = 1;

	if(cm && cm->opt_root){
		parse_shell_options(cm->opt_root, &do_quotes);
	}

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s", cn_top->value);
		}else{
			unparse_shell_line(fptr, cn_top, do_quotes);
		}
	}

	return 0;
}

void register_shell(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

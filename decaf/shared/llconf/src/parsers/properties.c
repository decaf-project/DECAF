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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "strutils.h"
#include "nodes.h"
#include "lines.h"
#include "modules.h"
#include "properties.h"

static
struct cnfmodule this_module = {
	name: "properties",
	default_file: NULL,
	parser: parse_properties,
	unparser: unparse_properties
};

/* for the Java properties file syntax read:
   http://java.sun.com/j2se/1.4.2/docs/api/java/util/Properties.html */

struct cnfnode *parse_properties_line(struct confline **cl, char **pp)
{
	int len = 0;
	char *p = *pp, *q, *buf;
	struct cnfnode *cn = NULL;

	while (*p && isprint(*p) && (*p != '=')){ 
		p++; 
		len++;
	}

	/* save key */
	buf = strndup(*pp, len);
	cn = create_cnfnode(buf);
	free(buf); buf = NULL;

	while(*p && isspace(*p)) p++;
	if(*p == '=' || *p == ':') p++;
	while(*p && isspace(*p)) p++;
	q=p;

	/* get element value */

	while(*p)
		/* element value reaches over to next line  */
		if(*p == '\\' && p[1] == '\n') {
			*p=0; /* temp. delete '\' */
			q = strjoin(buf, q);
			free(buf); buf = q;
			*p= '\\'; /* restore '\' */

			/* continue with next line in
			   confline buffer */
			*cl = (*cl)->next;
			p = (*cl)->line;
			/* eat up space in the beginning of
			   the line after '\' terminated line */
			while(*p && isspace(*p)) p++;
			q = p;
		} else p++;

	if(*q) {
		/* catch remaining element value */
		char c = *--p; *p=0; /* remove cr */
		q = strjoin(buf, q);
		free(buf); buf = q;
		*p=c;
	}
		
	cnfnode_setval(cn, buf);
	free(buf);

	return cn;
}

struct cnfnode *parse_properties(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		char *p = cl->line;

		while(*p && isspace(*p)) p++;
		if(*p){
			if(isalnum(*p)){
				cn = parse_properties_line(&cl, &p);
				if(cn)
					append_node(cn_root, cn);
			}else if(*p == '#' || *p == '!'){
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
int unparse_properties_line(FILE *fptr, struct cnfnode *cn_top)
{
	fprintf(fptr, "%s=%s", cn_top->name, cn_top->value);

	if(cn_top->first_child){
		struct cnfnode *cn_cmt = cn_top->first_child;
		if(cn_cmt->name[0] == '.')
			fprintf(fptr, "%s", cn_cmt->value);
	}
	fprintf(fptr, "\n");

	return 0;
}

int unparse_properties(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s", cn_top->value);
		}else{
			unparse_properties_line(fptr, cn_top);
		}
	}

	return 0;
}

void register_properties(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

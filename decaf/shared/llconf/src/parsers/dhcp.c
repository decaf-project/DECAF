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
#include "dhcp.h"

static
struct cnfmodule this_module = {
	name: "dhcp",
	parser: parse_dhcp,
	unparser: unparse_dhcp
};

static
struct confline *parse_dhcp_top(struct confline *cl_first, struct cnfnode *cn_top);

static
void dhcp_cp_arg(const char **pp, char **pq, int n)
{
	char *q = *pq, *q0 = *pq;
	const char *p = *pp;
	int in_quotes = 0;

	while(*p && (in_quotes || (*p != ';' && *p != '{')) && q < q0 + n){
		if(*p == '"') in_quotes = !in_quotes;
		*q++ = *p++;
	}
	*q = 0;
	/* strip trailing spaces: */
	while(q > q0 && isspace(q[-1])) *(--q) = 0;

	*pp = p;
	*pq = q;
}

static
struct confline *parse_dhcp_statement(struct confline *cl, struct cnfnode *cn_top, const char *p)
{
	char buf[256], *q;

	q = buf;
	skip_spaces(&p);
	dhcp_cp_arg(&p, &q, sizeof(buf)-1);

	if(*p == ';'){
		if(strncmp(cnfnode_getname(cn_top), "option", 6) == 0){
			struct cnfnode *cn_opt;
			char opt[256], *q = opt;
			const char *p = buf;
			
			while(*p && !isspace(*p) && q < opt+255) *q++ = *p++;
			*q = 0;
			skip_spaces(&p);

			cn_opt = create_cnfnode(opt);
			cnfnode_setval(cn_opt, p);
			append_node(cn_top, cn_opt);
			
		}else{
			/* this is simple statement */
			cnfnode_setval(cn_top, buf);
		}
		if(cl) cl = cl->next;
	}else if(*p == '{'){
		/* this is a group of statements.. */
		if(buf[0] == 0){
			/* ... without a name (like: group{}, pool{} */
			cl = cl->next;
			cl = parse_dhcp_top(cl, cn_top);
		}else{
			/* ... with a name (like: subnet{}, class{} */

			cnfnode_setval(cn_top, buf);
			cl = cl->next;
			cl = parse_dhcp_top(cl, cn_top);
		}
	}
	return cl;
}

static
struct confline *parse_dhcp_top(struct confline *cl_first, struct cnfnode *cn_top)
{
	struct confline *cl;
	struct cnfnode *cn;

	for(cl = cl_first; cl;){
		const char *p = cl->line;
		char buf[256];

		while(*p && isspace(*p)) p++;
		if(*p){
			if((*p != '#') && (*p != '}')){
				char *q = buf;

				skip_spaces(&p);
				while(*p && !isspace(*p) && (*p != ';') && q < buf+255) *(q++) = *(p++);
				*q = 0;

				cn = create_cnfnode(buf);
				append_node(cn_top, cn);

				skip_spaces(&p);

				if(*p && *p != ';'){
					cl = parse_dhcp_statement(cl, cn, p);
				}else
					cl = cl->next;
			}else if(*p == '}'){
				cl = cl->next;
				return cl;
			}else{
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
				append_node(cn_top, cn);
				cl = cl->next;
			}
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_top, cn);
			cl = cl->next;
		}
	}
	return cl;
}

struct cnfnode *parse_dhcp(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_top;
	struct confline *cl_root;

	cl_root = read_conflines(fptr);

	cn_top = create_cnfnode("(root)");

	parse_dhcp_top(cl_root, cn_top);

	destroy_confline_list(cl_root);
	return cn_top;
}

static
int unparse_dhcp_top(struct cnfnode *cn_root, FILE *fptr, int level)
{
	struct cnfnode *cn;

	for(cn = cn_root->first_child; cn; cn = cn->next){
		const char *name = cnfnode_getname(cn);
		const char *value = cnfnode_getval(cn);
		int i;

		if(name[0] == '.'){
			fprintf(fptr, "%s\n", value);
		}else{
			for(i = 0; i < level; i++)
				putc('\t', fptr);

			if(cn->first_child){
				if(strcmp(name, "option") != 0){
					if(value)
						fprintf(fptr, "%s %s {\n", name, value);
					else
						fprintf(fptr, "%s {\n", name);
					unparse_dhcp_top(cn, fptr, level+1);
					
					for(i = 0; i < level; i++)
						putc('\t', fptr);
					fprintf(fptr, "}\n");
				}else{
					struct cnfnode *cn_opt = cn->first_child;
					const char *opt_val = cnfnode_getval(cn_opt);

					fprintf(fptr, "%s %s %s;\n", name, cnfnode_getname(cn_opt), opt_val ? opt_val : "");
				}
			}else{
				if(value)
					fprintf(fptr, "%s %s;\n", name, value);
				else
					fprintf(fptr, "%s;\n", name);
			}
		}
	}
	return 0;
}

int unparse_dhcp(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	return unparse_dhcp_top(cn_root, fptr, 0);

	return 0;
}

void register_dhcp(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

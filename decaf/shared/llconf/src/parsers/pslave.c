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

#include "pslave.h"

static
struct cnfmodule this_module = {
	name: "pslave",
	default_file: "/etc/portslave/pslave.conf",
	parser: parse_pslave,
	unparser: unparse_pslave
};

struct cnfnode *parse_pslave(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn, *cn_top;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl;){
		const char *p = cl->line;
		char buf[256], *q;

		while(*p && isspace(*p)) p++;

		if(!*p){
			/* empty line */
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
			cl = cl->next;
		}else if(*p == '#'){
			/* comment */
			cn = create_cnfnode(".comment");
			cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
			append_node(cn_root, cn);
			cl = cl->next;
		}else{
			/* possible configuration line */
			/* get part before dot */
			q = buf;
			while(*p && isalnum(*p) && *p != '.')
				*q++ = *p++;
			*q = 0;
			if(*p == '.'){
#if 0
				/* we may append to an already existing tree.
				   unfortunately, this could break the order of lines
				   when unparsing */
				cn_top = find_node(cn_root->first_child, buf);
				if(!cn_top){
					cn_top = create_cnfnode(buf);
					append_node(cn_root, cn_top);
				}
#else
				cn_top = create_cnfnode(buf);
				append_node(cn_root, cn_top);
#endif
				p++;
				q = buf;
				while(*p && (isalnum(*p) || *p == '_'))
					*q++ = *p++;
				*q = 0;
				if(buf[0]){
					/* part after dot */
					cn = create_cnfnode(buf);

					while(*p && isspace(*p)) p++;
					q = buf;
					while(*p && *p != '\n' && *p != '#')
						*q++ = *p++;
					*q = 0;
					
					/* trim trailing spaces from value: */
					q--;
					while(isspace(*q) && (q > buf)) *q-- = 0;

					cnfnode_setval(cn, buf);
					append_node(cn_top, cn);

					if(*p == '#'){
						dup_next_line_b(&p, buf, 255);
						cn = create_cnfnode(".comment");
						cnfnode_setval(cn, buf);
						append_node(cn_top, cn);
					}
				}
			}else{
				/* we have no dot... must be sth else */
				cn_top = create_cnfnode(".unparsed");
				p = cl->line;
				dup_next_line_b(&p, buf, 255);
				cnfnode_setval(cn_top, buf);
				append_node(cn_root, cn_top);
			}
			cl = cl->next;
		}
	}
	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_pslave(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top, *cn;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s\n", cn_top->value);
		}else{
			fprintf(fptr, "%s", cn_top->name);
			for(cn = cn_top->first_child; cn; cn = cn->next){
				if(cn->name[0] == '.'){
					fprintf(fptr, "%s\n", cn->value);
				}else{
					fprintf(fptr, ".%s", cn->name);
					if(cn->value)
						fprintf(fptr, " %s", cn->value);
				}
			}
			fprintf(fptr, "\n");
		}
	}
	return 0;
}

void register_pslave(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

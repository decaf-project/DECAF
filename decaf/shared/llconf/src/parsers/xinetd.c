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
#include "xinetd.h"

static
struct cnfmodule this_module = {
	name: "xinetd",
	parser: parse_xinetd,
	unparser: unparse_xinetd
};

static
struct confline *parse_xinetd_section(struct confline *cl_first, struct cnfnode *cn_top)
{
	struct confline *cl;

	for(cl = cl_first; cl;){
		struct cnfnode *cn;
		const char *p = cl->line;
		char buf[256];

		if(*p && (*p != '#') && (*p != '}')){
			char *q = buf;

			skip_spaces(&p);

			if(*p){
				while(*p && !isspace(*p) && (*p != '=') && (*p != '+') && (*p != '-') && q < buf+255) *(q++) = *(p++);
				*q = 0;
				
				cn = create_cnfnode(buf);
				append_node(cn_top, cn);
				
				skip_spaces(&p);
				
				if(*p){
					if(*p == '-') p++;
					else if(*p == '+') p++;
					
					if(*p == '='){
						p++;
						skip_spaces(&p);
						q = buf;
						while(*p && (*p != '\r') && (*p != '\n') && q < buf+255) *(q++) = *(p++);
						*q = 0;
						cnfnode_setval(cn, buf);
					}
				}
			}else{
				cn = create_cnfnode(".empty");
				cnfnode_setval(cn, "");
				append_node(cn_top, cn);
			}
			cl = cl->next;
		}else if(*p == '}'){
			cl = cl->next;
			return cl;
		}else if(*p == '#'){
			cn = create_cnfnode(".comment");
			cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
			append_node(cn_top, cn);
			cl = cl->next;
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_top, cn);
			cl = cl->next;
		}
	}
	return cl;
}

static
struct confline *parse_xinetd_top(struct confline *cl_first, struct cnfnode *cn_root)
{
	struct confline *cl;
	struct cnfnode *cn_top = NULL;

	for(cl = cl_first; cl;){
		const char *p = cl->line;
		char buf[256];

		while(*p && isspace(*p)) p++;
		if(*p){
			if((*p != '#') && (*p != '{')){
				char *q = buf;

				skip_spaces(&p);
				while(*p && !isspace(*p) && q < buf+255) *(q++) = *(p++);
				*q = 0;

				cn_top = create_cnfnode(buf);
				append_node(cn_root, cn_top);

				skip_spaces(&p);

				if(*p){
					q = buf;
					while(*p && !isspace(*p) && q < buf+255) *(q++) = *(p++);
					*q = 0;

					cnfnode_setval(cn_top, buf);
				}
				cl = cl->next;
			}else if(*p == '{'){
				if(cn_top){
					cl = cl->next;
					cl = parse_xinetd_section(cl, cn_top);
				}
			}else{
				struct cnfnode *cn;

				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
				append_node(cn_root, cn);
				cl = cl->next;
			}
		}else{
			struct cnfnode *cn;

			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
			cl = cl->next;
		}
	}
	return cl;
}

struct cnfnode *parse_xinetd(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root;
	struct confline *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	parse_xinetd_top(cl_root, cn_root);

	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_xinetd(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cnfnode_getname(cn_top)[0] != '.'){
			if(cnfnode_getval(cn_top))
				fprintf(fptr, "%s %s\n", cnfnode_getname(cn_top), cnfnode_getval(cn_top));
			else
				fprintf(fptr, "%s\n", cnfnode_getname(cn_top));
			if(cn_top->first_child){
				struct cnfnode *cn;

				fprintf(fptr, "{\n");

				for(cn = cn_top->first_child; cn; cn = cn->next){
					if(cnfnode_getname(cn)[0] != '.'){
						fprintf(fptr, "\t%s = %s\n", cnfnode_getname(cn), cnfnode_getval(cn));
					}else
						fprintf(fptr, "%s\n", cnfnode_getval(cn));
				}

				fprintf(fptr, "}\n");
			}
		}else
			fprintf(fptr, "%s\n", cnfnode_getval(cn_top));
	}
	return 0;
}

void register_xinetd(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

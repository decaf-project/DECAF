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

#include "cyconf.h"

static
struct cnfmodule this_module = {
	name: "cyconf",
	parser: parse_cyconf,
	unparser: unparse_cyconf
};

static
struct confline *_parse_cyconf(struct cnfnode *cn_root, struct confline *cl)
{
	char buf[1024], *q;
	const char *p;
	struct cnfnode *cn_top;

	p = cl->line;

	while(*p && isspace(*p)) p++;

	if(*p == '#'){
		dup_next_line_b(&p, buf, sizeof(buf));
		cn_top = create_cnfnode(".comment");
		cnfnode_setval(cn_top, buf);
		append_node(cn_root, cn_top);
		cl = cl->next;

		return cl;
	}

	q = buf;
	while(*p && !isspace(*p) && *p != '{') *q++ = *p++;
	*q = 0;

	if(q > buf && q[-1] == ':'){
		while(*p && isspace(*p)) p++;

		append_node(cn_root, cn_top = create_cnfnode(buf));

		if(*p != '{'){
			
			while(*p && isspace(*p)) p++;
			dup_next_line_b(&p, buf, sizeof(buf));
			/* Strip trailing whitespace */
			if (*buf){
				q = buf + strlen(buf);
				while ((q > buf) && isspace(*(q - 1)))
					q--;
				*q = 0;
			}
			cnfnode_setval(cn_top, buf);
			return cl->next;
		}else{
			const char *p0;
			char *buffer, *q;
			int buf_size = 0;
			struct confline *cl0;

			p++;
			p0 = p;
			cl0 = cl;
			while(cl){
				while(*p && isspace(*p)) p++;
				cl = cl->next;
				if(*p == '}')
					break;
				else if(*p != '#')
					buf_size += strlen(p);
				if(cl)
					p = cl->line;
			}
			buffer = malloc(buf_size + 1);
			q = buffer;
			cl = cl0;
			p = p0;
			while(cl){
				while(*p && isspace(*p)) p++;
				cl = cl->next;
				if(*p == '}')
					break;
				else if(*p != '#')
					while(*p) *q++ = *p++;
				if(cl)
					p = cl->line;
			}
			*q = 0;
			cnfnode_setval(cn_top, buffer);
			free(buffer);
			return cl;
		}
	}else{
		while(*p && isspace(*p)) p++;
		if(*p == '{'){
			append_node(cn_root, cn_top = create_cnfnode(buf));

			cl = cl->next;
			while(1){
				p = cl->line;
				while(*p && isspace(*p)) p++;

				if(*p == '}'){
					cl = cl->next;
					break;
				}
				cl = _parse_cyconf(cn_top, cl);
				if(!cl)
					break;
			}
		} else {
			p = cl->line;
			while(*p && isspace(*p)) p++;
			if(!*p){
				cn_top = create_cnfnode(".empty");
				cnfnode_setval(cn_top, "");
				append_node(cn_root, cn_top);
			} else if(*p == '#'){
				cn_top = create_cnfnode(".comment");
				cnfnode_setval(cn_top, cl->line);
				append_node(cn_root, cn_top);
			} else {
				cn_top = create_cnfnode(".unparsed");
				cnfnode_setval(cn_top, cl->line);
				append_node(cn_root, cn_top);
			}
			cl = cl->next;
		}
	}
	return cl;
}

static
int _unparse_cyconf(FILE *fptr, struct cnfnode *cn_root, int level)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		int i;
			
		for(i = 0; i < level; i++) fputc('\t', fptr);

		if(!cnfnode_getval(cn_top)){
			fprintf(fptr, "%s{\n", cnfnode_getname(cn_top));
			_unparse_cyconf(fptr, cn_top, level+1);
			for(i = 0; i < level; i++) fputc('\t', fptr);
			fprintf(fptr, "}\n");
		}else{
			if(cnfnode_getname(cn_top)[0] != '.')
				fprintf(fptr, "%s %s\n", cnfnode_getname(cn_top), cnfnode_getval(cn_top));
			else
				fprintf(fptr, "%s\n", cnfnode_getval(cn_top));
		}
	}
	return 0;
}

int unparse_cyconf(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	_unparse_cyconf(fptr, cn_root, 0);

	return 0;
}

struct cnfnode *parse_cyconf(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);
	
	cn_root = create_cnfnode("(root)");
	
	for(cl = cl_root; cl;){
		cl = _parse_cyconf(cn_root, cl);
	}

	destroy_confline_list(cl_root);
	return cn_root;
}

void register_cyconf(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

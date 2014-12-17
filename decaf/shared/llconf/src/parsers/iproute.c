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
#include "iproute.h"

static
struct cnfmodule this_module = {
	name: "iproute",
	default_file: NULL,
	parser: parse_iproute,
	unparser: unparse_iproute
};

struct cnfnode *parse_iproute(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn_top, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		char buf[256];
		const char *p = cl->line;

		if(*p && *p != '#'){
			dup_next_word_b(&p, buf, sizeof(buf)-1);
			
			cn_top = create_cnfnode(buf);
			
			skip_spaces(&p);
			/* simple assumption that args are key, spaces, value repeated.
			   This is no true for all ip arguments. */
			while(*p){
				dup_next_word_b(&p, buf, sizeof(buf)-1);
				cn = create_cnfnode(buf);
				dup_next_word_b(&p, buf, sizeof(buf)-1);
				cnfnode_setval(cn, buf);
				skip_spaces(&p);
				append_node(cn_top, cn);
			}
			append_node(cn_root, cn_top);

		}else if(*p == '#'){
			cn = create_cnfnode(".comment");
			cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
			append_node(cn_root, cn);
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
		}
	}

	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_iproute(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top, *cn;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cnfnode_getname(cn_top)[0] != '.'){
			fprintf(fptr, "%s", cnfnode_getname(cn_top));
			for(cn = cn_top->first_child; cn; cn = cn->next){
				fprintf(fptr, " %s %s", cnfnode_getname(cn), cnfnode_getval(cn));
			}
			fprintf(fptr, "\n");
		}else{
			fprintf(fptr, "%s\n", cn_top->value);
		}
	}
	return 0;
}

void register_iproute(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

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
#include "pair.h" /* for parse_pair_line() */
#include "mgetty.h"

static
struct cnfmodule this_module = {
	name: "mgetty",
	default_file: NULL,
	parser: parse_mgetty,
	unparser: unparse_mgetty
};

struct cnfnode *parse_mgetty(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn = NULL, *cn_port = NULL, *cn_port_top = NULL;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");
	cn_port = cn_root; /* once we have a port line, we do not have any global options any more */

	for(cl = cl_root; cl; cl = cl->next){
		char buf[256];
		const char *p = cl->line;

		while(*p && isspace(*p)) p++;
		if(*p){
			if(*p != '#'){
				const char *p0 = p;

				dup_next_word_b(&p, buf, sizeof(buf)-1);
				if(strcmp(buf, "port") == 0){
					if(!cn_port_top){
						cn_port_top = create_cnfnode("port");
						append_node(cn_root, cn_port_top);
					}
					cn_port = create_cnfnode(dup_next_word_b(&p, buf, sizeof(buf)-1));
					append_node(cn_port_top, cn_port);
				}else{
					p = p0;
					cn = parse_pair_line(&p);
					append_node(cn_port, cn);
				}

			}else if(*p == '#'){
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
				append_node(cn_port, cn);
			}else{
				cn = create_cnfnode(".unparsed");
				cnfnode_setval(cn, cl->line);
				append_node(cn_port, cn);
			}
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_port, cn);
		}
      
	}
	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_mgetty(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s\n", cn_top->value);
		}else{
			if(strcmp(cn_top->name, "port") != 0){
				unparse_pair_line(fptr, cn_top);
			}else{
				struct cnfnode *cn_port_top = cn_top, *cn_port, *cn;

				for(cn_port = cn_port_top->first_child; cn_port; cn_port = cn_port->next){
					fprintf(fptr, "port %s\n", cn_port->name);
					for(cn = cn_port->first_child; cn; cn = cn->next){
						if(cn->name[0] == '.')
							fprintf(fptr, "%s\n", cn->value);
						else{
							fprintf(fptr, "  "); /* indent */
							unparse_pair_line(fptr, cn);
						}
					}
				}
			}
		}
	}

	return 0;
}

void register_mgetty(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

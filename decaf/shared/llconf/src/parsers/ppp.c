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
#include "ppp.h"

static
struct cnfmodule this_module = {
	name: "ppp",
	default_file: NULL,
	parser: parse_ppp,
	unparser: unparse_ppp
};

struct cnfnode *parse_ppp(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_top, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_top = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		char buf[256], *q;
		const char *p = cl->line;

		while(*p && isspace(*p)) p++;
		if(*p){
			if (*p == '/'){
				q = buf;
				while(*p && !isspace(*p) && *p != '#' && q < buf+255)
					*q++ = *p++;
				*q = 0;
				cn = create_cnfnode("device");
				cnfnode_setval(cn, buf);
				append_node(cn_top, cn);
			}else if (isdigit(*p) || *p == ':') {
				int is_speed = 1;

				q = buf;
				while(*p && (isdigit(*p) || *p == '.' || *p == ':')
				      && q < buf+sizeof(buf)-1 ){
					if(!isdigit(*p))
						is_speed = 0;
					*q++ = *p++;
				}
				*q = 0;
				cn = create_cnfnode(is_speed ? "speed" : "ip");
				cnfnode_setval(cn, buf);
				append_node(cn_top, cn);
			}else if(*p != '#'){
				int no = 0;

				dup_next_word_b(&p, buf, 255);
				if(strncmp(buf, "no", 2) == 0){
					no = 1;
				}
				cn = create_cnfnode(no ? buf+2 : buf);
				append_node(cn_top, cn);
	
				while(*p && isspace(*p)) p++;

				if(*p){
					if(*p == '"' || *p == '\''){
						char c = *p;
	    
						buf[0] = c;
						dup_next_quoted_b(&p, buf+1, 253, *p);
						buf[strlen(buf)+1] = 0;
						buf[strlen(buf)] = c;
	    
					}else
						dup_line_until_b(&p, '#', buf, 255);
	  
					cnfnode_setval(cn, buf);
				} else {
					if(no)
						cnfnode_setval(cn, "no");
					else
						cnfnode_setval(cn, "yes");
				}
	
				while(*p && isspace(*p)) p++;
	
				if(*p == '#'){
					cn = create_cnfnode(".comment");
					cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
					append_node(cn_top, cn);
				}
			} else { /* *p == '#' */
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
				append_node(cn_top, cn);
			}
		} else {
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_top, cn);
		}
      
	}
	destroy_confline_list(cl_root);
	return cn_top;
}

int unparse_ppp(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s\n", cn_top->value);
		}else{
			if((strcmp(cn_top->name, "ip") == 0) ||
			   (strcmp(cn_top->name, "speed") == 0) ||
			   (strcmp(cn_top->name, "device") == 0)){
				fprintf(fptr, "%s\n", cn_top->value);
			}else{
				if(cn_top->value){
					if(strcmp(cn_top->value, "yes") == 0)
						fprintf(fptr, "%s\n", cn_top->name);
					else if(strcmp(cn_top->value, "no") == 0)
						fprintf(fptr, "no%s\n", cn_top->name);
					else
						fprintf(fptr, "%s %s\n",
							cn_top->name, cn_top->value);
				}else
					fprintf(fptr, "%s\n", cn_top->name);
			}
		}
	}

	return 0;
}

void register_ppp(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

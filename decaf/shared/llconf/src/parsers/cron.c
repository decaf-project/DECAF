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
#include "cron.h"

static
struct cnfmodule this_module = {
	name: "cron",
	parser: parse_cron,
	unparser: unparse_cron
};

struct cnfnode *parse_cron(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		const char *p = cl->line;
		char buf[strlen(p)+1];

		while(*p && isspace(*p)) p++;
		if(*p){
			if(isalpha(*p)){ /* env. setting */
				char *q = buf;

				while(*p && *p != '=') *q++ = *p++;
				*q = 0;
				cn = create_cnfnode(buf);
				skip_spaces(&p);

				if(*p == '='){
					p++; q = buf;
					skip_spaces(&p);
					cp_word(&p, &q, sizeof(buf));
					*q = 0;
					cnfnode_setval(cn, buf);
				}
				append_node(cn_root, cn);
			}else if(isdigit(*p) || (*p == '@')){
				struct cnfnode *cn_time, *cn_user, *cn_cmd;
				int i, l = sizeof(buf);
				char *q = buf;

				q = buf;
				if(*p == '@'){
					cp_word(&p, &q, l);
				}else{
					for(i = 0; *p && i < 5; i++){
						cp_word(&p, &q, l);
						if(i < 4)
							cp_spaces(&p, &q, l);
					}
				}
				*q = 0;
				skip_spaces(&p);
				cn_time = create_cnfnode(buf);

				dup_next_word_b(&p, buf, sizeof(buf)-1);
				cn_user = create_cnfnode("user");
				cnfnode_setval(cn_user, buf);

				skip_spaces(&p);
					
				dup_next_line_b(&p, buf, sizeof(buf)-1);
				cn_cmd = create_cnfnode("command");
				cnfnode_setval(cn_cmd, buf);

				append_node(cn_time, cn_user);
				append_node(cn_time, cn_cmd);
				append_node(cn_root, cn_time);
				
			}else if(*p == '#'){
				cn = create_cnfnode(".comment");
				dup_next_line_b(&p, buf, sizeof(buf)-1);
				cnfnode_setval(cn, buf);

				append_node(cn_root, cn);
			}
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
		}
	}

	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_cron(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(isalpha(cn_top->name[0]))
			fprintf(fptr, "%s = %s\n", cn_top->name, cn_top->value ? cn_top->value : "");
		else if(cn_top->name[0] != '.'){
			struct cnfnode *cn;

			fprintf(fptr, "%s\t", cn_top->name);

			cn = find_node(cn_top->first_child, "user");
			if(cn)
				fprintf(fptr, "%s\t", cn->value ? cn->value : "");
			cn = find_node(cn_top->first_child, "command");
			if(cn)
				fprintf(fptr, "%s", cn->value ? cn->value : "");

			fprintf(fptr, "\n");
		}else if((strcmp(cn_top->name, ".comment") == 0) ||
			 (strcmp(cn_top->name, ".empty") == 0)){
			fprintf(fptr, "%s\n", cn_top->value);
		}
	}

	return 0;
}

void register_cron(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

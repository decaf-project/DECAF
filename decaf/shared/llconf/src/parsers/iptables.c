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

#include "iptables.h"

static
struct cnfmodule this_module = {
	name: "iptables",
	parser: parse_iptables,
	unparser: unparse_iptables,
};

static
void parse_iptables_rules(struct cnfnode *cn_table, struct confline *cl)
{
	struct cnfnode *cn_chain, *cn;
	const char *p = cl->line;
	char buf[256];

	/* -A */
	dup_next_word_b(&p, buf, sizeof(buf)-1);

	/* chain: */
	dup_next_word_b(&p, buf, sizeof(buf)-1);
	cn_chain = create_cnfnode(buf);

	/* options: */
	while(*p && isspace(*p)) p++;
	while(*p){
		char option[64];
		int inv = 0;

		if(*p == '!'){
			inv = 1;
			p++;
			while(*p && isspace(*p)) p++;
		}

		dup_next_word_b(&p, option, sizeof(option)-1);
		cn = create_cnfnode(option);

		while(*p && isspace(*p)) p++;
		if(*p == '!'){
			inv = 1;
			p++;
			while(*p && isspace(*p)) p++;
		}

		if(strcmp(option, "--tcp-flags")){
			dup_next_word_b(&p, buf, sizeof(buf)-1);
			cnfnode_setval(cn, buf);
		}else{
			/* tcp-flags is the only option that takes two arguments, AFAIK */
			char buf1[256], string[256];

			dup_next_word_b(&p, buf, sizeof(buf)-1);
			while(*p && isspace(*p)) p++;
			dup_next_word_b(&p, buf1, sizeof(buf1)-1);
			snprintf(string, sizeof(string), "%s %s", buf, buf1);
			cnfnode_setval(cn, string);
		}
		append_node(cn_chain, cn);

		if(inv){
			struct cnfnode *cn_inv;
			cn_inv = create_cnfnode("inv");
			append_node(cn, cn_inv);
		}

		while(*p && isspace(*p)) p++;
	}
	append_node(cn_table, cn_chain);
}

struct cnfnode *parse_iptables(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn_top, *cn_table = NULL, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		const char *p = cl->line;
		char buf[256];

		if(*p == '#'){
			cn_top = create_cnfnode(".comment");
			cnfnode_setval(cn_top, cl->line);
			if(cn_table)
				append_node(cn_table, cn_top);
			else
				append_node(cn_root, cn_top);
		}else if(*p == ':'){
			p++;

			cn_top = create_cnfnode(".policy");
			dup_next_word_b(&p, buf, sizeof(buf)-1);
			cn = create_cnfnode(buf);
			dup_next_word_b(&p, buf, sizeof(buf)-1);
			cnfnode_setval(cn, buf);
			append_node(cn_table, cn_top);
			append_node(cn_top, cn);
		}else if(*p == '*'){
			p++;

			dup_next_word_b(&p, buf, sizeof(buf)-1);
			cn_table = create_cnfnode(buf);
			append_node(cn_root, cn_table);
		}else if(*p == '-'){
			parse_iptables_rules(cn_table, cl);
		}else if(strncmp(p, "COMMIT", 6) == 0){
			cn_top = create_cnfnode("COMMIT");
			append_node(cn_table, cn_top);
		}
	}
	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_iptables(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top, *cn_table = NULL, *cn;

	for(cn_table = cn_root->first_child; cn_table; cn_table = cn_table->next){
		if(strcmp(cn_table->name, ".comment") == 0){
			fprintf(fptr, "%s", cn_table->value);
		}else{
			fprintf(fptr, "*%s\n", cn_table->name);
			for(cn_top = cn_table->first_child; cn_top; cn_top = cn_top->next){
				if(strcmp(cn_top->name, ".comment") == 0){
					fprintf(fptr, "%s", cn_top->value);
				}else if(strcmp(cn_top->name, ".policy") == 0){
					cn = cn_top->first_child;
					if(cn){
						fprintf(fptr, ":%s %s\n", cn->name, cn->value);
					}
				}else if(strcmp(cn_top->name, "COMMIT") == 0){
					fprintf(fptr, "%s\n", cn_top->name);
				}else{
					fprintf(fptr, "-A %s ", cn_top->name);
					for(cn = cn_top->first_child; cn; cn = cn->next){
						struct cnfnode *cn_inv = cn->first_child;
						if(cn_inv && strcmp(cn_inv->name, "inv") == 0){
							/* if the option has an argument,
							   the '!' comes after the option, before the argument */
							if(cn->value && (cn->value[0] != 0)){
								fprintf(fptr, "%s ! %s%s", cn->name, cn->value,
									cn->next ? " " : "\n");
							/* otherwise,
							   the '!' comes before the option */
							}else{
								fprintf(fptr, "! %s%s", cn->name, cn->next ? " " : "\n");
							}
						}else{
							fprintf(fptr, "%s %s%s", cn->name, cn->value ? cn->value : "",
								cn->next ? " " : "\n");
						}
					}
				}
			}
		}
	}
	return 0;
}

void register_iptables(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

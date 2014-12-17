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
#include "ifupdown.h"

static
struct cnfmodule this_module = {
	name: "ifupdown",
	default_file: "/etc/network/interfaces",
	parser: parse_ifupdown,
	unparser: unparse_ifupdown
};


static char *stanzas[] = {"auto", "mapping", "iface", "allow-", NULL};

static
struct confline *parse_ifupdown_auto(struct confline *cl, struct cnfnode *cn_root)
{
	const char *p = cl->line;
	char *iface;
	struct cnfnode *cn;

	//  printf("parse_ifupdown_auto\n");

	while(*p && isspace(*p)) p++;
	while(*p && !isspace(*p)) p++; /* skip 'auto' (we know it's there...) */
	while(*p && isspace(*p)) p++;

	while(*p){
		iface = dup_next_word(&p);
		cn = create_cnfnode(iface);
		free(iface);
		append_node(cn_root, cn);
		while(*p && isspace(*p)) p++;
	}
	return cl->next;
}

static
struct confline *parse_ifupdown_iface(struct confline *cl_root, struct cnfnode *cn_root)
{
	const char *p = cl_root->line;
	char *tmp;
	struct cnfnode *cn, *cn_iface;
	struct confline *cl;
	char buf[256];
	char *stanza;

	//  printf("parse_ifupdown_iface\n");

	stanza = dup_next_word(&p);

	dup_next_word_b(&p, buf, 255);
	cn_iface = create_cnfnode(buf);
	append_node(cn_root, cn_iface);
	while(*p && isspace(*p)) p++;

	if(strcmp(stanza, "iface") == 0){
		/* if syntactically correct, children of 'iface' will be family and method */

		dup_next_word_b(&p, buf, 255);
		if(strcmp(buf, ".null") != 0){
			cn = create_cnfnode("family");
			cnfnode_setval(cn, buf);
			append_node(cn_iface, cn);
		}
		while(*p && isspace(*p)) p++;
    
		dup_next_word_b(&p, buf, 255);
		if(strcmp(buf, ".null") != 0){
			cn = create_cnfnode("method");
			cnfnode_setval(cn, buf);
			append_node(cn_iface, cn);
		}
		while(*p && isspace(*p)) p++;
	}

	free(stanza);

	for(cl = cl_root->next; cl;){
		struct cnfnode *cn_opt;
		const char *p = cl->line;

		while(*p && isspace(*p)) p++;
		if(*p && (*p != '#')){
			int i;

			/* a stanza marks the end for this iface */
			for(i = 0; stanzas[i]; i++){
				if(strncmp(p, stanzas[i], strlen(stanzas[i])) == 0)
					break;
			}
			if(stanzas[i])
				return cl;

			tmp = dup_next_word(&p);
			cn_opt = create_cnfnode(tmp);
			free(tmp);
			append_node(cn_iface, cn_opt);
			while(*p && isspace(*p)) p++;
			if(*p){
				tmp = dup_next_line(&p);
				cnfnode_setval(cn_opt, tmp);
				free(tmp);
			}
		}else if(*p == '#'){
			cn = create_cnfnode(".comment");
			cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
			append_node(cn_iface, cn);
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_iface, cn);
		}
		cl = cl->next;
	}

	return cl;
}


static
struct confline *parse_ifupdown_mapping(struct confline *cl_root, struct cnfnode *cn_root)
{
	/* for that is relevant, 'mapping' is similar to 'iface'...*/
	return parse_ifupdown_iface(cl_root, cn_root);
}

struct cnfnode *parse_ifupdown(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_top, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_top = create_cnfnode("(root)");

	for(cl = cl_root; cl;){
		const char *p = cl->line;
		char buf[256];

		while(*p && isspace(*p)) p++;
		if(*p){
			if(strncmp("auto", p, 4) == 0){
				cn = create_cnfnode("auto");
				append_node(cn_top, cn);
				cl = parse_ifupdown_auto(cl, cn);
			}else if(strncmp("allow-", p, 6) == 0){
				char buf[256];

				dup_next_word_b(&p, buf, sizeof(buf)-1);
				cn = create_cnfnode(buf);
				append_node(cn_top, cn);
				cl = parse_ifupdown_auto(cl, cn);
			}else if(strncmp("iface", p, 5) == 0){
				cn = create_cnfnode("iface");
				append_node(cn_top, cn);
				cl = parse_ifupdown_iface(cl, cn);
			}else if(strncmp("mapping", p, 7) == 0){
				cn = create_cnfnode("mapping");
				append_node(cn_top, cn);
				cl = parse_ifupdown_mapping(cl, cn);
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
	destroy_confline_list(cl_root);
	return cn_top;
}

int unparse_ifupdown(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct confline *cl_list = NULL, *cl;
	struct cnfnode *cn_top, *cn;
	char buf[256];

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		//    printf("unparse_ifupdown(), %s\n", cn_top->name);
		if(strcmp(cn_top->name, "iface") == 0){
			struct cnfnode *cn_opts;
			cn_opts = cn_top->first_child;

			if(cn_opts){
				char *iface, *family = ".null", *method = ".null";

				iface = cn_opts->name;

				for(cn = cn_opts->first_child; cn; cn = cn->next){
					if(strcmp(cn->name, "family") == 0)
						family = cn->value;
					else if(strcmp(cn->name, "method") == 0)
						method = cn->value;
				}
				snprintf(buf, 255, "iface %s %s %s\n", iface, family, method);
				cl_list = append_confline(cl_list, cl = create_confline(buf));

				for(cn = cn_opts->first_child; cn; cn = cn->next){
					if(cn->name[0] == '.'){
						snprintf(buf, 255, "%s\n", cn->value);
						cl_list =
							append_confline(cl_list,
									cl = create_confline(buf));
					}else{
						if((strcmp(cn->name, "family") != 0) &&
						   (strcmp(cn->name, "method") != 0)){
							if(cn->value)
								snprintf(buf, 255, "\t%s %s\n",
									 cn->name, cn->value);
							else
								snprintf(buf, 255, "\t%s\n",
									 cn->name);
							cl_list =
								append_confline(cl_list,
										cl = create_confline(buf));
						}
					}
				}
			}
		}else if(strcmp(cn_top->name, "auto") == 0){
			char buf1[256], *b0 = buf, *b1 = buf1, *tmp;

			snprintf(b0, 255, "auto");
			for(cn = cn_top->first_child; cn; cn = cn->next){
				snprintf(b1, 255, "%s %s", b0, cn->name);
				tmp = b0; b0 = b1; b1 = tmp;
			}
			snprintf(b1, 255, "%s\n", b0);
			cl_list = append_confline(cl_list, cl = create_confline(b1));
		}else if(strcmp(cn_top->name, "mapping") == 0){
			cn = cn_top->first_child;

			if(cn){
				char *iface;
				iface = cn->name;

				snprintf(buf, 255, "mapping %s\n", iface);
				cl_list = append_confline(cl_list, cl = create_confline(buf));
	
				for(cn = cn->first_child; cn; cn = cn->next){
					if(cn->name[0] == '.')
						snprintf(buf, 255, "%s\n", cn->value);
					else
						snprintf(buf, 255, "\t%s %s\n",
							 cn->name, cn->value);
					cl_list = append_confline(cl_list,
								  cl = create_confline(buf));
				}
			}

		}else{
			snprintf(buf, 255, "%s\n", cn_top->value);
			cl_list = append_confline(cl_list, cl = create_confline(buf));
		}
	}

	for(cl = cl_list; cl; cl = cl->next){
		fprintf(fptr, "%s", cl->line);
	}

	destroy_confline_list(cl_list);
	return 0;
}

void register_ifupdown(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

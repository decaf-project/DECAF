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
#include "ipsec.h"

static
struct cnfmodule this_module = {
	name: "ipsec",
	default_file: "/etc/ipsec.conf",
	parser: parse_ipsec,
	unparser: unparse_ipsec
};

static char *stanzas[] = {"config", "conn", "include", NULL};

static
struct confline *parse_ipsec_include(struct confline *cl_root, struct cnfnode *cn_top)
{
	const char *p = cl_root->line;
	struct confline *cl = cl_root;
	char buf[strlen(p)+1];

	while(*p && isalpha(*p)) p++;
	while(*p && isspace(*p)) p++;

	dup_next_line_b(&p, buf, sizeof(buf)-1);
	cnfnode_setval(cn_top, buf);
	cl = cl->next;

	return cl;
}

static
struct confline *parse_ipsec_stanza(struct confline *cl_root, struct cnfnode *cn_root)
{
	const char *p = cl_root->line;
	struct cnfnode *cn_name;
	struct confline *cl = cl_root;
	char buf[strlen(p)+1];
	char stanza[256];

	dup_next_word_b(&p, stanza, sizeof(stanza)-1);

	dup_next_word_b(&p, buf, sizeof(buf)-1);
	cn_name = create_cnfnode(buf);
	append_node(cn_root, cn_name);
	while(*p && isspace(*p)) p++;

	for(cl = cl_root->next; cl;){
		struct cnfnode *cn;
		struct cnfnode *cn_opt;
		const char *p = cl->line;
		char buf[strlen(p)+1];
		char *q;

		while(*p && isspace(*p)) p++;
		if(*p && (*p != '#')){
			int i;

			q = buf;
			while(*p && (isalnum(*p) || (*p == '-') || (*p == '_')) && q < buf+sizeof(buf)-1)
				*q++ = *p++;
			*q = 0;

			/* a stanza marks the end */
			for(i = 0; stanzas[i]; i++){
				if(strcmp(buf, stanzas[i]) == 0)
					return cl;
			}

			cn_opt = create_cnfnode(buf);
			append_node(cn_name, cn_opt);

			while(*p && isspace(*p)) p++;
			if(*p == '='){
				p++;
				while(*p && isspace(*p)) p++;
				if(*p){
					dup_next_line_b(&p, buf, sizeof(buf)-1);
					cnfnode_setval(cn_opt, buf);
				}
			}
		}else if(*p == '#'){
			char *tmp;
			tmp = strdup(cl->line);
			/* strip newline: */
			tmp[strlen(tmp)-1] = 0;
			cn = create_cnfnode(".comment");
			cnfnode_setval(cn, tmp);
			append_node(cn_name, cn);
			free(tmp);
		}else{
			/* an empty line marks the end */
			return cl;
		}
		cl = cl->next;
	}

	return cl;
}

struct cnfnode *parse_ipsec(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl;){
		const char *p = cl->line;
		char buf[strlen(p)+1];

		while(*p && isspace(*p)) p++;
		if(*p){
			if(strncmp("config", p, 6) == 0){
				cn = create_cnfnode("config");
				append_node(cn_root, cn);
				cl = parse_ipsec_stanza(cl, cn);
			}
			else if(strncmp("conn", p, 4) == 0){
				cn = create_cnfnode("conn");
				append_node(cn_root, cn);
				cl = parse_ipsec_stanza(cl, cn);
			}
			else if(strncmp("include", p, 7) == 0){
				cn = create_cnfnode("include");
				append_node(cn_root, cn);
				cl = parse_ipsec_include(cl, cn);
			}else{
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, sizeof(buf)-1));
				append_node(cn_root, cn);
				cl = cl->next;
			}
		}else{
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
			cl = cl->next;
		}
	}
	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_ipsec(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct confline *cl_list = NULL, *cl;
	struct cnfnode *cn_top, *cn;
	char buf[1024];

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			snprintf(buf, sizeof(buf), "%s\n", cn_top->value);
			cl_list = append_confline(cl_list, cl = create_confline(buf));
		}else if(strcmp(cn_top->name, "include") != 0){
			cn = cn_top->first_child;

			if(cn){
				snprintf(buf, sizeof(buf), "%s %s\n", cn_top->name, cn->name);
				cl_list = append_confline(cl_list, cl = create_confline(buf));
	
				for(cn = cn->first_child; cn; cn = cn->next){
					if(cn->name[0] == '.')
						snprintf(buf, sizeof(buf), "%s\n", cn->value);
					else
						snprintf(buf, sizeof(buf), "\t%s=%s\n",
							 cn->name, cn->value ? cn->value : "");
					cl_list = append_confline(cl_list,
								  cl = create_confline(buf));
				}
			}

		}else{
			snprintf(buf, sizeof(buf), "%s %s\n", cn_top->name, cn_top->value);	
			cl_list = append_confline(cl_list, cl = create_confline(buf));
		}
	}

	for(cl = cl_list; cl; cl = cl->next){
		fprintf(fptr, "%s", cl->line);
	}

	return 0;
}

void register_ipsec(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

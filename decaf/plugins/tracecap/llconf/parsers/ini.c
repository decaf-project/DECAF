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
#include "ini.h"

static
struct cnfmodule this_module = {
	name: "ini",
	parser: parse_ini,
	unparser: unparse_ini,
	default_file: NULL,
	opt_root: NULL
};

static
int parse_ini_options(struct cnfnode *opt_root, int *cmt_char)
{
	struct cnfnode *cn;

	if(opt_root == NULL) return -1;
	if(opt_root->first_child == NULL) return -1;

	for(cn = opt_root->first_child; cn; cn = cn->next){
		if(strcmp(cn->name, "comment") == 0){
			*cmt_char = (unsigned char)(cn->value[0]);
		}
	}

	return 0;
}

static
struct confline *parse_ini_subsection(struct confline *cl, struct cnfnode *cn, int cmt_char)
{
	const char *p;
	char buf[1024];
  
	for(cl = cl->next; cl; cl = cl->next){
		struct cnfnode *cn_sub;
    
		p = cl->line;
		while(*p && isspace(*p)) p++;
    
		if(*p == '}') break;

		if(!*p){
			cn_sub = create_cnfnode(".empty");
			p = cl->line;
			dup_next_line_b(&p, buf, sizeof(buf)-1);
			cnfnode_setval(cn_sub, buf);
			append_node(cn, cn_sub);
		}else if(*p == cmt_char){
			cn_sub = create_cnfnode(".comment");
			p = cl->line;
			dup_next_line_b(&p, buf, sizeof(buf)-1);
			cnfnode_setval(cn_sub, buf);
			append_node(cn, cn_sub);
		}else{
			char *q = buf;

			while(*p &&
			      (!isspace(*p) && (*p != '=')) &&
			      q < buf+sizeof(buf)-1)
				*(q++) = *(p++);
			*q = 0;

			while(*p && isspace(*p)) p++;
			if(*p == '='){
				p++;
				while(*p && isspace(*p)) p++;
				
				cn_sub = create_cnfnode(buf);
				
				if(*p != '{'){
					q = buf;
					while(*p && q < buf+sizeof(buf)-1)
						*q++ = *p++;
					if(q > buf)
						while(isspace(q[-1])) q--; /* trim trailing spaces */
					*q = 0;
					cnfnode_setval(cn_sub, buf);
				}else{
					p++;
					while(*p && isspace(*p)) p++;
					if(*p != '}'){
						/* recursion: */
						cl = parse_ini_subsection(cl, cn_sub, cmt_char);
					}
				}
				append_node(cn, cn_sub);
			}else{
				cn_sub = create_cnfnode(".unparsed");
				p = cl->line;
				dup_next_line_b(&p, buf, sizeof(buf)-1);
				cnfnode_setval(cn_sub, buf);
				append_node(cn, cn_sub);
			}
		}
	}
	return cl;
}

static
struct confline *parse_ini_section(struct confline *cl_root, struct cnfnode *cn_root, int cmt_char)
{
	struct cnfnode *cn;
	struct confline *cl;

	cl = cl_root->next;
  
	while(cl){
		const char *p = cl->line;
		char buf[1024];
    
		while(*p && isspace(*p)) p++;
		if(*p){
			if(*p == '['){
				return cl;
			}else if(*p == cmt_char){
				p = cl->line;
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, sizeof(buf)-1));
				append_node(cn_root, cn);
			}else{
				char *q = buf;

				while(*p &&
				      (!isspace(*p) && (*p != '=')) &&
				      q < buf+sizeof(buf)-1)
					*(q++) = *(p++);
				*q = 0;

				while(*p && isspace(*p)) p++;
				if(*p == '='){
					p++;
					while(*p && isspace(*p)) p++;

					cn = create_cnfnode(buf);

					if(*p != '{'){
						dup_next_line_b(&p, buf, sizeof(buf)-1);
						cnfnode_setval(cn, buf);
					}else{
						p++;
						while(*p && isspace(*p)) p++;
						if(*p != '}'){
							/* recursion: */
							cl = parse_ini_subsection(cl, cn, cmt_char);
						}
					}
					append_node(cn_root, cn);

					while(*p && isspace(*p)) p++;
					if(*p == cmt_char){
						p = cl->line;
						cn = create_cnfnode(".comment");
						cnfnode_setval(cn, dup_next_line_b(&p, buf, sizeof(buf)-1));
						append_node(cn_root, cn);
					}
				}else{
					cn = create_cnfnode(".unparsed");
					p = cl->line;
					dup_next_line_b(&p, buf, sizeof(buf)-1);
					cnfnode_setval(cn, buf);
					append_node(cn_root, cn);
				}
			}/* else */
		}else{ /* if(*p) */
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
		}
		cl = cl ? cl->next : cl;
	} /* while(cl) */;

	return cl;
}

struct cnfnode *parse_ini(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_top, *cn;
	struct confline *cl, *cl_root;
	int cmt_char = '#';

	cl_root = read_conflines(fptr);

	cn_top = create_cnfnode("(root)");

	if(cm && cm->opt_root){
		parse_ini_options(cm->opt_root, &cmt_char);
	}

	for(cl = cl_root; cl;){
		const char *p = cl->line;
		char buf[1024];

		while(*p && isspace(*p)) p++;
		if(*p){
			if(*p == '['){
				char *q = buf;

				p++;
				while(*p && isspace(*p)) p++;
				while(*p && (*p != ']') && q < buf+sizeof(buf)-1) *(q++) = *(p++);
				*q = 0;

				cn = create_cnfnode(buf);
				append_node(cn_top, cn);

				cl = parse_ini_section(cl, cn, cmt_char);
			}else{
				p = cl->line;
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, sizeof(buf)-1));
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

static
struct confline *unparse_ini_subsection(struct cnfnode *cn, struct confline *cl_list, int level)
{
	char buf[1024], ident[1024];
	struct confline *cl;
	struct cnfnode *cn_line;
	int i;

	for(i = 0; (i < level*8) && (i < sizeof(buf)-1); i++){
		ident[i] = ' ';
	}
	ident[i] = 0;

	snprintf(buf, sizeof(buf), "%s%s = {\n", ident, cn->name);
	cl_list = append_confline(cl_list, cl = create_confline(buf));
  
	for(cn_line = cn->first_child; cn_line; cn_line = cn_line->next){
		if(cn_line->value){
			if((strcmp(cn_line->name, ".empty") == 0) || (strcmp(cn_line->name, ".comment") == 0)
				|| (strcmp(cn_line->name, ".unparsed") == 0))
				snprintf(buf, sizeof(buf), "%s\n", cn_line->value);
			else{
				snprintf(buf, sizeof(buf),
					 "        %s%s = %s\n", ident, cn_line->name, cn_line->value);
			}
			cl_list = append_confline(cl_list, cl = create_confline(buf));
		}else{
			cl_list = unparse_ini_subsection(cn_line, cl_list, level+1);
		}
	}

	snprintf(buf, sizeof(buf), "%s}\n", ident);
	cl_list = append_confline(cl_list, create_confline(buf));

	return cl_list;
} 

int unparse_ini(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct confline *cl_list = NULL, *cl;
	struct cnfnode *cn_section;
	char buf[1024];

	for(cn_section = cn_root->first_child; cn_section; cn_section = cn_section->next){
		if(cn_section->name[0] == '.'){
			snprintf(buf, sizeof(buf), "%s\n", cn_section->value);
			cl_list = append_confline(cl_list, cl = create_confline(buf));
		}else{
			struct cnfnode *cn_line;

			snprintf(buf, sizeof(buf), "[%s]\n", cn_section->name);
			cl_list = append_confline(cl_list, cl = create_confline(buf));

			for(cn_line = cn_section->first_child; cn_line; cn_line = cn_line->next){
				if((strcmp(cn_line->name, ".comment") == 0) ||
				   (strcmp(cn_line->name, ".empty") == 0) ||
				   (strcmp(cn_line->name, ".unparsed") == 0)){
					snprintf(buf, sizeof(buf), "%s\n", cn_line->value);
					cl_list = append_confline(cl_list,
								  cl = create_confline(buf));
				}else{
					if(cn_line->value){
						snprintf(buf, sizeof(buf), "        %s = %s\n",
							 cn_line->name, cn_line->value ? cn_line->value : "");
						cl_list =
							append_confline(cl_list,
									cl = create_confline(buf));
					}else{
						cl_list =
							unparse_ini_subsection(cn_line, cl_list, 1);
					}
				}
			}
		}
	}

	for(cl = cl_list; cl; cl = cl->next){
		fprintf(fptr, "%s", cl->line);
	}
	destroy_confline_list(cl_list);

	return 0;
}

void register_ini(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

struct cnfmodule *clone_cnfmodule_ini(struct cnfnode *opt_root)
{
	return clone_cnfmodule(&this_module, NULL, NULL, opt_root);
}


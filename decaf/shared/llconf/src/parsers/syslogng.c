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

#include "syslogng.h"

static
struct cnfmodule this_module = {
	name: "syslog-ng",
	parser: parse_syslogng,
	unparser: unparse_syslogng,
};

static
struct confline *next_char(struct confline *cl, const char **pp)
{
	const char *p = *pp;

	p++;
	if(!*p || *p == '#'){
		cl = cl->next;
		if(cl) p = cl->line;
	}
	*pp = p;
	return cl;
}

static
struct confline *_skip_spaces(struct confline *cl, const char **pp)
{
	const char *p = *pp;

	while(*p && (isspace(*p) || (*p == '#'))){
		while(*p && isspace(*p)) p++;
		while((!*p || (*p == '#')) && cl->next){
			cl = cl->next;
			p = cl->line;
		}
	}
	*pp = p;
	return cl;
}

static
struct confline *parse_syslogng_items(struct cnfnode *cn_top, 
				      struct confline *cl, const char **pp)
{
	struct cnfnode *cn;
	char buf[256], *q;
	const char *p = *pp;

	while(1){
		q = buf;
		while(*p && (isalnum(*p) || (*p == '_') || (*p == '-')))
			*q++ = *p++;
		*q = 0;	

		append_node(cn_top, cn = create_cnfnode(buf));

		cl = _skip_spaces(cl, &p);

		if(*p == '('){
			p++;
			cl = _skip_spaces(cl, &p);
			q = buf;
			if((*p == '\"' || *p == '\'') && (p[-1] != '\\')){
				char qc = *p;
				*q++ = *p++;
				while(*p && ((*p != qc) || (p[-1] == '\\'))&& q < buf+sizeof(buf)-1){
					*q++ = *p;
					cl = next_char(cl, &p);
				}
				if(*p == qc) *q++ = *p++;
			}else{
				while(*p && !isspace(*p) && *p != '(' && *p != ')' &&
				      q < buf+sizeof(buf)-1){
					*q++ = *p;
					cl = next_char(cl, &p);
				}
			}
			*q = 0;
			cl = _skip_spaces(cl, &p);
			if(*p == ')') p++;
			cnfnode_setval(cn, buf);
			cl = _skip_spaces(cl, &p);
		}
		if(*p == ')'){
			p++;
			break;
		}
	}
	*pp = p;
	return cl;
}

static
struct confline *parse_syslogng_options(struct cnfnode *cn_top, struct confline *cl, const char *p)
{
	struct cnfnode *cn;
	char buf[256], *q;

	cl = _skip_spaces(cl, &p);

	if(*p == '{'){
		p++;
		while(1){
			cl = _skip_spaces(cl, &p);

			q = buf;
			while(*p && (isalnum(*p) || (*p == '_') || (*p == '-')))
				*q++ = *p++;
			*q = 0;

			append_node(cn_top, cn = create_cnfnode(buf));

			cl = _skip_spaces(cl, &p);
			
			if(*p == '('){
				p++;
				cl = _skip_spaces(cl, &p);
				q = buf;
				if((*p == '\"' || *p == '\'') && (p[-1] != '\\')){
					char qc = *p;
					*q++ = *p++;
					while(*p && ((*p != qc) || (p[-1] == '\\'))&& q < buf+sizeof(buf)-1){
						*q++ = *p;
						cl = next_char(cl, &p);
					}
					if(*p == qc) *q++ = *p++;
					*q = 0;
				}else{
					while(*p && !isspace(*p) && *p != '(' && *p != ')' &&
					      q < buf+sizeof(buf)-1){
						*q++ = *p;
						cl = next_char(cl, &p);
					}
					*q = 0;
				}
				cnfnode_setval(cn, buf);
				cl = _skip_spaces(cl, &p);
				if(*p == ')') p++;
				else{
					cl = parse_syslogng_items(cn, cl, &p);
				}
				cl = _skip_spaces(cl, &p);
			}
			if(*p == ';') p++;
			cl = _skip_spaces(cl, &p);
			if(*p == '}'){
				p++;
				cl = _skip_spaces(cl, &p);
				break;
			}else if(!*p)
				break;
		}
	}
	return cl;
}

static
struct confline *parse_syslogng_filter(struct cnfnode *cn_top, struct confline *cl, const char *p)
{
	char buf[256], *q;

	cl = _skip_spaces(cl, &p);

	if(*p == '{'){
		p++;
		cl = _skip_spaces(cl, &p);

		q = buf;
		while(*p && *p != ';' &&
		      q < buf+sizeof(buf)-1){
			*q++ = *p;
			cl = next_char(cl, &p);
		}
		*q++ = *p; /* should be the ';' */
		*q = 0;
		cnfnode_setval(cn_top, buf);
		cl = _skip_spaces(cl, &p); /* get to the end of the filter line; p == '}' */
	}
	return cl;
}

struct cnfnode *parse_syslogng(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_top, *cn_root;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl;){
		const char *p = cl->line;
		char buf[256];

		cl = _skip_spaces(cl, &p);
		if(!*p) break;
		if(*p){
			dup_next_word_b(&p, buf, sizeof(buf)-1);

			append_node(cn_root, cn_top = create_cnfnode(buf));

			if((strcmp(buf, "options") == 0) || (strcmp(buf, "log") == 0)){
				cl = parse_syslogng_options(cn_top, cl, p);
			}else if((strcmp(buf, "source") == 0) ||
				 (strcmp(buf, "destination") == 0)){
				struct cnfnode *cn_top1;

				cl = _skip_spaces(cl, &p);
				dup_next_word_b(&p, buf, sizeof(buf)-1);
				append_node(cn_top, cn_top1 = create_cnfnode(buf));
				cl = parse_syslogng_options(cn_top1, cl, p);
			}else if(strcmp(buf, "filter") == 0){
				struct cnfnode *cn_top1;

				cl = _skip_spaces(cl, &p);
				dup_next_word_b(&p, buf, sizeof(buf)-1);
				append_node(cn_top, cn_top1 = create_cnfnode(buf));
				cl = parse_syslogng_filter(cn_top1, cl, p);
			}
			if(cl) cl = cl->next;
		}
	}
	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_syslogng(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top, *cn;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if((strcmp(cn_top->name, "options") == 0) ||
		   (strcmp(cn_top->name, "log") == 0)){
			fprintf(fptr, "%s { ", cn_top->name);
			for(cn = cn_top->first_child; cn; cn = cn->next){
				fprintf(fptr, "%s(%s); ", cn->name, cn->value ? cn->value : "");
			}
			fprintf(fptr, "};\n");
		}else if((strcmp(cn_top->name, "source") == 0) ||
			 (strcmp(cn_top->name, "destination") == 0)
			){
			struct cnfnode *cn_top1, *cn_opt;

			cn_top1 = cn_top->first_child;
			if(cn_top1){
				fprintf(fptr, "%s %s { ", cn_top->name, cn_top1->name);
				for(cn = cn_top1->first_child; cn; cn = cn->next){
					fprintf(fptr, "%s(%s", cn->name, cn->value ? cn->value : "");
					for(cn_opt = cn->first_child; cn_opt; cn_opt = cn_opt->next){
						fprintf(fptr, " %s(%s)", cn_opt->name, cn_opt->value ? cn_opt->value : "");
					}
					fprintf(fptr, "); ");
				}
				fprintf(fptr, "};\n");
			}
		}else if(strcmp(cn_top->name, "filter") == 0){
			struct cnfnode *cn_top1;

			cn_top1 = cn_top->first_child;
			if(cn_top1)
				fprintf(fptr, "%s %s { %s };\n", cn_top->name, cn_top1->name, cn_top1->value ? cn_top1->value : "");
		}
	}
	return 0;
}

void register_syslogng(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

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
#include "shell.h"
#include "table.h"

#include "conserver.h"

static
struct cnfmodule this_module = {
	name: "conserver",
	default_file: "/etc/conserver.cf",
	parser: parse_conserver,
	unparser: unparse_conserver
};

/*
from:  
http://www.fifi.org/cgi-bin/man2html/usr/share/man/man4/conserver.cf.4.gz

SYNOPSIS

LOGDIR=logdirectory
TIMESTAMP=timestamp-spec
BREAKn=break-spec
name:device[@conserver]:baud:logfile:timestamp-spec:break
name:!termserver[@conserver]:port:logfile:timestamp-spec:break
name:|command[@conserver]::logfile:timestamp-spec:break
%%
access: hosts  

*/

static char *cols[] = {"name", "device", "baud", "logfile", "timestamp-spec", "break"};

struct cnfnode *parse_conserver(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn_server = NULL;
	struct confline *cl, *cl_root;
	int ncols = sizeof(cols)/sizeof(char *);
	int got_percents = 0;
		
	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		char buf[256];
		const char *p = cl->line;
		struct cnfnode *cn_top;

		if((strncmp(p, "TIMESTAMP", 9) == 0) || (strncmp(p, "LOGDIR", 6) == 0) || (strncmp(p, "BREAK", 5) == 0)){
			if((cn_top = parse_shell_line(&p)))
				append_node(cn_root, cn_top);
		}else if(strncmp(p, "%%", 2) == 0){
			got_percents = 1;
		}else if(*p == '#'){
			cn_top = create_cnfnode(".comment");
			cnfnode_setval(cn_top, dup_next_line_b(&p, buf, 255));
			append_node(cn_root, cn_top);
		}else if(!*p){
			cn_top = create_cnfnode(".empty");
			cnfnode_setval(cn_top, "");
			append_node(cn_root, cn_top);
		}else if(!got_percents){
			if(!cn_server){
				cn_server = create_cnfnode("server");
				append_node(cn_root, cn_server);
			}
			
			if((cn_top = parse_table_line(&p, cols, ncols, ':')))
				append_node(cn_server, cn_top);
		}else if(got_percents){
			char buf[256];

			dup_line_until_b(&p, ':', buf, sizeof(buf)-1);
			cn_top = create_cnfnode(buf);

			if(*p == ':') p++;
			while(*p && isspace(*p)) p++;
			dup_next_line_b(&p, buf,  sizeof(buf)-1);
			cnfnode_setval(cn_top, buf);

			append_node(cn_root, cn_top);
		}
	}

	destroy_confline_list(cl_root);
	return cn_root;
}
			
int unparse_conserver(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;
	int ncols = sizeof(cols)/sizeof(char *);
	int done_server = 0;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s\n", cn_top->value);
		}else if(strcmp(cn_top->name, "server") != 0){
			if(!done_server){
				fprintf(fptr, "%s=%s\n", cn_top->name, cn_top->value);
			}else{
				fprintf(fptr, "%s: %s\n", cn_top->name, cn_top->value);
			}
		}else{
			struct cnfnode *cn;

			for(cn = cn_top->first_child; cn; cn = cn->next)
				unparse_table_line(fptr, cn, cols, ncols, ':');

			fprintf(fptr, "%%%%\n"); /* prints "%%" */
			done_server = 1;
		}
	}

	return 0;
}

void register_conserver(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

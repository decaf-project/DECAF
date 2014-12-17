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
#include "route.h"

static
struct cnfmodule this_module = {
	name: "route",
	default_file: "/etc/network/st_routes",
	parser: parse_route,
	unparser: unparse_route
};

struct route_options
{
	char *name;
	int nparams;
};

static struct route_options route_options[] =
{
	{ "gw", 1},
	{ "dev", 1},
	{ "metric", 1},
	{ "mss", 1},
	{ "window", 1},
	{ "irtt", 1},
	{ "reject", 0},
	{ "mod", 0},
	{ "dyn", 0},
	{ "reinstate", 0},
};

static
const char *parse_route_options(struct cnfnode *cn_top, const char *p)
{
	/* not yet */
	return p;
}

struct cnfnode *parse_route(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn_top, *cn;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		const char *p = cl->line;
		char buf[256], *q;
 		char target[256], netmask[256];
		int i;

		while(*p && isspace(*p)) p++;
		if(*p){
			if((strncmp("route", p, 5) == 0) || 
				(strncmp("/sbin/route", p, 11) == 0)) {

				cn_top = create_cnfnode("route");
				append_node(cn_root, cn_top);

				if (*p == '/') {
					p += 11;
				} else {
					p += 5;
				}
				while(*p && isspace(*p)) p++;
				if(!*p) continue;

				p = parse_route_options(cn_top, p);
				if(!*p) continue;

				if(strncmp(p, "add", 3) == 0){
					cn = create_cnfnode("add");
					append_node(cn_top, cn);
					p += 3;
				}else if(strncmp(p, "del", 3) == 0){
					cn = create_cnfnode("del");
					append_node(cn_top, cn);
					p += 3;
				}

				while(*p && isspace(*p)) p++;
				if(!*p) continue;
	
				if(strncmp(p, "-net", 4) == 0){
					cn = create_cnfnode("-net");
					append_node(cn_top, cn);
					p += 4;
				}else if(strncmp(p, "-host", 5) == 0){
					cn = create_cnfnode("-host");
					append_node(cn_top, cn);
					p += 5;
				}

				while(*p && isspace(*p)) p++;
				if(!*p) continue;

				/* get target */
				q = target;
				while(*p && !isspace(*p) && (q < target+255)) *q++ = *p++;
				*q = 0;
	
				while(*p && isspace(*p)) p++;
				if(!*p) continue;
				cn = create_cnfnode("target");
				append_node(cn_top, cn);
				cnfnode_setval(cn, target);

				netmask[0] = 0;
				if(strncmp(p, "netmask", 7) == 0){
					p += 7;
					while(*p && isspace(*p)) p++;
					if(!*p) continue;

					q = netmask;
					while(*p && !isspace(*p) && (q < netmask+255))
						*q++ = *p++;
					*q = 0;

					while(*p && isspace(*p)) p++;
					if(!*p) continue;
				}

				cn = create_cnfnode("netmask");
				append_node(cn_top, cn);
				cnfnode_setval(cn, netmask);
				//if(netmask[0])
				//	snprintf(buf, 256, "%s/%s", target, netmask);
				//else
				//	snprintf(buf, 256, "%s", target);
				//
				//cnfnode_setname(cn_top, buf);

				for(i = 0;
				    i < sizeof(route_options)/sizeof(struct route_options);
				    i++){
					if(strncmp(p, route_options[i].name,
						   strlen(route_options[i].name)) == 0){
						int j;
	    
						cn = create_cnfnode(route_options[i].name);
						append_node(cn_top, cn);
						p += strlen(route_options[i].name);
						while(*p && isspace(*p)) p++;
						if(!*p) break;
	    
						for(j = 0; j < route_options[i].nparams; j++){
							q = buf;
							while(*p && !isspace(*p) &&
							      (q < buf+255)) *q++ = *p++;
							*q = 0;
							cnfnode_setval(cn, buf);
	      
							while(*p && isspace(*p)) p++;
							if(!*p) break;
						}
						if(!*p) break;
					}
				}
				if(!*p) continue;
	
				while(*p && isspace(*p)) p++;
				if(!*p) continue;
	
				q = buf;
				while(*p && !isspace(*p) && (q < buf+255)) *q++ = *p++;
				*q = 0;
				cn = create_cnfnode("dev");
				append_node(cn_top, cn);
				cnfnode_setval(cn, buf);

			}else if(*p == '#'){
				cn = create_cnfnode(".comment");
				cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));
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

int unparse_route(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top, *cn;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] != '.'){
			int i;
			//char buf[256], *q;
			//const char *p;
      
			fprintf(fptr, "/sbin/route");
      
			if((cn = find_node(cn_top->first_child, "add")))
				fprintf(fptr, " add");
			if((cn = find_node(cn_top->first_child, "del")))
				fprintf(fptr, " del");

			if((cn = find_node(cn_top->first_child, "-host")))
				fprintf(fptr, " -host");
			if((cn = find_node(cn_top->first_child, "-net")))
				fprintf(fptr, " -net");
      
			//p = cn_top->name;
			//q = buf;
			//while(*p && *p != '/')
			//	*q++ = *p++;
			//*q = 0;
			//fprintf(fptr, " %s", buf);
			//
			//if(*p){
			//	p++;
			//	fprintf(fptr, " netmask %s", p);
			//}

			if((cn = find_node(cn_top->first_child, "target")) && 
				*(cn->value))
				fprintf(fptr, " %s", cn->value);
      
			if((cn = find_node(cn_top->first_child, "netmask")) && 
				*(cn->value))
				fprintf(fptr, " netmask %s", cn->value);
      
			for(i = 0; i < sizeof(route_options)/sizeof(struct route_options); i++){
				if((cn = find_node(cn_top->first_child, route_options[i].name))){
					if(cn->value){
						fprintf(fptr, " %s %s", cn->name, cn->value);
					}else{
						fprintf(fptr, " %s", cn->name);
					}
				}
			}

			fprintf(fptr, "\n");

		}else{
			fprintf(fptr, "%s\n", cn_top->value);
		} /* if */
	} /* for */

	return 0;
}

void register_route(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

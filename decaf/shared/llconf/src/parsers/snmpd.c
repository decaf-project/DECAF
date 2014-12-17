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
#include "pair.h"
#include "table.h"
#include "options.h"
#include "snmpd.h"

/** @file snmpd.c 
 * Parser module for configuration in the style:
 * "variable value"
 * This file also contains a few functions that can be used for other parsers.
 */

static
struct cnfmodule this_module = {
	name: "snmpd",
	default_file: NULL,
	parser: parse_snmpd,
	unparser: unparse_snmpd
};

static
struct cnfnode *parse_snmp_table(const char *name, const char *p, char **cols, int ncols)
{
	struct cnfnode *cn_table, *cn_top;

	cn_top = create_cnfnode(name);
	skip_spaces(&p);
	cn_table = parse_table_line(&p, cols, ncols, -1);
	
	append_node(cn_top, cn_table);

	return cn_top;
}

static
int unparse_snmp_table(FILE *fptr, struct cnfnode *cn_top, char **cols, int ncols)
{
	fprintf(fptr, "%s", cn_top->name);
	if(cn_top->first_child){
		fprintf(fptr, "\t");
		unparse_table_line(fptr, cn_top->first_child, cols, ncols, -1);
	}
	return 0;
}

struct cnfnode *parse_snmpd(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root, *cn = NULL;
	struct confline *cl, *cl_root;

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	for(cl = cl_root; cl; cl = cl->next){
		char buf[256];
		const char *p = cl->line, *p0;

		while(*p && isspace(*p)) p++;
		if(*p){
			if(*p != '#'){
				struct cnfnode *cn_top;

				p0 = p;
				dup_next_word_b(&p, buf, sizeof(buf)-1);

				if((strcmp(buf, "rouser") == 0) || (strcmp(buf, "rwuser") == 0)){
					char *cols[] = { "", "sec_level", "oid" };
					
					cn_top = parse_snmp_table(buf, p, cols, sizeof(cols)/sizeof(char *));

					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "rocommunity") == 0) || (strcmp(buf, "rwcommunity") == 0)
					 || (strcmp(buf, "rocommunity6") == 0) || (strcmp(buf, "rwcommunity6") == 0)){
					char *cols[] = { "", "source", "oid" };
					
					cn_top = parse_snmp_table(buf, p, cols, sizeof(cols)/sizeof(char *));

					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "group") == 0)){
					char *cols[] = { "", "sec_model", "sec_name" };
					
					cn_top = parse_snmp_table(buf, p, cols, sizeof(cols)/sizeof(char *));

					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "view") == 0)){
					char *cols[] = { "", "incl_excl", "subtree" , "mask"};
					
					cn_top = parse_snmp_table(buf, p, cols, sizeof(cols)/sizeof(char *));

					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "access") == 0)){
					char *cols[] = { "", "match", "read", "write", "notif"};
					struct cnfnode *cn_name, *cn_context, *cn_model;

					cn_top = create_cnfnode(buf);
					skip_spaces(&p);

					if(*p){
						dup_next_word_b(&p, buf, sizeof(buf));
						skip_spaces(&p);
						cn_name = create_cnfnode(buf);
						skip_spaces(&p);

						if(*p){
							dup_next_word_b(&p, buf, sizeof(buf));
							skip_spaces(&p);
							cn_context = create_cnfnode(buf);
							skip_spaces(&p);

							if(*p){
								dup_next_word_b(&p, buf, sizeof(buf));
								skip_spaces(&p);

								if(*p){
									cn_model =
										parse_snmp_table(buf, p, cols, sizeof(cols)/sizeof(char *));
									append_node(cn_context, cn_model);
								}
							}
							append_node(cn_name, cn_context);
						}
						append_node(cn_top, cn_name);
					}
					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "createUser") == 0)){
					char *cols[] = { "", "authmethod", "authpass", "cryptmethod", "cryptpass" };
					
					cn_top = parse_snmp_table(buf, p, cols, sizeof(cols)/sizeof(char *));

					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "exec") == 0) ||(strcmp(buf, "disk") == 0)){
					struct cnfnode *cn_name;

					cn_top = create_cnfnode(buf);
					skip_spaces(&p);
					cn_name = create_cnfnode(dup_next_word_b(&p, buf, sizeof(buf)));
					cnfnode_setval(cn, dup_next_line_b(&p, buf, 255));

					append_node(cn_top, cn_name);
					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "proxy") == 0)){
					struct cnfnode *cn_nonopt;

					p = p0;
					/* taken from net-snmp-5.1.2/snmplib/snmp_parse_args.c, and added 'C:' */
					cn_top = parse_cmdline(&p, "Y:VhHm:M:O:I:P:D:dv:r:t:c:Z:e:E:n:u:l:x:X:a:A:p:T:-:3:s:S:L:C:");

					skip_spaces(&p);
					if(*p){
						dup_next_word_b(&p, buf, sizeof(buf));
						cn_nonopt = create_cnfnode("host");
						cnfnode_setval(cn_nonopt, buf);
						append_node(cn_top, cn_nonopt);
					}
					skip_spaces(&p);
					if(*p){
						dup_next_word_b(&p, buf, sizeof(buf));
						cn_nonopt = create_cnfnode("oid");
						cnfnode_setval(cn_nonopt, buf);
						append_node(cn_top, cn_nonopt);
					}
					skip_spaces(&p);
					if(*p){
						dup_next_word_b(&p, buf, sizeof(buf));
						cn_nonopt = create_cnfnode("remote_oid");
						cnfnode_setval(cn_nonopt, buf);
						append_node(cn_top, cn_nonopt);
					}

					append_node(cn_root, cn_top);
				}else if((strcmp(buf, "com2sec") == 0)){
					struct cnfnode *cn_top;

					cn_top = create_cnfnode(buf);
					skip_spaces(&p);

					if(*p){
						const char *nodes[] = {"name", "source", "community" };
						int i;

						while(*p == '-'){
							struct cnfnode *cn_opt;

//							printf("option\n");
							dup_next_word_b(&p, buf, sizeof(buf));
							cn_opt = create_cnfnode(buf);
							skip_spaces(&p);
							if(*p){
								dup_next_word_b(&p, buf, sizeof(buf));
								cnfnode_setval(cn_opt, buf);
							}
							append_node(cn_top, cn_opt);
							skip_spaces(&p);
						}
						for(i = 0; *p && (i < 3); i++){
							struct cnfnode *cn_node;

							dup_next_word_b(&p, buf, sizeof(buf));
							cn_node = create_cnfnode(nodes[i]);
							cnfnode_setval(cn_node, buf);
							append_node(cn_top, cn_node);
							skip_spaces(&p);
						}
						append_node(cn_root, cn_top);
					}
				}else{
					p = p0;
					cn = parse_pair_line(&p);
					if(cn)
						append_node(cn_root, cn);
				}
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

int unparse_snmpd(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		if(cn_top->name[0] == '.'){
			fprintf(fptr, "%s\n", cn_top->value);
		}else{
			const char *name = cn_top->name;

			if((strcmp(name, "rouser") == 0) || (strcmp(name, "rwuser") == 0)){
				char *cols[] = { "", "sec_level", "oid" };
				unparse_snmp_table(fptr, cn_top, cols, sizeof(cols)/sizeof(char *));
			}else if((strcmp(name, "rocommunity") == 0) || (strcmp(name, "rwcommunity") == 0)
				 || (strcmp(name, "rocommunity6") == 0) || (strcmp(name, "rwcommunity6") == 0)){
				char *cols[] = { "", "source", "oid" };
				unparse_snmp_table(fptr, cn_top, cols, sizeof(cols)/sizeof(char *));
			}else if((strcmp(name, "group") == 0)){
				char *cols[] = { "", "sec_model", "sec_name" };
				unparse_snmp_table(fptr, cn_top, cols, sizeof(cols)/sizeof(char *));
			}else if((strcmp(name, "view") == 0)){
				char *cols[] = { "", "incl_excl", "subtree" , "mask"};
				unparse_snmp_table(fptr, cn_top, cols, sizeof(cols)/sizeof(char *));
			}else if((strcmp(name, "access") == 0)){
				char *cols[] = { "", "match", "read", "write", "notif"};
				struct cnfnode *cn_name, *cn_context, *cn_model;

				if((cn_name = cn_top->first_child)){
					if((cn_context = cn_name->first_child)){
						if((cn_model = cn_context->first_child)){
							fprintf(fptr, "access\t%s\t%s",
								cnfnode_getname(cn_name),
								cnfnode_getname(cn_context));

							fprintf(fptr, "\t");
							unparse_snmp_table(fptr, cn_model, cols,
									   sizeof(cols)/sizeof(char *));
						}
					}
				}
			}else if((strcmp(name, "createUser") == 0)){
				char *cols[] = { "", "name", "authmethod", "authpass", "cryptmethod", "cryptpass" };
				unparse_snmp_table(fptr, cn_top, cols, sizeof(cols)/sizeof(char *));
			}else if((strcmp(name, "createUser") == 0)){
				// TODO
			}else if((strcmp(name, "proxy") == 0)){
				struct cnfnode *cn;

				fprintf(fptr, "%s ", cn_top->name);
				for(cn = cn_top->first_child; cn; cn = cn->next){
					if(cn->name[0] == '-')
						fprintf(fptr, "%s %s%s", cn->name, cn->value, cn->next ? " " : "\n");
					else
						fprintf(fptr, "%s%s", cn->value, cn->next ? " " : "\n");
				}

			}else if((strcmp(name, "com2sec") == 0)){
				struct cnfnode *cn;
				const char *nodes[] = {"name", "source", "community" };
				int i;

				fprintf(fptr, "%s", cn_top->name);

				for(cn = cn_top->first_child; cn; cn = cn->next){
					if(cnfnode_getname(cn)[0] == '-'){
						fprintf(fptr, " %s %s", cnfnode_getname(cn), cnfnode_getval(cn));
					}
				}
				for(i = 0; i < 3; i++){
					cn = find_node(cn_top->first_child, nodes[i]);
					if(cn)
						fprintf(fptr, " %s", cnfnode_getval(cn));
				}

				fprintf(fptr, "\n");

			}else
				unparse_pair_line(fptr, cn_top);
		}
	}

	return 0;
}

void register_snmpd(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

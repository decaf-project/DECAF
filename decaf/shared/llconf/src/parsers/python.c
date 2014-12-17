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

#include "python.h"

static
struct cnfmodule this_module = {
	name: "python",
	parser: NULL,
	unparser: unparse_python
};

static
int _unparse_python(FILE *fptr, struct cnfnode *cn_root, int level)
{
	struct cnfnode *cn_top;

	for(cn_top = cn_root->first_child; cn_top; cn_top = cn_top->next){
		int i;
			
		for(i = 0; i < level; i++) fputc('\t', fptr);

		if(!cnfnode_getval(cn_top)){
			char *key = dup_quote_string(cnfnode_getname(cn_top), '\'');
			fprintf(fptr, "%s: {\n", key);
			free(key);
			_unparse_python(fptr, cn_top, level+1);
			for(i = 0; i < level; i++) fputc('\t', fptr);
			fprintf(fptr, "}%s\n", (cn_top->next ? "," : ""));
		}else{
			if(cnfnode_getname(cn_top)[0] != '.'){
				char *key = dup_quote_string(cnfnode_getname(cn_top), '\'');
				char *val = dup_quote_string(cnfnode_getval(cn_top), '\'');
				fprintf(fptr, "%s: %s%s\n", key, val, (cn_top->next ? "," : ""));
				free(val);
				free(key);
			}else
				fprintf(fptr, "%s\n", cnfnode_getval(cn_top));
		}
	}
	return 0;
}

int unparse_python(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	fprintf(fptr, "cn_tree = {\n");
	_unparse_python(fptr, cn_root, 1);
	fprintf(fptr, "}\n");

	return 0;
}

void register_python(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}

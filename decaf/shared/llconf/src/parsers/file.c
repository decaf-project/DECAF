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
#include "file.h"

static
struct cnfmodule this_module = {
	name: "file",
	default_file: NULL,
	parser: parse_file,
	unparser: unparse_file
};

static
int has_option(struct cnfmodule *cm, const char *opt_name)
{
	return cm &&
		cm->opt_root &&
		cm->opt_root->first_child &&
		find_node(cm->opt_root->first_child, opt_name);
}

struct cnfnode *parse_file(struct cnfmodule *cm, FILE *fptr)
{
	struct cnfnode *cn_root;
	char buf[4096];
	int n;

	cn_root = create_cnfnode("(root)");
	if(cn_root){
		char *p = buf;
		n = fread(buf, 1, 4095, fptr);
		buf[n] = 0;
		if (has_option(cm, "strip")){
			/* Strip whitespace at the end. */
			while (n && isspace(buf[n-1]))
				n--;
			buf[n] = 0;

			/* And at the start. */
			while (isspace(*p))
				p++;
		}
		cnfnode_setval(cn_root, p);
	}

	return cn_root;
}

int unparse_file(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root)
{
	fprintf(fptr, "%s", cn_root->value ? cn_root->value : "");
	if(has_option(cm, "add-newline"))
		fputc('\n', fptr);

	return 0;
}

void register_file(struct cnfnode *opt_root)
{
	register_cnfmodule(&this_module, opt_root);
}
	

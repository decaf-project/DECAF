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

struct cnfnode *parse_cmdline(const char **pp, const char *optstring)
{
	struct cnfnode *cn_top, *cn_opt;
	const char *p, *po;
	char buf[256];

	p = *pp;

	dup_next_word_b(&p, buf, sizeof(buf));
	cn_top = create_cnfnode(buf);

	while(*p){
		const char *p0;

		p0 = p;

		skip_spaces(&p);

		dup_next_word_b(&p, buf, sizeof(buf));

		if(buf[0] == '-'){
			int c = buf[1];

			po = optstring;
			while(*po && *po != c) po++;

			if(*po){
				cn_opt = create_cnfnode(buf);
				skip_spaces(&p);

				po++;
				if(*po == ':'){
					if(*p != '\"' && *p != '\'')
						dup_next_word_b(&p, buf, sizeof(buf));
					else
						dup_next_quoted_b(&p, buf, sizeof(buf), *p);
					cnfnode_setval(cn_opt, buf);
				}

				append_node(cn_top, cn_opt);
			}else
				break;
		}else{
			p = p0;
			break;
		}
	}

	*pp = p;
	return cn_top;
}



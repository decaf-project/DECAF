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

/* may be used by other parsers: */
struct cnfnode *parse_pair_line_sep(const char **pp, int sep);
struct cnfnode *parse_pair_line(const char **p);

int unparse_pair_line_sep(FILE *fptr, struct cnfnode *cn_top, int sep);
int unparse_pair_line(FILE *fptr, struct cnfnode *cn_top);


struct cnfnode *parse_pair(struct cnfmodule *cm, FILE *fptr);
int unparse_pair(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root);
void register_pair(struct cnfnode *opt_root);

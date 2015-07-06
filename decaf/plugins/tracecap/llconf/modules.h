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

extern struct cnfmodule *cnfmodules;

/** Information for registering a parser module. */
struct cnfmodule
{
  struct cnfmodule *next; /**< The next module in the registration list. */
  char *name; /**< The text name of this parser module. */
  char *default_file; /**< The file that this module operates on, if none is specified when the parser is instantiated. */
  struct cnfnode *(*parser)(struct cnfmodule *cm, FILE *fptr); /**< The function to use to create an in-memory tree from the given stream. */
  int (*unparser)(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root); /**< The function to use to write the in-memory tree to the given stream. */
  struct cnfnode *opt_root; /**< The default set of options for this parser. */
};

void register_cnfmodule(struct cnfmodule *cm, struct cnfnode *opt_root);

void unregister_all(void);

struct cnfmodule *find_cnfmodule(const char *name);

void cnfmodule_setopts(struct cnfmodule *cm, struct cnfnode *opt_root);

struct cnfnode *parse_options(const char *string);

struct cnfnode *cnfmodule_parse(struct cnfmodule *cm, FILE *fin);

struct cnfnode *cnfmodule_parse_file(struct cnfmodule *cm, const char *fname);

void destroy_cnfmodule(struct cnfmodule *cm);

struct cnfmodule *clone_cnfmodule(struct cnfmodule *cm,
				  const char *new_name,
				  const char *default_file,
				  struct cnfnode *opt_root);

int cnfmodule_unparse(struct cnfmodule *cm, FILE* fout,
				  struct cnfnode *cn_root);

int cnfmodule_unparse_file(struct cnfmodule *cm, const char *fname,
			   struct cnfnode *cn_root);

int cnfmodule_register_plugin(const char *name, const char *path, struct cnfnode *opt_root);

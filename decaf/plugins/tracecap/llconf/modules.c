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
#include <errno.h>
#include <dlfcn.h>

#include "strutils.h"
#include "nodes.h"
#include "modules.h"

struct cnfmodule *cnfmodules = NULL;

/** Register a module.
 * This will usually be called by the parser module
 * in one of the register_* functions.
 * @param cm pointer to the module
 * @param opt_root pointer to the root of the options tree. Can be NULL.
 */
void register_cnfmodule(struct cnfmodule *cm, struct cnfnode *opt_root)
{
	struct cnfmodule **pcm;

	/* we also check if the module is already registered - otherwise
	   we may get very weird results. */
	for(pcm = &cnfmodules; *pcm && *pcm != cm; pcm = &((*pcm)->next));
	if(!*pcm){
		*pcm = cm;
		cm->opt_root = opt_root;
		cm->next = NULL;
	}
}

void unregister_all(void)
{
	cnfmodules = NULL;
}

/** Destroy a module previously created using clone_cnfmodule().
 * free the resources of a module. The module MUST have been created by clone_module.
 * @param cm pointer to the module to be destroyed
 */
void destroy_cnfmodule(struct cnfmodule *cm)
{
	if(cm->default_file) free(cm->default_file);
	if(cm->name) free(cm->name);
	if(cm->opt_root) destroy_cnftree(cm->opt_root);
	free(cm);
}

/** Clone a module.
 * Copy a module structure, and give the copy a new name, and optionally
 * another default file and options.
 * @param cm pointer to the module to be copied
 * @param new_name pointer to the name of the new module
 * @param default_file pointer to the new default file. If NULL, the value of the original will be copied
 * @param opt_root pointer to the root of the options tree. If NULL, the value of the original will be copied
 * @return pointer to the new module
 */
struct cnfmodule *clone_cnfmodule(struct cnfmodule *cm,
				  const char *new_name, const char *default_file, struct cnfnode *opt_root)
{
	struct cnfmodule *new_cm;

	new_cm = malloc(sizeof(struct cnfmodule));
	memset(new_cm, 0, sizeof(struct cnfmodule));

	if(new_name)
		new_cm->name = strdup(new_name);
	else if(cm->name)
		new_cm->name = strdup(cm->name);

	if(default_file)
		new_cm->default_file = strdup(default_file);
	else if(cm->default_file)
		new_cm->default_file = strdup(cm->default_file);

	if(opt_root)
		new_cm->opt_root = opt_root;
	else if(cm->opt_root)
		new_cm->opt_root = cm->opt_root;

	new_cm->parser = cm->parser;
	new_cm->unparser = cm->unparser;

	return new_cm;
}

/** Find a module by name.
 * Searches in the list of registered modules for a module with a matching name.
 * @param name the name
 * @return pointer to the found module, or NULL if no module was found.
 */
struct cnfmodule *find_cnfmodule(const char *name)
{
	struct cnfmodule *cm;

	for(cm = cnfmodules; cm; cm = cm->next){
		if(strcmp(cm->name, name) == 0)
			return cm;
	}
	return NULL;
}

/** Set module options.
 * @param cm pointer to the module
 * @param opt_root pointer to the root of the options tree. Although the
 * @param opt_root syntax of this tree depends on the implementation of the module,
 * @param opt_root this will most likely be one returned by the parse_options() function
 * @sa parse_options
*/
void cnfmodule_setopts(struct cnfmodule *cm, struct cnfnode *opt_root)
{
	cm->opt_root = opt_root;
}

/** Set module options.
    @param cm pointer to the module
    @param name pointer to the name
*/
void cnfmodule_setname(struct cnfmodule *cm, const char *name)
{
	if(cm->name)
		free(cm->name);
	cm->name = strdup(name);
}

/** Parse module options
 * Parse an option string into a cnfnode tree. This can be used to
 * set options for a particular node.
 * @param string
 * @return the root to the options tree
 */
struct cnfnode *parse_options(const char *string)
{
	struct cnfnode *cn_top, *cn;
	const char *p;
	char *q, buf[256];

	cn_top = create_cnfnode("(root)");

	p = string;
	while(*p){
		q = buf;
		while(*p && *p != '=' && *p != ',' && q < buf+255) *q++ = *p++;
		*q = 0;
		cn = create_cnfnode(buf);
		append_node(cn_top, cn);

		if(*p && *p == '='){
			p++; q = buf;
			while(*p && *p != ',' && q < buf+255) *q++ = *p++;
			*q = 0;
			cnfnode_setval(cn, buf);

		}else
			cnfnode_setval(cn, "");
		if(*p) p++;
	}

	return cn_top;
}

/** Parse from a stream.
 * @param cm pointer to the module
 * @param fin pointer to the stream
 * @return if successful, the pointer to root of the parsed tree.
*/
struct cnfnode *cnfmodule_parse(struct cnfmodule *cm, FILE *fin)
{
	struct cnfnode *cn_root = NULL;

	cn_root = cm->parser(cm, fin);

	return cn_root;
}

/** Parse from a file.
 * @param cm pointer to the module
 * @param fname pointer to the filename. If NULL, the default of the module will be used.
 * @return if successful, the pointer to root of the parsed tree.
*/
struct cnfnode *cnfmodule_parse_file(struct cnfmodule *cm, const char *fname)
{
	struct cnfnode *cn_root = NULL;
	FILE *fin;

	if(fname == NULL)
		fname = cm->default_file;
	if(fname){
		if((fin = fopen(fname, "r"))){
			cn_root = cnfmodule_parse(cm, fin);
			fclose(fin);
		}
	}
	return cn_root;
}

/** Unparse to a stream.
 * @param cm pointer to the module
 * @param fout pointer to the stream
 * @param cn_root the pointer to root of the parsed tree
 * @return 0 if successful
*/
int cnfmodule_unparse(struct cnfmodule *cm, FILE* fout,
				  struct cnfnode *cn_root)
{
	return cm->unparser(cm, fout, cn_root);
}

/** Unparse to a file.
 * @param cm pointer to the module
 * @param fname pointer to the filename. If NULL, the default of the module will be used.
 * @param cn_root the pointer to root of the parsed tree
 * @return 0 if successful
*/
int cnfmodule_unparse_file(struct cnfmodule *cm, const char *fname,
			   struct cnfnode *cn_root)
{
	FILE *fout;
	int ret = -1;

	if(fname == NULL)
		fname = cm->default_file;
	if(fname){
		if((fout = fopen(fname, "w"))){
			ret = cnfmodule_unparse(cm, fout, cn_root);
			fclose(fout);
		}
	}
	return ret;
}

/** Load a shared library module
 * Load a shared library module and register a new parser by calling a
 * register function. The register function needs to have a register function of the prototype
 *  void llconf_register_foo(struct cnfnode *opt_root), with 'foo' replaced by the name.
 * @param name name of the parser
 * @param path path to the shared library object
 * @param opt_root pointer to the root of the options tree. Can be NULL.
 * @return 0 on success
 */

int cnfmodule_register_plugin(const char *name, const char *path, struct cnfnode *opt_root)
{
	void *dlh;

	dlh = dlopen(path, RTLD_LAZY);
	if(dlh){
		char *dlerr;
		char fname[256];
		struct cnfmodule *(*fe_reg_func)(struct cnfnode *);

		snprintf(fname, sizeof(fname), "llconf_register_%s", name);
		dlerror();    /* Clear any existing error */
		fe_reg_func = dlsym(dlh, fname);
		if((dlerr = dlerror()) == NULL)
			fe_reg_func(opt_root);
		else{
			return -2;
		}
	}else{
		return -1;
	}
	return 0;
}

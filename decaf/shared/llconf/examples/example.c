/* -*- linux-c -*- */
/*
    This file is part of llconf2

    Copyright (C) 2004  Oliver Kurth <oku@debian.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "strutils.h"
#include "nodes.h"
#include "lines.h"
#include "entry.h"
#include "modules.h"
#include "parsers/pslave.h"

int main(int argc, char *argv[])
{
        FILE *fout;
	struct cnfmodule *module;
	char *modname = "pslave";
	struct cnfnode *cn_root, *cn;
	struct cnfnode *cn_root_backup;
	struct cnfresult *cnf_res = NULL, *cr;
	int ret;
	const char *string;

	/* you have to register all modules you want to use
	   somewhere in the beginning of your program: */
	register_pslave(NULL);

	/* find the module you want to use by name: */
	module = find_cnfmodule(modname);
	if(!module){
		fprintf(stderr, "no module with name '%s' found.\n", modname);
		exit(1);
	}
      
#if 0
	/* open the file */
	fin = fopen(argv[1], "r");
	if(!fin){
		fprintf(stderr, "could not open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	/* the parser reads from the file pointer */
	cn_root = module->parser(fin);

	/* the whole configuration is now parsed into a tree. Close the file */
	fclose(fin);

#else
	/* read and parse from file: */
	cn_root = cnfmodule_parse_file(module, argv[1]);
	if(!cn_root){
		fprintf(stderr, "could not open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}
#endif

	/* we can now access the whole configuration through cn_root */

	/* make a backup, to be used later: */
	cn_root_backup = clone_cnftree(cn_root);

	ret = compare_cnftree(cn_root, cn_root_backup);
	printf("comparing trees returned %d\n", ret);

	/* search for some configuration entry: */
	cnf_res = cnf_find_entry(cn_root, "all/authtype");

	if(cnf_res){
		char *path = NULL;
		/* cnf_res is a list with all matching entries: */
		for(cr = cnf_res; cr; cr = cr->next){
			cn = cr->cnfnode;
			path = cr->path;
			printf("%s = %s\n", cr->path, cn->value);
		}

		/* path will contain the last full path to authtype. Save it,
		   because destroy_cnfresult() will free the string: */
		if (NULL != path) {
			path = strdup(path);

			/* Okay, let's change the last entry to 'local': */

			printf("setting '%s' to local.\n", path);

			cnf_res = cnf_find_entry(cn_root, path);
			if(cnf_res){
				cn = cnf_res->cnfnode;
				cnfnode_setval(cn, "local");
			}else{
				fprintf(stderr, "'%s' not found\n", path);
			}
			free(path);
		}
		/* idealy, we free the memory used by cnf_res: */
		destroy_cnfresult(cnf_res);
		cnf_res = NULL;
	}else{
		/* there was no entry for authtype. Let's add one.

		 We can set the value at the same time.

		 We do not want to merge with any existing branch,
		 so we set the 'do_merge' parameter to 0: */
		cnf_add_branch(cn_root, "all/authtype=local", 0);
	}

	ret = compare_cnftree(cn_root, cn_root_backup);
	printf("comparing trees returned %d\n", ret);

	/* ideally, we free the memory used by cnf_res. If it's NULL, destroy_cnfresult() does nothing: */
	destroy_cnfresult(cnf_res);
	cnf_res = NULL;

	/* some other examples: */

	/* set an existing entry: */
	ret = cnf_set_entry(cn_root, "s0/authtype", "local", 0);
	if(ret < 0){
		/* we did not want to create the entry if it does not exist,
		   so we may fail: */
		printf("cnf_set_entry for '%s' failed: %s\n", "s0/authtype", strerror(errno));
	}

	/* set an entry also if it does not exist: */
	ret = cnf_set_entry(cn_root, "s1/authtype", "local", 1);
	if(ret < 0){
		/* this should not fail: */
		printf("cnf_set_entry for '%s' failed: %s\n", "s1/authtype", strerror(errno));
	}

	/* just print an entry, if it exists: */
	string = cnf_get_entry(cn_root, "s2/authtype");
	if(string)
		printf("s2.authtype = %s\n", string);
	else
		printf("s2.authtype does not exist\n");

	/* now write the modified file: */
	fout = fopen(argv[2], "w");
	if(fout){
		module->unparser(module, fout, cn_root);
	}else{
		fprintf(stderr, "could not open %s: %s\n", argv[2], strerror(errno));
		exit(1);
	}

	/* compare with backup: */

	ret = compare_cnftree(cn_root, cn_root_backup);
	printf("comparing trees returned %d\n", ret);

	/* clean up: */
	destroy_cnftree(cn_root);

	exit(0);
}


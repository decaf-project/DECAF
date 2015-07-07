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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "strutils.h"
#include "nodes.h"
#include "lines.h"
#include "entry.h"
#include "modules.h"

#include "parsers/allmodules.h"

int exit_code = 0;

FILE *open_file_or_exit(const char *fname, const char *mode)
{
	FILE *fptr;

	if(strcmp(fname, "-") == 0){
		if(mode[0] == 'r')
			fptr = stdin;
		else
			fptr = stdout;
	}else{
		fptr = fopen(fname, mode);
		if(!fptr){
			fprintf(stderr, "could not open '%s': %s\n", fname, strerror(errno));
			exit(1);
		}
	}
	return fptr;
}

FILE *open_tmpfile_or_exit(const char *fname)
{
	if(strcmp(fname, "-") == 0){
		return stdout;
	}else{
		char tmpname[256];
		snprintf(tmpname, 256, "%s.tmp", fname);
		return open_file_or_exit(tmpname, "w");
	}
}

void move_tmpfile_or_exit(const char *fname)
{
	if(strcmp(fname, "-") == 0)
		return;
	else{
		char tmpname[256];
		snprintf(tmpname, 256, "%s.tmp", fname);
		if(rename(tmpname, fname) != 0){
			fprintf(stderr, "could not move '%s' to '%s' : %s\n",
				tmpname, fname, strerror(errno));
			exit(1);
		}
	}
}

void usage()
{
	struct cnfmodule *module = NULL;
	fprintf(stderr, "Usage: %s [<parser-name>] [OPTIONS...] action [arg ...] ( ; action [arg...] )...\n\n"
		"LossLess CONFiguration command-line utility; parse and manipulate config files.\n"
		"\nSupported parsers:\n", "ThisProgram" /*program_invocation_short_name*/); //MacOS does not have this symbol

	/*
	 * Output the list of known parsers
	 */
	if (!cnfmodules)
		register_all();

	for (module = cnfmodules; module; module = module->next) {
		fprintf(stderr, " %s", module->name);
	}
	fprintf(stderr, "\n" 
		"\nInput/Output Options:\n"
		" -f file, --file=file          Reads and writes from the named file.\n"
		" -i file, --in-file=file       Reads from the named file.\n"
		" -o file, --out-file=file      Writes to the named file.\n"
		" -s, --strip                   Strip non-data nodes after reading.\n"
		" -p, --parser=name             Select the parser to use.\n"
		" -O opts, --parser-options=opts   Provide options for the parser.\n"
		"\nMiscellaneous Options:\n"
		" -h, --help                    This help message.\n"
		"\nActions:\n"
		" get <path>                    Return the value at the specified node.\n"
		" set <path> <value>            Write a new value into the given node.\n"
		" add <containerpath> <name>    Adds a new node with the given name.\n"
		" addne <containerpath> <name>  Adds a new node if it doesn't already exist.\n"
		" del <path>                    Deletes a node, also deleting empty parents.\n"
		" rem <path>                    Deletes only the named node.\n"
		" list [<path>]                 Lists child nodes at a path (default=root node).\n"
		" find [<path>]                 Print a fully-qualified path to matching nodes.\n"
		" exists <path>                 Return success if the node exists.\n"
		" unparse                       Write the output file without altering the tree.\n"
		" dump                          Output the parsed file in a tab-delimited tree.\n"
		"\n");
}

static void fail(int argcount, char *argv[], char *reason)
{
	int i;
	fprintf(stderr, "Error: %s\nFound: \"", reason);
	for (i = 0; i < argcount; ++i) {
		fprintf(stderr, "%s", argv[optind + i]);
		if (i < argcount - 1)
			fprintf(stderr, " ");
		else
			fprintf(stderr, "\"\n");
	}
	exit_code = 1;
}


int main(int argc, char *argv[])
{
	char *fname_in = NULL, *fname_out = NULL, *modname = NULL;
	struct cnfnode *opt_root = NULL;
	int do_strip = 0;
	int is_dirty = 0;
	FILE *fin;
	struct cnfmodule *module;
	struct cnfnode *cn_root;

	while(1){
		int c;

		static struct option long_options[] = {
			{"in-file", 1, 0, 'i'},
			{"out-file", 1, 0, 'o'},
			{"file", 1, 0, 'f'},
			{"strip", 0, 0, 's'},
			{"parser", 1, 0, 'p'},
			{"parser-options", 1, 0, 'O'},
			{"help", 0, 0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "i:o:f:p:sO:h?",
			long_options, NULL);

		if (c == -1)
			break;

		switch(c){
		case 'i':
			fname_in = optarg;
			break;
		case 'o':
			fname_out = optarg;
			break;
		case 'f':
			fname_out = fname_in = optarg;
			break;
		case 'p':
			modname = optarg;
			break;
		case 's':
			do_strip = 1;
			break;
		case 'O':
			opt_root = parse_options(optarg);
			break;
		case 'h':
		case '?':
		default:
			/* If it's an error, getopt has already produced an error message. */
			usage();
			return 1;
		}
	}

	/*
	 * Allow the module name to be specified as the first parameter
	 * rather than as an option.
	 */
	if (!modname){
		if (optind < argc) {
			modname = argv[optind++];
		}else{
			fprintf(stderr, "No parser module specified.\n");
			usage();
			return 1;
		}
	}

	/*
	 * Find the parser module.
	 */
	register_all();

	module = find_cnfmodule(modname);
	if(!module){
		fprintf(stderr, "no module with name '%s' found.\n", modname);
		exit(1);
	}

	/*
	 * Set the module up as requested.
	 */
	cnfmodule_setopts(module, opt_root);

	if(!fname_in)
		fname_in = module->default_file;

	if(!fname_out)
		fname_out = module->default_file;

	if(fname_in){
		fin = open_file_or_exit(fname_in, "r");
	}else{
		fprintf(stderr, "no input filename given, and the module has no default.\n");
		exit(1);
	}

	/*
	 * Parse the input file.
	 */
	cn_root = module->parser(module, fin);

	if(!cn_root){
		fprintf(stderr, "parser failed\n");
		exit(1);
	}

	if(do_strip)
		strip_cnftree(cn_root);

	if(fin != stdin) fclose(fin);

	/*
	 * Process the action(s).
	 */
	while (optind < argc) {
		struct cnfresult *cnf_res = NULL, *cr;
		int argcount = 0;
		char *action;

		while ((optind + argcount < argc) && (strcmp(argv[optind + argcount], ";") != 0))
			argcount++;

		/*
		 * A null command...
		 */
		if(!argcount) {
			optind++;
			continue;
		}
    
    		/*
		 * Find the action.
		 */
		action = argv[optind];
		if(strcmp(action, "dump") == 0){

			dump_nodes(cn_root, 0);

		}else if(strcmp(action, "get") == 0){
			if (argcount != 2) {
				fail(argcount, argv, "Expected one path for \"get\".");
				break;
			}

			cnf_res = cnf_find_entry(cn_root, argv[optind + 1]);

			if(cnf_res){
				for(cr = cnf_res; cr; cr = cr->next){
					printf("%s\n", cr->cnfnode->value ? cr->cnfnode->value : "");
				}
				destroy_cnfresult_list(cnf_res);
			}else{
				exit_code = 1;
			}

		}else if(strcmp(action, "exists") == 0){
			if (argcount != 2) {
				fail(argcount, argv, "Expected one path for \"exists\".");
				break;
			}

			cnf_res = cnf_find_entry(cn_root, argv[optind + 1]);

			if(!cnf_res){
				exit_code = 1;
			}else{
				destroy_cnfresult_list(cnf_res);
			}

		}else if(strcmp(action, "set") == 0){
			if (argcount != 3) {
				fail(argcount, argv, "Expected a path and a value for \"set\".");
				break;
			}

			cnf_res = cnf_find_entry(cn_root, argv[optind + 1]);
			if(cnf_res){
				/* we set the _first_ found result */
				cnfnode_setval(cnf_res->cnfnode, argv[optind + 2]);
				is_dirty = 1;
				destroy_cnfresult_list(cnf_res);
			}else{
				fprintf(stderr, "%s not found\n", argv[optind + 1]);
				exit_code = 1;
			}
		}else if(strcmp(action, "rename") == 0){
			if (argcount != 3) {
				fail(argcount, argv, "Expected a path and a new name for \"rename\".");
				break;
			}

			cnf_res = cnf_find_entry(cn_root, argv[optind + 1]);
			if(cnf_res){
				/* we set the _first_ found result */
				cnfnode_setname(cnf_res->cnfnode, argv[optind + 2]);
				is_dirty = 1;
				destroy_cnfresult_list(cnf_res);
			}else{
				fprintf(stderr, "%s not found\n", argv[optind + 1]);
				exit_code = 1;
			}
		}else if(strcmp(action, "unparse") == 0){
			if (argcount != 1) {
				fail(argcount, argv, "Expected no parameters for \"unparse\".");
				break;
			}
			is_dirty = 1;

		}else if((strcmp(action, "add") == 0) || (strcmp(action, "addne") == 0) || 
			 (strcmp(action, "del") == 0) || (strcmp(action, "rem") == 0)){

			if(strcmp(action, "add") == 0){
				struct cnfnode *cn_path;
				if (argcount != 3) {
					fail(argcount, argv, "Expected a path and a new node name for \"add\".");
					break;
				}

				cn_path = cnf_get_node(cn_root, argv[optind + 1]);
				if(cn_path){
					cnf_add_branch(cn_path, argv[optind + 2], 0);
					is_dirty = 1;
				}else{
					fprintf(stderr, "%s not found\n", argv[optind + 1]);
					exit_code = 1;
				}
			}else if(strcmp(action, "addne") == 0){
				struct cnfnode *cn_path;
				if (argcount != 3) {
					fail(argcount, argv, "Expected a path and a new node name for \"addne\".");
					break;
				}

				cn_path = cnf_get_node(cn_root, argv[optind + 1]);
				if(cn_path){
					if(!cnf_get_node(cn_path, argv[optind + 2])){
						cnf_add_branch(cn_path, argv[optind + 2], 0);
						is_dirty = 1;
					}
				}else{
					fprintf(stderr, "%s not found\n", argv[optind + 1]);
					exit_code = 1;
				}
			}else{
				if (argcount != 2) {
					fail(argcount, argv, "Expected a path for \"del\" or \"rem\".");
					break;
				}
				cnf_del_branch(cn_root, argv[optind + 1], strcmp(action, "del") == 0);
				is_dirty = 1;
			}

		}else if(strcmp(action, "list") == 0){
			if (argcount > 2) {
				fail(argcount, argv, "Expected zero or one parameters for \"list\".");
				break;
			}

			if(argcount == 2)
				cnf_res = cnf_find_entry(cn_root, argv[optind + 1]);
			else
				cnf_res = cnf_find_entry(cn_root, ".");

			for(cr = cnf_res; cr; cr = cr->next){
				struct cnfnode *cn = cr->cnfnode;
				for(cn = cn->first_child;cn; cn = cn->next)
					printf("%s\n", cn->name);
			}
			if(cnf_res) destroy_cnfresult_list(cnf_res);
		}else if(strcmp(action, "find") == 0){
			if (argcount > 2) {
				fail(argcount, argv, "Expected zero or one parameters for \"find\".");
				break;
			}

			if(argcount == 2)
				cnf_res = cnf_find_entry(cn_root, argv[optind + 1]);
			else
				cnf_res = cnf_find_entry(cn_root, ".");

			for(cr = cnf_res; cr; cr = cr->next)
				printf("%s\n", cr->path);
			
			if(cnf_res) destroy_cnfresult_list(cnf_res);
		}

		/*
		 * Move to the next command (skipping the separator).
		 */
		optind += argcount + 1;
	}

	/*
	 * If a change was made, rewrite the file.
	 */
	if(is_dirty){
		FILE *fout;
		int result;
		if(!fname_out){
			fprintf(stderr, "no output filename given, and the module has no default.\n");
			exit(1);
		}

		fout = open_tmpfile_or_exit(fname_out);

		result = module->unparser(module, fout, cn_root);
		if(fout != stdout) fclose(fout);

		move_tmpfile_or_exit(fname_out);
	}

	destroy_cnftree(cn_root);

	exit(exit_code);
}

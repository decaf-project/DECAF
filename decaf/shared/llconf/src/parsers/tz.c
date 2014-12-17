/* -*- linux-c -*- */
/*
    This file is part of llconf2

    Copyright (C) 2004-2007 Avocent Corp.

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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

#include "strutils.h"
#include "nodes.h"
#include "lines.h"
#include "modules.h"
#include "tz.h"
#include "entry.h"

#define TRUNK_AFTER_LAST 0x00
#define CONCAT_AFTER_LAST 0x01

static
struct cnfmodule this_module = {
	name: "tz",
	parser: parse_tz,
	unparser: unparse_tz,
	default_file: NULL,
	opt_root: NULL
};

int get_strings(const char *str, unsigned int num_args, int flags, ...) {
	va_list ap;
	int num, len;
	const char *p;
	char **buff = NULL;
	
	p = str;
	num = len = 0;
	
	va_start(ap, flags);
	while(*p && isspace(*p)) p++;
	while (	(*p && flags == CONCAT_AFTER_LAST) ||
			 (*p && num < num_args)) {
		//count until whitespace
		while (!isspace(*p)) {
			p++;
			len++;
		}
		//allocate memory and copy to pointer provided
		if (len > 0) {
			if (num < num_args) {
				buff = va_arg(ap, char **);
				*buff = strndup(str, len);
			}
			else {
				//concatenate the last string with everything we see in the file after num_args is up
				char * tmp = malloc(strlen(*buff) + len + 2);
				strcpy(tmp, *buff);
				strcat(tmp, " ");
				strncat(tmp, str, len);
				free(*buff);
				*buff = tmp;
			}
			num++;
			str += len;
			len = 0;
		}
		else {
			p++;
			str++;
		}
	}
	
	//return the number of strings we malloced
	return num;
}

// Returns 1 if found, 0 if not found
int regmatch (const char *string, char *regex) {
	
	regex_t preg;
	int match = 0;
	if (regcomp( &preg, regex, REG_EXTENDED | REG_NOSUB ) == 0) {
		if (regexec( &preg, string, 0, NULL, 0 ) != REG_NOMATCH)
			match = 1;
		regfree(&preg);
	}
	return match;
}

static
int parse_tz_options(struct cnfnode *opt_root, int *cmt_char) {
	struct cnfnode *cn;

	if(opt_root == NULL) return -1;
	if(opt_root->first_child == NULL) return -1;

	for(cn = opt_root->first_child; cn; cn = cn->next){
		if(strcmp(cn->name, "comment") == 0){
			*cmt_char = (unsigned char)(cn->value[0]);
		}
	}

	return 0;
}

static
struct confline *parse_tz_rule(struct confline *cl, struct cnfnode *cn_root, int cmt_char) {
	
	struct cnfnode *cn;
	//Rule  NAME  FROM  TO    TYPE  IN   ON       AT    SAVE  LETTER/S
	char	*rule = NULL,
	 	*name = NULL,
		*from = NULL,
		*to = NULL,
		*type = NULL,
		*in = NULL,
		*on = NULL,
		*at = NULL,
		*save = NULL,
		*letters = NULL;
	const char *p = cl->line;
	
	if(*p == cmt_char) {
		cn = create_cnfnode(".comment");
		cnfnode_setval(cn, p);
		append_node(cn_root, cn);
	}
	else if (get_strings(p, 10, TRUNK_AFTER_LAST, &rule, &name, &from, &to, &type, &in, &on, &at, &save, &letters) == 10) {
		cn = create_cnfnode(name);
		append_node(cn_root, cn);
		cn_root = cn;
		
		cn = create_cnfnode("from");
		cnfnode_setval(cn, from);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("to");
		cnfnode_setval(cn, to);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("type");
		cnfnode_setval(cn, type);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("in");
		cnfnode_setval(cn, in);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("on");
		cnfnode_setval(cn, on);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("at");
		cnfnode_setval(cn, at);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("save");
		cnfnode_setval(cn, save);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("letters");
		cnfnode_setval(cn, letters);
		append_node(cn_root, cn);
	}
	else {
		cn = create_cnfnode(".empty");
		cnfnode_setval(cn, "");
		append_node(cn_root, cn);
	}
	
	cl = cl->next;
	if (rule)
		free(rule);
	if (name)
		free(name);
	if (from)
		free(from);
	if (to)
		free(to);
	if (type)
		free(type);
	if (in)
		free(in);
	if (on)
		free(on);
	if (at)
		free(at);
	if (save)
		free(save);
	if (letters)
		free(letters);
	return cl;
}

static
struct confline *parse_tz_zone(struct confline *cl, struct cnfnode *cn_root, int cmt_char) {
	
	struct cnfnode *cn;
	//Zone  NAME                GMTOFF  RULES/SAVE  FORMAT  [UNTIL]
	char 	*zone = NULL,
	 	*name = NULL,
		*gmtoff = NULL,
		*rules = NULL,
		*format = NULL,
		*until = NULL;
	const char *p = cl->line;
	
	if(*p == cmt_char) {
		cn = create_cnfnode(".comment");
		cnfnode_setval(cn, p);
		append_node(cn_root, cn);
	}
	else if (get_strings(p, 6, CONCAT_AFTER_LAST, &zone, &name, &gmtoff, &rules, &format, &until) >= 5) {
		cn = create_cnfnode(name);
		append_node(cn_root, cn);
		cn_root = cn;
		
		cn = create_cnfnode("gmtoff");
		cnfnode_setval(cn, gmtoff);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("rules");
		cnfnode_setval(cn, rules);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("format");
		cnfnode_setval(cn, format);
		append_node(cn_root, cn);
		
		if (until) {
			cn = create_cnfnode("until");
			cnfnode_setval(cn, until);
			append_node(cn_root, cn);
		}
	}
	else {
		cn = create_cnfnode(".empty");
		cnfnode_setval(cn, "");
		append_node(cn_root, cn);
	}
	
	cl = cl->next;
	if (zone)
		free(zone);
	if (name)
		free(name);
	if (gmtoff)
		free(gmtoff);
	if (rules)
		free(rules);
	if (format)
		free(format);
	if (until)
		free(until);
	return cl;
}

static
struct confline *parse_tz_link(struct confline *cl, struct cnfnode *cn_root, int cmt_char) {
	
	struct cnfnode *cn;
	//Link  LINK-FROM        LINK-TO
	char 	*link = NULL,
		*from = NULL,
		*to = NULL;
	const char *p = cl->line;
	
	if(*p == cmt_char) {
		cn = create_cnfnode(".comment");
		cnfnode_setval(cn, p);
		append_node(cn_root, cn);
	}
	else if (get_strings(p, 3, TRUNK_AFTER_LAST, &link, &from, &to) == 3) {
		cn = create_cnfnode(from);
		append_node(cn_root, cn);
		cn_root = cn;
				
		cn = create_cnfnode("to");
		cnfnode_setval(cn, to);
		append_node(cn_root, cn);
	}
	else {
		cn = create_cnfnode(".empty");
		cnfnode_setval(cn, "");
		append_node(cn_root, cn);
	}
	
	cl = cl->next;
	if (link)
		free(link);
	if (from)
		free(from);
	if (to)
		free(to);
	return cl;
}

static
struct confline *parse_tz_leap(struct confline *cl, struct cnfnode *cn_root, int cmt_char) {
	
	struct cnfnode *cn;
	//Leap  YEAR  MONTH  DAY  HH:MM:SS  CORR  R/S
	char 	*leap = NULL,
		*year = NULL,
		*month = NULL,
		*day = NULL,
		*hhmmss = NULL,
		*corr = NULL,
		*rs = NULL;
	const char *p = cl->line;
	
	if(*p == cmt_char) {
		cn = create_cnfnode(".comment");
		cnfnode_setval(cn, p);
		append_node(cn_root, cn);
	}
	else if (get_strings(p, 7, TRUNK_AFTER_LAST, &leap, &year, &month, &day, &hhmmss, &corr, &rs) == 7) {
		cn = create_cnfnode(year);
		append_node(cn_root, cn);
		cn_root = cn;
		
		cn = create_cnfnode("month");
		cnfnode_setval(cn, month);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("day");
		cnfnode_setval(cn, day);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("hhmmss");
		cnfnode_setval(cn, hhmmss);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("corr");
		cnfnode_setval(cn, corr);
		append_node(cn_root, cn);
		
		cn = create_cnfnode("rs");
		cnfnode_setval(cn, rs);
		append_node(cn_root, cn);
	}
	else {
		cn = create_cnfnode(".empty");
		cnfnode_setval(cn, "");
		append_node(cn_root, cn);
	}
	
	cl = cl->next;
	if (leap)
		free(leap);
	if (year)
		free(year);
	if (month)
		free(month);
	if (day)
		free(day);
	if (hhmmss)
		free(hhmmss);
	if (corr)
		free(corr);
	if (rs)
		free(rs);
	return cl;
}

struct cnfnode *parse_tz(struct cnfmodule *cm, FILE *fptr) {
	struct cnfnode	*cn_root = NULL,
			*cn = NULL,
			*cn_zone = NULL,
			*cn_rule = NULL,
			*cn_link = NULL,
			*cn_leap = NULL;
	struct confline *cl, *cl_root;
	int cmt_char = '#';

	cl_root = read_conflines(fptr);

	cn_root = create_cnfnode("(root)");

	if(cm && cm->opt_root){
		parse_tz_options(cm->opt_root, &cmt_char);
	}
	
	//each node must have 4 subnodes
	if ((cn_zone = cnf_get_node(cn_root, "zone")) == NULL) {
		cn_zone = create_cnfnode("zone");
		append_node(cn_root, cn_zone); 
	}
	if ((cn_rule = cnf_get_node(cn_root, "rule")) == NULL) {
		cn_rule = create_cnfnode("rule");
		append_node(cn_root, cn_rule);
	}
	if ((cn_link = cnf_get_node(cn_root, "link")) == NULL) {
		cn_link = create_cnfnode("link");
		append_node(cn_root, cn_link);
	}
	if ((cn_leap = cnf_get_node(cn_root, "leap")) == NULL) {
		cn_leap = create_cnfnode("leap");
		append_node(cn_root, cn_leap);
	}
	
	for (cl = cl_root; cl;) {
		const char *p = cl->line;

		while (*p && isspace(*p)) p++;
		
		if (regmatch(p, "^Zone"))
			cl = parse_tz_zone(cl, cn_zone, cmt_char);
		else if (regmatch(p, "^Rule"))
			cl = parse_tz_rule(cl, cn_rule, cmt_char);
		else if (regmatch(p, "^Link"))
			cl = parse_tz_link(cl, cn_link, cmt_char);
		else if (regmatch(p, "^Leap"))
			cl = parse_tz_leap(cl, cn_leap, cmt_char);
		else {
			cn = create_cnfnode(".empty");
			cnfnode_setval(cn, "");
			append_node(cn_root, cn);
			cl = cl->next;
		}
	}

	destroy_confline_list(cl_root);
	return cn_root;
}

int unparse_tz(struct cnfmodule *cm, FILE *fptr, struct cnfnode *cn_root) {
	struct confline *cl_list = NULL, *cl;
	struct cnfnode *cn_section, *cn_zone, *cn_rule, *cn_link, *cn_leap, *cn_node;
	char *buf;

	if ((cn_zone = cnf_get_node(cn_root, "zone"))) {
		for (cn_section = cn_zone->first_child; cn_section; cn_section = cn_section->next) {
			char	*name = cn_section->name,
					*gmtoff  = NULL,
					*rules = NULL, 
					*format = NULL, 
					*until = NULL; 
					
			if ((cn_node = cnf_get_node(cn_section, "gmtoff")))
				gmtoff = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "rules")))
				rules = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "format")))
				format = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "until")))
				until = cn_node->value;
					
			if (name && gmtoff && rules && format && until) {
				asprintf(&buf, "Zone %s %s %s %s %s\n", name, gmtoff, rules, format, until);
				cl_list = append_confline(cl_list, cl = create_confline(buf));
				free(buf);
			}
		}
	}
	if ((cn_rule = cnf_get_node(cn_root, "rule"))) {
		for (cn_section = cn_rule->first_child; cn_section; cn_section = cn_section->next) {
			char	*name = cn_section->name,
					*from = NULL,
					*to = NULL,
					*type = NULL,
					*in = NULL,
					*on = NULL,
					*at = NULL,
					*save = NULL,
					*letters = NULL;
					
			if ((cn_node = cnf_get_node(cn_section, "from")))
				from = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "to")))
				to = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "type")))
				type = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "in")))
				in = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "on")))
				on = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "at")))
				at = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "save")))
				save = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "letters")))
				letters = cn_node->value;
			
			if (name && from && to && type && in && on && at && save && letters) {
				asprintf(&buf, "Rule %s %s %s %s %s %s %s %s %s\n", name, from, to, type, in, on, at, save, letters);
				cl_list = append_confline(cl_list, cl = create_confline(buf));
				free(buf);
			}
		}
	}
	if ((cn_link = cnf_get_node(cn_root, "link"))) {
		for (cn_section = cn_link->first_child; cn_section; cn_section = cn_section->next) {
			char	*from = cn_section->name,
					*to = NULL;
					
			if ((cn_node = cnf_get_node(cn_section, "to")))
				to = cn_node->value;
			
			if (from && to) {
				asprintf(&buf, "Link %s %s\n", from, to);
				cl_list = append_confline(cl_list, cl = create_confline(buf));
				free(buf);
			}
		}		
	}
	if ((cn_leap = cnf_get_node(cn_root, "leap"))) {
		for (cn_section = cn_leap->first_child; cn_section; cn_section = cn_section->next) {
			char	*year = cn_section->name,
					*month = NULL,
					*day = NULL,
					*hhmmss = NULL,
					*corr = NULL,
					*rs = NULL;
			
			if ((cn_node =  cnf_get_node(cn_section, "month")))
				month = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "day")))
				day = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "hhmmss")))
				hhmmss = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "corr")))
				corr = cn_node->value;
			if ((cn_node = cnf_get_node(cn_section, "rs")))
				rs = cn_node->value;
			
			if (month && day && hhmmss && corr && rs) {
				asprintf(&buf, "Leap %s %s %s %s %s %s\n", year, month, day, hhmmss, corr, rs);
				cl_list = append_confline(cl_list, cl = create_confline(buf));
				free(buf);
			}
		}		
	}

	for(cl = cl_list; cl; cl = cl->next){
		fprintf(fptr, "%s", cl->line);
	}
	destroy_confline_list(cl_list);

	return 0;
}

void register_tz(struct cnfnode *opt_root) {
	register_cnfmodule(&this_module, opt_root);
}

struct cnfmodule *clone_cnfmodule_tz(struct cnfnode *opt_root) {
	return clone_cnfmodule(&this_module, NULL, NULL, opt_root);
}

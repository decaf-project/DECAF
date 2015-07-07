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

#include "lines.h"

struct confline *create_confline(const char *line)
{
	struct confline *cl = NULL;

	cl = malloc(sizeof(struct confline));
	if(cl){
		memset(cl, 0, sizeof(struct confline));
		cl->line = strdup(line);
	}
	return cl;
}

void destroy_confline(struct confline *cl)
{
	if(cl){
		if(cl->line) free(cl->line);
		free(cl);
	}
}

void destroy_confline_list(struct confline *cl_list)
{
	struct confline *cl, *cl_next;

	for(cl = cl_list; cl; cl = cl_next){
		cl_next = cl->next;
		destroy_confline(cl);
	}
}

struct confline *append_confline(struct confline *cl_list, struct confline *cl)
{
	if(cl_list){
		struct confline **pcl;
		for(pcl = &(cl_list->next); *pcl; pcl = &((*pcl)->next));
		*pcl = cl;
		return cl_list;
	}else
		return cl;
}

struct confline *read_conflines(FILE *fptr)
{
	char line[MAX_CONFLINE];
	struct confline *cl = NULL, *cl_next, *cl_root = NULL;

	while(fgets(line, MAX_CONFLINE-1, fptr)){
		cl_next = create_confline(line);
		if(cl)
			cl->next = cl_next;
		else
			cl_root = cl_next;
		cl = cl_next;
	}
	return cl_root;
}

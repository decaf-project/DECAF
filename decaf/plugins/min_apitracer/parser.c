/*
 * parser.c
 *
 *  Created on: Jun 8, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "DECAF_main.h"
#include "hookapi.h"

#include "parser.h"
#include "handlers.h"

#include "function_map.h"

static char error_string_response[512];

QLIST_HEAD (apilist_head, api_entry) apilist;
void gen_api_handler();

struct interpreters {
	const char *name;
	Interpreter handler;
	val2str tostr;
};

struct interpreters interpreters[] = {
		{
				.name 		= "default",
				.handler 	= (Interpreter) populate_arg,
				.tostr		= (val2str) convertval2str,
		},
};

/* Function to retrieve the pointer to the value of a config variable */
static inline char *strstr_rs (const char *haystack, const char *needle)
{
	char *end_address;
	char *new_needle;

	new_needle = (char *) strdup (needle);
	new_needle[strlen(new_needle) - 1] = '\0';

	end_address = strstr (haystack, new_needle);
	if (end_address) {
		end_address += strlen (new_needle);
		end_address = strstr (end_address, needle + strlen (new_needle));
	}
	if (end_address) {
		end_address += 1; /* skip past { or = */
		do {
			if (*end_address == '\t' || *end_address == ' ') {
				end_address++;
			} else {
				break;
			}
		} while (*end_address != '\0');
	}

	free (new_needle);
	return (end_address);
}

static inline void set_arg_defaults(struct apiarg *arg)
{
	switch(arg->type) {
	case U8:
		arg->size = 1; //1 byte
		break;
	case U16:
		arg->size = 2; //2 bytes
		break;
	case U32:
		arg->size = 4; //4 bytes
		break;
	case U64:
		arg->size = 8; //8 bytes
		break;
	case USTR:
		arg->size = 4;//Unicode String //hu a pointer to UNICODE string
		break;
	case WSTR:
	case CSTR:
		arg->size = 4; //4 bytes ptr
		break;
	case VOID:
		arg->size = 0;
		break;

	/*
	 * If ignoring, size can't be 0. We need the size field to skip those many bytes to reach the next argument.
	 */
	case IGNORE:
	case SPECIAL:
		assert(arg->size != 0);
		break;
	default:
		break;
	}
}

/*
 * This function both updates the default fields and validates the mandatory fields.
 */
int update_defaults(char *error_string)
{
	struct api_entry *api;
	struct apiarg *args;
	int i;

	QLIST_FOREACH(api, &apilist, api_list_entry) {
		if(api->modname == 0 || api->apiname == 0) {
			sprintf(error_string, "Ensure that all apis have modulename and apiname");
			goto error_ret;
		}
		if(api->numargs <= 0)
			api->numargs = 0;
		api->retarg.spec = out;
		if(api->retarg.name == 0)
			strcpy(api->retarg.name, "");
		set_arg_defaults(&(api->retarg));
		args = (struct apiarg *) api->args;
		for(i = 0; i < api->numargs; i++) {
			set_arg_defaults(&args[i]);
		}
	}

	return 0;

error_ret:
	return -1;
}

void process_apilist(uint32_t cr3)
{
	struct api_entry *api;
	target_ulong pc;
	QLIST_FOREACH(api, &apilist, api_list_entry) {
		if(api->iskernel) {
			hookapi_hook_function_byname(
					api->modname,
					api->apiname,
					1,
					0,
					gen_api_handler,
					(void *) api,
					(sizeof(struct api_entry) + api->numargs * sizeof(struct apiarg)));
		} else {
			hookapi_hook_function_byname(
					api->modname,
					api->apiname,
					1,
					cr3,
					gen_api_handler,
					(void *) api,
					(sizeof(struct api_entry) + api->numargs * sizeof(struct apiarg)));
		}
	}
}

int api_parse (const char *config_file, char **error_string, uint32_t cr3)
{
	char line[255] = {'\0'};
	char update_error_str[255] = {'\0'};
	char *loc = NULL;
	char *tempstr = NULL;
	unsigned int line_number = 0;
	struct api_entry *api = NULL;
	struct api_entry *todel = NULL;
	struct apiarg *argptr = NULL;
	int curr_arg = -1;
	int i = 0;
	int index = 0;
	char *error_reason = error_string_response;

	FILE *fp = fopen(config_file, "r");
	if(fp == NULL) {
		sprintf (error_reason, "Can't open configuration file %s for reading.\n", config_file);
		*error_string = error_reason;
		return -1;
	}

	typedef enum {
		HEAD,
		API,
		ARG,
	} ParseState;

	ParseState current_parse = HEAD;

	QLIST_INIT(&apilist);

	while( fgets (line, 255, fp)) {
		line_number += 1;
		line[strlen(line) - 1] = '\0';

		/*
		 * Clear out white space and tabs
		 */
		for (i = strlen (line) - 1; i > -1; i--) {
			if (line[i] == '\t' || line[i] == ' ') {
				line[i] = '\0';
			} else {
				break;
			}
		}

		/*
		 * Clear out comments and empty lines
		 */
		if (line[0] == '#' || line[0] == '\0') {
			continue;
		}

		switch (current_parse) {
		case HEAD:
			if (strstr_rs (line, "api{")) {
				api = (struct api_entry *) malloc (sizeof(struct api_entry));
				memset(api, 0, sizeof(struct api_entry));
				curr_arg = -1;
				current_parse = API;
			} else if (strcmp (line, "") == 0) {
			} else {
				goto parse_error;
			}
			break;

		case API:
			if ((loc = strstr_rs (line, "modname=")) != 0) {
				strcpy(api->modname, loc);
			} else if((loc = strstr_rs(line, "apiname=")) != 0) {
				strcpy(api->apiname, loc);
			} else if ((loc = strstr_rs (line, "numberargs=")) != 0) {
				api->numargs = atoi(loc);
				api = (struct api_entry *) realloc (api, sizeof(struct api_entry) + api->numargs * sizeof(struct apiarg));
				memset(api->args, 0, api->numargs * sizeof(struct apiarg));
			} else if ((loc = strstr_rs (line, "iskernel=")) != 0) {
				api->iskernel = (strcasecmp("yes", loc) == 0)? 1: 0;
			} else if ((loc = strstr_rs (line, "returntype=")) != 0) {
				if(strcasecmp(loc, "U8") == 0) {
					api->retarg.type = (ArgType) U8;
					api->retarg.size = 1;
				}
				else if(strcasecmp(loc, "U16") == 0) {
					api->retarg.type = (ArgType) U16;
					api->retarg.size = 2;
				}
				else if(strcasecmp(loc, "U32") == 0) {
					api->retarg.type = (ArgType) U32;
					api->retarg.size = 4;
				}
				else if(strcasecmp(loc, "U64") == 0) {
					api->retarg.type = (ArgType) U64;
					api->retarg.size = 8;
				}
				else if(strcasecmp(loc, "USTR") == 0)
					api->retarg.type = (ArgType) USTR;
				else if(strcasecmp(loc, "CSTR") == 0)
					api->retarg.type = (ArgType) CSTR;
				else if(strcasecmp(loc, "WSTR") == 0)
					api->retarg.type = (ArgType) WSTR;
				else if(strcasecmp(loc, "IGNORE") == 0)
					api->retarg.type = (ArgType) IGNORE;
				else if(strcasecmp(loc, "SPECIAL") == 0)
					api->retarg.type = (ArgType) SPECIAL;
				else
					goto parse_error_with_cleanup;

			} else if ((loc = strstr_rs (line, "returnstr=")) != 0) {
				strncpy(api->retarg.name, (const char *) loc, MAX_NAME_LENGTH);
			} else if ((loc = strstr_rs (line, "returnhandler=")) != 0) {
				api->retarg.func = NULL;
			} else if ((loc = strstr_rs (line, "convention=")) != 0) {
				if(strcasecmp(loc, "STDCALL") == 0)
					api->conv = (CallingConvention) STDCALL;
				else if (strcasecmp(loc, "CDECL") == 0)
					api->conv = (CallingConvention) CDECL;
				else if (strcasecmp(loc, "FASTCALL") == 0)
					api->conv = (CallingConvention) FASTCALL;
				else if (strcasecmp(loc, "THISCALL") == 0)
					api->conv = (CallingConvention) THISCALL;
				else
					goto parse_error_with_cleanup;
			} else if ((loc = strstr_rs (line, "arg{")) != 0) {
				curr_arg ++;
				argptr = (struct apiarg *)((char *) (api->args) + curr_arg * sizeof(struct apiarg));
				current_parse = ARG;
			} else if ((loc = strstr_rs (line, "}")) != 0) {
				current_parse = HEAD;
				QLIST_INSERT_HEAD (&apilist, api, api_list_entry);
			} else
				goto parse_error_with_cleanup;
			break;

		case ARG:
			if ((loc = strstr_rs (line, "name=")) != 0) {
				strncpy(argptr->name, loc, MAX_NAME_LENGTH);
			} else if ((loc = strstr_rs (line, "type=")) != 0) {
				if(strcasecmp(loc, "U8") == 0)
					argptr->type = (ArgType) U8;
				else if(strcasecmp(loc, "U16") == 0)
					argptr->type = (ArgType) U16;
				else if(strcasecmp(loc, "U32") == 0)
					argptr->type = (ArgType) U32;
				else if(strcasecmp(loc, "U64") == 0)
					argptr->type = (ArgType) U64;
				else if(strcasecmp(loc, "USTR") == 0)
					argptr->type = (ArgType) USTR;
				else if(strcasecmp(loc, "CSTR") == 0)
					argptr->type = (ArgType) CSTR;
				else if(strcasecmp(loc, "WSTR") == 0)
					argptr->type = (ArgType) WSTR;
				else if(strcasecmp(loc, "IGNORE") == 0)
					argptr->type = (ArgType) IGNORE;
				else if(strcasecmp(loc, "SPECIAL") == 0)
					argptr->type = (ArgType) SPECIAL;
				else
					goto parse_error_with_cleanup;
			} else if ((loc = strstr_rs (line, "spec=")) != 0) {
				if(strcasecmp(loc, "in") == 0)
					argptr->spec = (ArgSpec) in;
				else if(strcasecmp(loc, "out") == 0)
					argptr->spec = (ArgSpec) out;
				else if(strcasecmp(loc, "inout") == 0)
					argptr->spec = (ArgSpec) inout;
				else
					goto parse_error_with_cleanup;
			} else if ((loc = strstr_rs (line, "handler=")) != 0) {
				for(index = 0; index < sizeof(interpreters); index++) {
					if(strcmp(loc, interpreters[index].name) == 0) {
						argptr->func = interpreters[index].handler;
						argptr->tostr = interpreters[index].tostr;
						break;
					}
				}
			} else if ((loc = strstr_rs (line, "size=")) != 0) {
				argptr->size = atoi(loc);
			} else if ((loc = strstr_rs (line, "}")) != 0) {
				current_parse = API;
			} else
				goto parse_error_with_cleanup;
			break;

		default:
			goto parse_error_with_cleanup;
		}
	}

	fclose(fp);

	/* Fill in the default fields and the inferred fields for the arguments */
	if(update_defaults(update_error_str) != 0)
		goto update_error;

	process_apilist(cr3);
	return 0;

parse_error_with_cleanup:
	QLIST_FOREACH(todel, &apilist, api_list_entry) {
		QLIST_REMOVE(todel, api_list_entry);
		free(todel);
	}

parse_error:
	sprintf (error_string_response,
		"parse error at %s:%d.\n", config_file, line_number);
	*error_string = error_string_response;
	fclose (fp);
	return (-1);

update_error:
	sprintf(error_string_response,
			"parser failed the mandatory field validation: %s\n", update_error_str);
	*error_string = error_string_response;
	fclose(fp);
	return -1;
}

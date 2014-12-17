/*
 * parser.h
 *
 *  Created on: Jun 8, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 */

#ifndef PARSER_H_
#define PARSER_H_

#include <stdint.h>
#include "qemu-queue.h"

typedef enum {
	U8 		= 1, //1 byte
	U16 	= 2, //2 bytes
	U32 	= 3, //4 bytes
	U64		= 4, //8 bytes
	USTR	= 5, //Unicode String
	CSTR	= 6, //Char string (terminated by '\0')
	SPECIAL	= 7, //Special interpretation needed (invoke the interpretation function)
	IGNORE	= 8, //Ignore - If ignoring, use the size argument
	VOID	= 9, //Used only for retarg
	WSTR	= 10,
} ArgType;

typedef enum {
	STDCALL		= 1,
	CDECL		= 2,
	FASTCALL	= 3,
	THISCALL	= 4,
} CallingConvention;

typedef enum {
	in		= 1,
	out		= 2,
	inout	= 3,
} ArgSpec;

union data_holder {
	uint8_t val_8;
	uint16_t val_16;
	uint32_t val_32;
	char *str;
	uint64_t val_64;
};

struct apiarg; //To satisfy the compiler

typedef unsigned int (*Interpreter) (struct apiarg *arg, target_ulong addr, unsigned int after);
typedef void (*val2str) (char *store, struct apiarg *arg);

struct apiarg {
	char name[128];
	//Value variables are malloc'd when value is read off memory and converted to str. Freed in populate_trace_str()
	union data_holder value_before; //used for in and inout variables
	union data_holder value_after;  //used in out and inout variables.
	//void *temp; //Temporary variable to hold the address of the out variables (needed for STDCALL)
	unsigned int temp;
	ArgType type;
	ArgSpec spec;
	Interpreter func;
	val2str tostr;
	unsigned int size;
};

#define MAX_NAME_LENGTH 128
#define MAX_UNICODE_LENGTH 2*MAX_NAME_LENGTH

struct api_entry {
	char modname[MAX_NAME_LENGTH];
	char apiname[MAX_NAME_LENGTH];
	uintptr_t hook_handle;
	CallingConvention conv;
	unsigned char iskernel;
	unsigned short numargs;
	QLIST_ENTRY (api_entry) api_list_entry;
	struct apiarg retarg;
	char args[0];
};

/* Main parse function that parses the config file into the global hash table */
int api_parse (const char *config_file, char **error_string, uint32_t cr3);

void process_apilist(uint32_t cr3);
void hook_module_apis(char *name, uint32_t base, uint32_t cr3);

#endif /* PARSER_H_ */

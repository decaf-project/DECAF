/*
 * handlers.h
 *
 *  Created on: Jun 11, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 */

#ifndef HANDLERS_H_
#define HANDLERS_H_

#include <stdio.h>

struct monitored_proc {
	char name[512];
	char configfile[512];
	uint32_t cr3;
	uint32_t pid;
	FILE *tracefile;
};


struct service_entry {
	char name[512];
	uint32_t handle;
	uint32_t base;
	uint32_t size;
	uint32_t pid;
	uint32_t cr3;
	FILE *tracefile;
};

extern struct monitored_proc mon_proc;

void stdcall_handler (struct api_entry *api);
void cdecl_handler(struct api_entry *api);
void fastcall_handler (struct api_entry *api);
void thiscall_handler(struct api_entry *api);
void gen_ret_handler(void *opaque);
void gen_api_handler(void *opaque);

/* Helpers */
void convertval2str (char *store, struct apiarg *arg);
void populate_trace_str(struct api_entry *api, uint32_t pid, uint32_t tid);

int readustr(uint32_t addr, void *buf);


/* Argument handler */
unsigned int populate_arg (struct apiarg *arg, target_ulong addr, unsigned int after);

#endif /* HANDLERS_H_ */

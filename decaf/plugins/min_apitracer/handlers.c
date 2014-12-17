/*
 * handlers.c
 *
 *  Created on: Jun 11, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 */
#include <stdio.h>
#include "DECAF_main.h"
#include "hookapi.h"
#include "vmi_c_wrapper.h"
#include "parser.h"
#include "handlers.h"

extern struct monitored_proc mon_proc;

void (*spec_handlers[]) (struct api_entry *) = {
										stdcall_handler, 	/* Corresponds to stdcall  */
										cdecl_handler,   	/* Corresponds to cdecl    */
										fastcall_handler, 	/* Corresponds to fastcall */
										thiscall_handler 	/* Corresponds to thiscall */
								   };

/* The following apis get the data as offsets from from fs:base */
/* Get the 'current' thread's thread id */
static inline uint32_t get_this_tid(void)
{
	uint32_t fs_base = ((SegmentCache *)(cpu_single_env->segs + R_FS))->base;
	uint32_t tid = 0;
	DECAF_read_mem(cpu_single_env, fs_base+0x24,4, (uint8_t *)&tid);
	return tid;
}

void convertval2str (char *store, struct apiarg *arg)
{
	switch(arg->type) {
		case IGNORE:
			sprintf(store, "IGNORED");
			break;
		case U8:
			if(arg->spec == in) {
				sprintf(store, "%02x", arg->value_before.val_8);
			} else if(arg->spec == out) {
				sprintf(store, "%02x", arg->value_after.val_8);
			} else if(arg->spec == inout) {
				sprintf(store, "before:%02x after:%02x", arg->value_before.val_8, arg->value_after.val_8);
			}
			break;
		case U16:
			if(arg->spec == in) {
				sprintf(store, "%04x", arg->value_before.val_16);
			} else if(arg->spec == out) {
				sprintf(store, "%04x", arg->value_after.val_16);
			} else if(arg->spec == inout) {
				sprintf(store, "before:%04x after:%04x", arg->value_before.val_16, arg->value_after.val_16);
			}
			break;
		case U32:
			if(arg->spec == in) {
				sprintf(store, "%08x", arg->value_before.val_32);
			} else if(arg->spec == out) {
				sprintf(store, "%08x", arg->value_after.val_32);
			} else if(arg->spec == inout) {
				sprintf(store, "before:%08x after:%08x", arg->value_before.val_32, arg->value_after.val_32);
			}
			break;
		case U64:
			if(arg->spec == in) {
				sprintf(store, "%llx", arg->value_before.val_64);
			} else if(arg->spec == out) {
				sprintf(store, "%llx", arg->value_after.val_64);
			} else if(arg->spec == inout) {
				sprintf(store, "before:%llx after:%llx", arg->value_before.val_64, arg->value_after.val_64);
			}
			break;

		case WSTR:
		case CSTR:
		case USTR:
			if(arg->spec == in)
				strcpy(store, arg->value_before.str);
			else if(arg->spec == out)
				strcpy(store, arg->value_after.str);
			else if(arg->spec == inout)
				sprintf(store, "before:%s after:%s", arg->value_before.str, arg->value_after.str);
		default:
			break;
		}
}

#define ENTRY_SIZE 2048

void populate_trace_str(struct api_entry *api, uint32_t pid, uint32_t tid)
{
	char trace_entry[ENTRY_SIZE] = {'\0'};
	char temp[512] = {'\0'};
	char data_str[512] = {'\0'};
	char mon_proc_temp[256] = {'\0'};
	struct apiarg *args;
	int i;
	tmodinfo_t *modinfo;
	target_ulong pc = 0;
	struct mem_region *mr = NULL;

	modinfo = (tmodinfo_t *)malloc(sizeof(tmodinfo_t));
	VMI_locate_module_c(cpu_single_env->eip, cpu_single_env->cr[3], mon_proc_temp,modinfo);

	args = (struct apiarg *) api->args;
	if(modinfo) {
		sprintf(trace_entry, "PID:%d, TID:%d. Called from: %s:0x%08x, %s:%s( ", pid, tid, modinfo->name, cpu_single_env->eip, api->modname, api->apiname);
	} else {
		sprintf(trace_entry, "PID:%d, TID:%d, %s:%s( ", pid, tid, api->modname, api->apiname);
	}
	for(i = 0; i < api->numargs; i++) {
		memset(data_str, 0, sizeof(data_str));
		memset(temp, 0, sizeof(temp));

		 if(args[i].tostr) {
			 args[i].tostr(data_str, (struct apiarg *)&(args[i]));
		 } else {
			 convertval2str(data_str, (struct apiarg *)&(args[i]));
		 }

		sprintf(temp, "%s=%s", args[i].name, data_str);
		strcat(trace_entry, temp);
		if(i != api->numargs - 1)
			strcat(trace_entry, ", ");
	}
	strcat(trace_entry, "), RET: ");
	memset(data_str, 0, sizeof(data_str));
	memset(temp, 0, sizeof(temp));
	if(api->retarg.type == IGNORE) {
		sprintf(temp, "%s=IGNORED", args[i].name);
		strcat(trace_entry, temp);
	} else if (api->retarg.type == VOID) {
		sprintf(temp, "VOID");
		strcat(trace_entry, temp);
	} else {
		if(api->retarg.tostr) {
			api->retarg.tostr(data_str, &(api->retarg));
		} else {
			convertval2str(data_str, &(api->retarg));
		}

		sprintf(temp, "%s=%s\n", api->retarg.name, data_str);
		strcat(trace_entry, temp);
	}

	fputs(trace_entry, mon_proc.tracefile);
}

static inline int readu8(target_ulong addr, void *buf)
{
	if(!DECAF_read_mem(cpu_single_env, addr, 1,buf))
		return 1;
	return 0;
}

static inline int readu16(target_ulong addr, void *buf)
{
	if(!DECAF_read_mem(cpu_single_env, addr,2, buf))
		return 2;
	return 0;
}

static inline int readu32(target_ulong addr, void *buf)
{
	if(!DECAF_read_mem(cpu_single_env, addr,4, buf))
		return 4;
	return 0;
}

static inline int readu64(target_ulong addr, void *buf)
{
	if(!DECAF_read_mem(cpu_single_env, addr,8, buf))
		return 8;
	return 0;
}

int readwstr(target_ulong addr, void *buf)
{
	//bytewise for now, perhaps block wise later.
	char *store = (char *) buf;
	int i = -1;
	do {
		if(++i == MAX_NAME_LENGTH)
			break;

		if(DECAF_read_mem(cpu_single_env, addr+(2*i), 1,(uint8_t *)&store[i]) < 0) {
			store[i] = '\0';
			return i;
		}

	} while (store[i] != '\0');

	if(i == MAX_NAME_LENGTH) {
		store[i-1] = '\0';
	}
	return i-1;
}

int readcstr(target_ulong addr, void *buf)
{
	//bytewise for now, perhaps block wise later.
	char *store = (char *) buf;
	int i = -1;
	do {
		if(++i == MAX_NAME_LENGTH)
			break;

		if(DECAF_read_mem(cpu_single_env, addr+i, 1,(uint8_t *)&store[i]) < 0) {
			store[i] = '\0';
			return i;
		}

	} while (store[i] != '\0');

	if(i == MAX_NAME_LENGTH) {
		store[i-1] = '\0';
	}
	return i-1;
}


int readustr(uint32_t addr, void *buf)
{
	    uint32_t unicode_data[2];
		int unicode_len = 0;
		int j, i;
		uint8_t unicode_str[MAX_UNICODE_LENGTH] = {'\0'};
		char *store = (char *) buf;
		CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	    if(DECAF_read_mem(env, addr,  sizeof(unicode_data),(uint8_t *)&unicode_data) < 0) {
	            printf("DECAF_read_mem(0x%08x, %d) returned non-zero.\n", addr, sizeof(unicode_data));
	            store[0] = '\0';
	            goto done;
	    }

	    unicode_len = (unsigned int)((unicode_data[0]>>16) & 0xFFFF);
	    if (unicode_len > MAX_UNICODE_LENGTH)
	            unicode_len = MAX_UNICODE_LENGTH;

	    if(DECAF_read_mem(env, unicode_data[1],unicode_len,(uint8_t *)unicode_str) < 0) {
	            monitor_printf(default_mon, "DECAF read mem(cpu, 0x%08x, 0x%08x, %d, 0) returned non-zero.\n", unicode_data[1], (target_ulong)unicode_str, unicode_len);
	            store[0] = '\0';
	            goto done;
	    }

	    for (i = 0, j = 0; i < unicode_len; i+=2, j++) {
	            store[j] = unicode_str[i];
	    }
	    store[j] = '\0';

	done:
	    return strlen(store);
}


unsigned int populate_arg (struct apiarg *arg, target_ulong vaddr, unsigned int after)
{
	union data_holder *dest;
	int i, ret = 0;
	uint32_t addr=0;

	if(arg->size == 0 || arg->type == IGNORE)
		return arg->size;

	if(after == 0) {
		dest = &(arg->value_before);
		if(arg->type == CSTR || arg->type == USTR || arg->type == WSTR)
			DECAF_read_mem(cpu_single_env, vaddr, 4,(uint8_t *)&addr);
	} else {
		dest = &(arg->value_after);
		addr = (uint32_t) arg->temp;
	}

	switch(arg->type) {
	case U8:
		ret = readu8(addr, &(dest->val_8));
		break;
	case U16:
		ret = readu16(addr, &(dest->val_16));
		break;
	case U32:
		ret = readu32(addr, &(dest->val_32));
		break;
	case U64:
		ret = readu64(addr, &(dest->val_64));
		break;
	case WSTR:
		dest->str =malloc (sizeof(char) * MAX_NAME_LENGTH);
		ret = (readwstr(addr, dest->str) > 0)? sizeof(target_ulong): 0;
		break;
	case CSTR:
		dest->str =malloc (sizeof(char) * MAX_NAME_LENGTH);
		ret = (readcstr(addr, dest->str) > 0)? sizeof(target_ulong): 0;
		break;
	case USTR:
		dest->str =malloc (sizeof(char) * MAX_UNICODE_LENGTH);
		ret = (readustr(addr, dest->str) > 0)? sizeof(target_ulong): 0;
		break;
	default:
		break;
	}

	return ret;
}


void cdecl_ret_handler(void *opaque)
{
	int i;
	uint32_t esp, offset;
	struct api_entry *api;
	struct apiarg *args;
	int bytes_read;
	CPUState *cpu;
	uint32_t tid;

	bytes_read = 0;
	api = (struct api_entry *) opaque;
	cpu = cpu_single_env;

	/* Populate the return value */
	if(api->retarg.size > 4) { //Ret val in edx:eax
		api->retarg.value_after.val_64 = cpu->regs[R_EDX];
		api->retarg.value_after.val_64 <<= 32;
		api->retarg.value_after.val_64 |= cpu->regs[R_EAX];
	} else if (api->retarg.size > 0) {
		api->retarg.value_after.val_32 = cpu->regs[R_EAX];
	}

	/* Populate out arguments */
	esp = cpu->regs[R_ESP];
	//This is true if caller cleans the stack.
	offset = esp;
	args = (struct apiarg *) api->args;
	for(i = 0; i < api->numargs; i++) {
		switch(args[i].spec) {
		case out:
		case inout:
			if(args[i].func) {
				bytes_read = args[i].func((struct apiarg *)&args[i], (target_ulong)args[i].temp, 1);
			} else {
				bytes_read = populate_arg((struct apiarg *)&args[i], (target_ulong)(args[i].temp), 1);
			}
			offset += args[i].size;
			break;
		case in:
			offset += args[i].size;
			break;
		default:
			break;
		}
	}

	tid = get_this_tid();
	populate_trace_str(api, VMI_find_pid_by_cr3_c(cpu_single_env->cr[3]), tid);

	hookapi_remove_hook(api->hook_handle);

	//Free this instance
	free(api);
}

/*
 * WINAPI calling convention
 * Stack cleanup by called function
 * Return value in eax
 */
void stdcall_handler (struct api_entry *api)
{
	cdecl_handler(api);
}


/*  __cdecl (compiler option /Gd)
 * Return value in eax
 * Stack cleanup by caller
 */
void cdecl_handler(struct api_entry *api_act)
{
	int i;
	uint32_t esp, ebp;
	target_ulong ret_addr, offset;
	int bytes_read;
	struct apiarg *args;
	CPUState *cpu;

	//A new api instance.
	struct api_entry *api = (struct api_entry *) malloc (sizeof(struct api_entry) + api_act->numargs * sizeof(struct apiarg));
	if(api == NULL) {
		DECAF_printf("Malloc failed in cdecl_handler().\n");
		return;
	}
	memset(api, 0, sizeof(struct api_entry) + api_act->numargs * sizeof(struct apiarg));
	memcpy(api, api_act, sizeof(struct api_entry) + api_act->numargs * sizeof(struct apiarg));

	if(api->iskernel)
		return;

	bytes_read = 0;
	cpu = cpu_single_env;
	args = (struct apiarg *) (api->args);
	esp = cpu->regs[R_ESP];
	ebp = cpu->regs[R_EBP];
	DECAF_read_mem(cpu_single_env, esp, 4,(uint8_t *)&ret_addr);
	api->hook_handle = hookapi_hook_return(ret_addr, cdecl_ret_handler, api, sizeof(struct api_entry) + api->numargs * sizeof(struct apiarg));

	offset = esp + sizeof(target_ulong); //ret addr
	for(i = 0; i < api->numargs; i++) {
		switch(args[i].spec) {

		case inout:
			readu32(offset, &(args[i].temp));
			; //pass through
		case in:
			//If there is a special interpreter function, invoke it.
			if(args[i].func) {
				bytes_read = args[i].func(&args[i], offset, 0);
			} else {
				bytes_read = populate_arg(&args[i], offset, 0);
			}
			offset += args[i].size;//EK bytes_read;
			break;
		case out:
			readu32(offset, (void *)&(args[i].temp));
			offset += 4;
			break;
		default:
			break;
		}
	}
}

/*
 * __fastcall (compiler option /Gr)
 * args 1 and 2 go in ecx and edx. Rest pushed from right to left on stack.
 * Return value in eax
 * stack cleanup by called function
 */
void fastcall_handler (struct api_entry *api)
{

}


/*
 * Default for calling c++ member functions (except when var args)
 * this ptr in ecx. Args on stack.
 * Stack cleanup by called function.
 */
void thiscall_handler(struct api_entry *api)
{

}

void gen_api_handler(void *opaque)
{
	struct api_entry *api, *instance;
	api = (struct api_entry *) opaque;

	//Create a copy for this instance.
	instance = (struct api_entry *) malloc (sizeof(struct api_entry) + api->numargs * sizeof(struct apiarg));
	memcpy(instance, api, sizeof(struct api_entry) + api->numargs * sizeof(struct apiarg));
	spec_handlers[api->conv]((void*) instance);
}


void gen_ret_handler(void * opaque)
{
	cdecl_ret_handler(opaque);
}

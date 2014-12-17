/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU GPL, version 3 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
#if 0 //comment off for now until new tainting is implemented --Heng
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "DECAF_main_x86.h"
#include "procmod.h"
#include "hooks/function_map.h"
#include "shared/hookapi.h"
#include "shared/read_linux.h"
#include "shared/tainting/taintcheck.h" // AWH

#if 1 // AWH TAINT_ENABLED
#include "shared/tainting/reduce_taint.h"

//LOK: Need to patch these function calls to use CPUState* env
// Fixed the warning with hookapi_proc_t expecting void return
// but these functions use int returns
static void ExAllocatePoolWithTag_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 12, 0, NULL);
}

static void RtlAllocateHeap_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 12, 0, NULL);
}

static void RtlReAllocateHeap_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 16, 0, NULL);
}

static void ExInterlockedPushEntryList_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 12, 0, NULL);
  uint32_t vaddr;
  DECAF_read_mem(NULL, DECAF_cpu_regs[R_ESP] + 4, 4, &vaddr); // first argument
  taintcheck_taint_virtmem(NULL, vaddr, 8, 0, NULL); //clean it
  DECAF_read_mem(NULL, DECAF_cpu_regs[R_ESP] + 8, 4, &vaddr); // second argument
  taintcheck_taint_virtmem(NULL, vaddr, 8, 0, NULL); //clean it
}

static void InterlockedPushEntrySList_call(void *opaque)
{
  //fastcall
  taintcheck_reg_clean(R_ECX, 0, 4);
  taintcheck_reg_clean(R_EDX, 0, 4);
  uint32_t vaddr;
  DECAF_read_mem(NULL, DECAF_cpu_regs[R_ECX], 4, &vaddr); // first argument
  taintcheck_taint_virtmem(NULL, vaddr, 8, 0, NULL); //clean it
  DECAF_read_mem(NULL, DECAF_cpu_regs[R_EDX], 4, &vaddr); // second argument
  taintcheck_taint_virtmem(NULL, vaddr, 8, 0, NULL); //clean it
}

static void alloca_probe_ret(void *opaque)
{
  uint32_t *handle = (uint32_t *)opaque;
  hookapi_remove_hook(*handle);
  free(handle);
  taintcheck_reg_clean(R_ESP, 0, 4);
}

static void alloca_probe_call(void *opaque)
{
  uint32_t ret_eip;
  DECAF_read_mem(NULL, DECAF_cpu_regs[R_ESP], 4, &ret_eip);
  uint32_t *hook_handle = malloc(sizeof(uint32_t));
  if(hook_handle) {
    *hook_handle = hookapi_hook_return(ret_eip, alloca_probe_ret, 
			hook_handle, sizeof(uint32_t));
  }
}

static void _aligned_offset_malloc_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 12, 0, NULL);
}

static void _aligned_offset_realloc_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 16, 0, NULL);
}

static void calloc_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 8, 0, NULL);
}

static void malloc_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 4, 0, NULL);
}

static void realloc_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 8, 0, NULL);
}

static void NtAllocateVirtualMemory_call(void *opaque)
{
  taintcheck_taint_virtmem(NULL, DECAF_cpu_regs[R_ESP]+4, 24, 0, NULL);
}

void reduce_taint_init(void)
{
  hookapi_hook_function_byname("ntoskrnl.exe", "ExAllocatePoolWithTag", 
  		1, 0, ExAllocatePoolWithTag_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "RtlAllocateHeap", 
  		1, 0, RtlAllocateHeap_call, 0, 0);
  hookapi_hook_function_byname("ntdll.dll", "RtlAllocateHeap", 
  		1, 0, RtlAllocateHeap_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "NtAllocateVirtualMemory",
		 1, 0, NtAllocateVirtualMemory_call, 0, 0);
  hookapi_hook_function_byname("ntdll.dll", "NtAllocateVirtualMemory",
		 1, 0, NtAllocateVirtualMemory_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "ZwAllocateVirtualMemory",
		 1, 0, NtAllocateVirtualMemory_call, 0, 0);
  hookapi_hook_function_byname("ntdll.dll", "RtlReAllocateHeap", 
  		1, 0, RtlReAllocateHeap_call, 0, 0);

  hookapi_hook_function_byname("ntoskrnl.exe", "ExInterlockedPushEntryList", 
  		1, 0, ExInterlockedPushEntryList_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "ExInterlockedInsertHeadList", 
  		1, 0, ExInterlockedPushEntryList_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "ExInterlockedInsertTailList", 
  		1, 0, ExInterlockedPushEntryList_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "InterlockedPushEntrySList", 
  		1, 0, InterlockedPushEntrySList_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "ExInterlockedPushEntrySList", 
  		1, 0, InterlockedPushEntrySList_call, 0, 0);
  hookapi_hook_function_byname("ntdll.dll", "_alloca_probe", 1, 0,
			alloca_probe_call, 0, 0);
  hookapi_hook_function_byname("ntoskrnl.exe", "_alloca_probe", 1, 0,
			alloca_probe_call, 0, 0);

  hookapi_hook_function_byname("msvcrt.dll", "_aligned_offset_malloc", 0,
		 1, _aligned_offset_malloc_call, 0, 0);
  hookapi_hook_function_byname("msvcrt.dll", "_aligned_offset_realloc", 0,
		 1, _aligned_offset_realloc_call, 0, 0);
  hookapi_hook_function_byname("msvcrt.dll", "calloc", 1, 0,
		 calloc_call, 0, 0);  
  hookapi_hook_function_byname("msvcrt.dll", "malloc", 1, 0,
		 malloc_call, 0, 0);  
  hookapi_hook_function_byname("msvcrt.dll", "realloc", 1, 0,
		 realloc_call, 0, 0);  
} 

#endif 

#endif


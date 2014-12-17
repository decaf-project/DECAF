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
/**********************************************************************
 * hookapi.cpp
 * @Author Heng Yin <heyin@syr.edu>
 * This file is responsible for registering and calling function hooks.
 * 
 */ 
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include <limits.h>
#include <sys/queue.h>
#include <string>
#include <map>
#include <list>
#include "hw/hw.h"
#include "qemu-queue.h"
#include "qemu-common.h" // AWH - QEMUFile
#include "DECAF_main.h" // AWH
#include "function_map.h"
#include "hookapi.h"
#include "DECAF_callback.h"
#include "DECAF_types.h"
#include "DECAF_target.h"

//LOK: Since the old interface registered for ALL possible basic blocks,
// the addition of the optimized basic block callback means that
// the whole logic must be updated to use the optimized basic block
// callback infrastructure
//At this early stage, we are not going to implement any optimizations
// based on caching (more on this later) and are just going to register
// and unregister the callbacks dynamically. So this means that
// a user can hook a function by name (causing a new basic block callback
// to be registered), and then hook the return value (causing a new basic block
// to be registered), which is then called and the return value unhooked
// (which causes the target basic block to be unregistered and potentially flushed).
// If this whole process took place within a function, then the registering and
// unregistering of the return values will lead to many basic block flushes
// which does not seem like a good idea. A delayed flush or usage count would
// help make this better - however it also has overhead.

using namespace std;

typedef struct hookapi_record{
  uint32_t eip;
  int is_global;
  uint32_t esp; //for hooking function return
  uint32_t cr3; //for hooking function return
  hook_proc_t fnhook;
  void *opaque;
  uint32_t sizeof_opaque;
  //LOK: Added an entry for the DECAF callback handle
  DECAF_Handle cbhandle;
  QLIST_ENTRY(hookapi_record) link;
} hookapi_record_t;

typedef struct hookapi_handle{
  uintptr_t handle;
  QLIST_ENTRY(hookapi_handle) link;
} hookapi_handle_t;

typedef struct {
  string module;
  string function;
  hookapi_record_t* record;
}fnhook_info_t;

static list<fnhook_info_t> fun_to_hook;

#define HOOKAPI_HTAB_SIZE 256
QLIST_HEAD(hookapi_record_list_head, hookapi_record) 
	hookapi_record_heads[HOOKAPI_HTAB_SIZE];

QLIST_HEAD(hookapi_handle_list_head, hookapi_handle) hookapi_handle_head = 
	QLIST_HEAD_INITIALIZER(&hookapi_handle_head);

static inline void hookapi_insert(hookapi_record_t *record)
{
  struct hookapi_record_list_head *head =
      &hookapi_record_heads[record->eip & (HOOKAPI_HTAB_SIZE - 1)];
  
  QLIST_INSERT_HEAD(head, record, link); 
  
  hookapi_handle_t *handle_info = (hookapi_handle_t *)g_malloc(sizeof(hookapi_handle_t));
  if(handle_info) {
    handle_info->handle = (uintptr_t)record;
    QLIST_INSERT_HEAD(&hookapi_handle_head, handle_info, link);
  }
}

static void hookapi_remove_all(void)
{
	hookapi_record_t *hrec;
	int i;

	for(i = 0; i<HOOKAPI_HTAB_SIZE; i++) {
		struct hookapi_record_list_head * head = &hookapi_record_heads[i];
		while(!QLIST_EMPTY(head)) {
			hrec = QLIST_FIRST(head);
			//LOK: Before we remove the record we must first unregister the bb callback
			if (hrec->cbhandle == DECAF_NULL_HANDLE)
			{
				fprintf(stderr, "ERROR: in hookapi_remove_all: We have a NULL handle for a bb callback\n");
			}
			DECAF_unregisterOptimizedBlockBeginCallback(hrec->cbhandle);
			QLIST_REMOVE(hrec, link);
			if(hrec->opaque != 0 && (uintptr_t)(hrec->opaque) != 1)
			{
				//Normally, it is up to the user to free this opaque record. However, here we need to clean up
				//all the hooks, which the user forgets to remove or does not get a chance to.
				g_free(hrec->opaque);
			}
			g_free(hrec);
		}
	}
  
	hookapi_handle_t *handle_info;
	while(!QLIST_EMPTY(&hookapi_handle_head)) {
		handle_info = QLIST_FIRST(&hookapi_handle_head);
		QLIST_REMOVE(handle_info, link);
		g_free(handle_info);
	}

	//LOK: We can no longer just do clear - must first free the temporary records that we created before
	list<fnhook_info_t>::iterator iter = fun_to_hook.begin();
	while (iter != fun_to_hook.end())
	{
		g_free(iter->record);
		iter->record = NULL;
		iter = fun_to_hook.erase(iter);
	}

	fun_to_hook.clear();
}

static void hookapi_save(QEMUFile *f, void *opaque)
{
  hookapi_record_t *hrec;
  int i;
  Dl_info info;
  uint32_t len;
  
  for(i = 0; i < HOOKAPI_HTAB_SIZE; i++) {
    QLIST_FOREACH(hrec, &hookapi_record_heads[i], link) {
      qemu_put_be32(f, hrec->eip);
      
      if(dladdr((void *)hrec->fnhook, &info) == 0) {
        fprintf(stderr, "%s\n", dlerror());
        return;
      }
      len = strlen(info.dli_fname) + 1;
      qemu_put_be32(f, len);
      qemu_put_buffer(f, (uint8_t*)info.dli_fname, len); //module name
      qemu_put_be32(f, (uint32_t)((uintptr_t)hrec->fnhook - (uintptr_t)info.dli_fbase)); //relative address
      qemu_put_be32(f, (uint32_t)hrec->is_global);
      qemu_put_be32(f, hrec->esp);
      qemu_put_be32(f, hrec->cr3);
      qemu_put_be32(f, (uint32_t)(uintptr_t)hrec->opaque); //hacky!!!
      qemu_put_be32(f, hrec->sizeof_opaque);
      if(hrec->sizeof_opaque)
      {
        qemu_put_buffer(f, (uint8_t *)hrec->opaque, hrec->sizeof_opaque);
      }
      //LOK: We do not save the handle since it must be registered again at load
      qemu_put_be32(f, 0);
      qemu_put_byte(f, 0xff); //separator
    }
  }

  qemu_put_be32(f, 0); //terminator

  //TODO: save fun_to_hook
}


//This function is called internally by DECAF to check and invoke function hooks.
//Note that on the same execution point, there may be multiple hooks.
//Also note that a hook on function return must consider its thread context, because in a
//multi-threading context, the same return address may be hooked in several threads.
static void hookapi_check_hook(DECAF_Callback_Params* params)
{
        if(cpu_single_env == NULL) return;

        target_ulong pc = DECAF_getPC(cpu_single_env);
        target_ulong pgd = DECAF_getPGD(cpu_single_env);

    struct hookapi_record_list_head *head =
              &hookapi_record_heads[pc & (HOOKAPI_HTAB_SIZE - 1)];
        hookapi_record_t *record, *tmp;

        //NOTE: this is a safe version of QLIST_FOREACH
        for(record = head->lh_first; record;  record = tmp) {
                tmp = record->link.le_next;

            if(record->eip != pc)
                continue;
            
            //On the same execution point, there might be multiple callbacks registered.
            //So we have to check the handle to make sure we act on the same callback
            if(record->cbhandle != params->cbhandle)
                continue;    

            //check if this hook is only for a specific memory space
            if(record->cr3 != 0 && record->cr3!=pgd)
                continue;

            //check if this is a global hook
            //if it is a local hook, then should_monitor must be true
            if(!record->is_global && ! should_monitor)
                continue;

            //check if this is a return hook.
            //the recorded ESP must be close enough to the current ESP.
            //otherwise, it must be a return hook for the same function call in a different thread.
            //The threshold 80 is based on that normally no function has more than 20 arguments.
            if(record->esp && DECAF_getESP(cpu_single_env) - record->esp > 80)
                continue;

            record->fnhook(record->opaque);
        }
}

static int hookapi_load(QEMUFile *f, void *opaque, int version_id)
{
  hookapi_remove_all();
  
  uint32_t eip, len;
  uintptr_t base = 0, relative_addr;
  hookapi_record_t *record;
  
  char mod_name[PATH_MAX];
  void *handle;
  
  while((eip = qemu_get_be32(f))) {
    len = qemu_get_be32(f);
    if(len < 1 || len > PATH_MAX) return -EINVAL;
    
    qemu_get_buffer(f, (uint8_t *)mod_name, len);
    handle = dlopen(mod_name, RTLD_NOLOAD);
    if(NULL == handle) {
      fprintf(stderr, "%s is not loaded \n", mod_name);
      dlclose(handle);
      return -EINVAL;
    }
    void *sym;
    if ((sym = dlsym(handle, "_init"))) {
      Dl_info dli;
      if (dladdr(sym, &dli)) 
        base = (uintptr_t) dli.dli_fbase;
    } else {
      fprintf(stderr, "cannot find base address of %s\n", mod_name);
      dlclose(handle);
      return -EINVAL;
    }
   
	relative_addr = qemu_get_be32(f);

    record = (hookapi_record_t *)g_malloc(sizeof(hookapi_record_t));
    if(record == NULL) {
      fprintf(stderr, "out of memory!\n");
      dlclose(handle);
      return -EINVAL;
    }    

    record->eip = eip;
    record->fnhook = (hook_proc_t)(relative_addr + base);
    record->is_global = (int)qemu_get_be32(f);
    record->esp = qemu_get_be32(f);
    record->cr3 = qemu_get_be32(f);
    record->opaque = (void *)(uintptr_t)qemu_get_be32(f);
    record->sizeof_opaque = qemu_get_be32(f);
    if(record->sizeof_opaque) {
      if(NULL == (record->opaque = g_malloc(record->sizeof_opaque))) {
        fprintf(stderr, "out of memory: size=%d\n", record->sizeof_opaque);
        dlclose(handle);
        g_free(record);
        return -EINVAL;
      }
      qemu_get_buffer(f, (uint8_t *)record->opaque, record->sizeof_opaque);
    }      
    uint32_t cb = qemu_get_be32(f);
    if (cb != 0)
    {
      dlclose(handle);
      g_free(record->opaque);
      g_free(record);
      return(-EINVAL);
    }

    uint8_t separator = qemu_get_byte(f);
    if(separator != 0xff) {
      dlclose(handle);
      g_free(record->opaque);
      g_free(record);
      return -EINVAL; 
    } 

    record->cbhandle = DECAF_registerOptimizedBlockBeginCallback(&hookapi_check_hook, NULL, record->eip, OCB_CONST);
    if (record->cbhandle == DECAF_NULL_HANDLE)
    {
      dlclose(handle);
      g_free(record->opaque);
      g_free(record);
      return(-EINVAL);
    }

    hookapi_insert(record);

    dlclose(handle);
  }

  //TODO: load fun_to_hook

  return 0;
}




/* This function flushes the hooks that a plugin might have registered during plugin unload */
void hookapi_flush_hooks(char *plugin_path)
{
	hookapi_handle_t *handle_info, *next;
//	hookapi_record_t *record, *tmp;
	const char *name;
//	int i = 0;
	Dl_info dl;

//	struct hookapi_record_list_head *head;

	QLIST_FOREACH_SAFE(handle_info, &hookapi_handle_head, link, next) {
		hookapi_record_t *record = (hookapi_record_t *)(handle_info->handle);
		if(dladdr((void *)(((uintptr_t)(record->fnhook))+1), &dl) != 0) { //does not return error
			name = strrchr(dl.dli_fname, '/') ?
					strrchr(dl.dli_fname, '/')+1 : dl.dli_fname;
			if(strstr(plugin_path, name) != NULL) { //this hook belongs to the plugin being unloaded
				//we found this handle
		//		monitor_printf(default_mon, "Unregistering hook: %s, %s\n", dl.dli_fname, dl.dli_sname);
				QLIST_REMOVE(handle_info, link); //remove the handle from the hook_handle_table
				g_free(handle_info);

				DECAF_unregisterOptimizedBlockBeginCallback(record->cbhandle);
				QLIST_REMOVE(record, link); //Remove the record
				//here, we do not g_free record->opaque, because caller should g_free it
				g_free(record);
			}
		}
	}
}



void init_hookapi(void)
{
  int i;
  for (i = 0; i < HOOKAPI_HTAB_SIZE; i++)
    QLIST_INIT(&hookapi_record_heads[i]);

  //LOK: We no longer register for ALL basic block callbacks
  //block_begin_handle = DECAF_register_callback(DECAF_BLOCK_BEGIN_CB, hookapi_check_hook, NULL);

  register_savevm(NULL, "hookapi", 0, 2, hookapi_save, hookapi_load, NULL);
}


void hookapi_cleanup(void)
{
  hookapi_remove_all();  
  unregister_savevm(NULL, "hookapi", 0);
  //LOK: Since we don't register for ALL basic block callbacks
  // We don't have to unregister the generic one -- block_begin_handle.
  // We unregister the individual handles instad
  //DECAF_unregister_callback(DECAF_BLOCK_BEGIN_CB, block_begin_handle);

}

uintptr_t hookapi_hook_function(
               int is_global,
               target_ulong pc,
               target_ulong cr3,
               hook_proc_t fnhook, 
               void *opaque, 
               uint32_t sizeof_opaque
               )
{
  hookapi_record_t *record = (hookapi_record_t *)g_malloc(sizeof(hookapi_record_t));

  if (record == NULL)
    return 0;

  record->eip = pc;
  record->cr3 = cr3;
  record->is_global = is_global;
  record->fnhook = fnhook;
  record->sizeof_opaque = sizeof_opaque;
  record->opaque = opaque;
  record->esp = 0; //esp is only used for return hook
  record->cbhandle = DECAF_registerOptimizedBlockBeginCallback(&hookapi_check_hook, NULL, pc, OCB_CONST);
  if (record->cbhandle == DECAF_NULL_HANDLE)
  {
    g_free(record);
    return (0);
  }

  hookapi_insert(record);
  return (uintptr_t)record;
}

uintptr_t 
hookapi_hook_return(
               target_ulong pc,
               hook_proc_t fnhook, 
               void *opaque, 
               uint32_t sizeof_opaque
               )
{
  hookapi_record_t *record = (hookapi_record_t *)g_malloc(sizeof(hookapi_record_t));
  if (record == NULL)
    return 0;

  record->eip = pc;
  record->is_global = 0;

  record->esp = DECAF_getESP(cpu_single_env);
  record->cr3 = DECAF_getPGD(cpu_single_env);

  record->fnhook = fnhook;
  record->sizeof_opaque = sizeof_opaque;
  record->opaque = opaque;

  record->cbhandle = DECAF_registerOptimizedBlockBeginCallback(&hookapi_check_hook, NULL, pc, OCB_CONST);
  if (record->cbhandle == DECAF_NULL_HANDLE)
  {
    g_free(record);
    return (0);
  }

  hookapi_insert(record);
  return (uintptr_t)record;
}


void hookapi_remove_hook(uintptr_t handle)
{
  //we need to sanitize this handle. 
  //check if this handle exists in our handle list
  hookapi_handle_t *handle_info;

  //FIXME: not safe for multi-threading
  QLIST_FOREACH(handle_info, &hookapi_handle_head, link) {
    if(handle_info->handle != handle)
      continue;

    //we found this handle
    QLIST_REMOVE(handle_info, link);
    g_free(handle_info);
    hookapi_record_t *record = (hookapi_record_t *)handle;
    //LOK: Before we remove it, we need to unregister the handle
    if (record->cbhandle != DECAF_NULL_HANDLE)
    {
      DECAF_unregisterOptimizedBlockBeginCallback(record->cbhandle);
    }
    else
    {
      fprintf(stderr, "ERROR: in hookapi_remove_hook: The callback handle is NULL - it should not be!\n");
    }

    QLIST_REMOVE(record, link);
    //here, we do not g_free record->opaque, because caller should g_free it
    g_free(record);
    return;
  }

  //In the case that the handle is still waiting to be hooked
  list<fnhook_info_t>::iterator iter = fun_to_hook.begin();
  for ( ; iter != fun_to_hook.end(); iter++)
  {
    if (handle == (uintptr_t)iter->record)
    {
      //LOK: In this case the callback should never have been registered in the first place
      // so there is no reason to free it. We can double check though
      if (iter->record->cbhandle != DECAF_NULL_HANDLE)
      {
        fprintf(stderr, "ERROR: in hookapi_remove_hook: A record for a function waiting to be hooked already has a handle\n");
      }
      g_free(iter->record);
      iter->record = NULL;
      iter = fun_to_hook.erase(iter);
      return;
    }
  }

  assert(0); //this handle is invalid
}


static uintptr_t add_unresolved_hook(const char *module_name,
                     const char *function_name,
                     hook_proc_t hookfn,
                     int is_global,
                     target_ulong cr3,
                     void* opaque,
                     uint32_t sizeof_opaque)
{
  //LOK: Preallocating the hook handle so we can return the handler as part of this function
  hookapi_record_t *record = (hookapi_record_t *)g_malloc(sizeof(hookapi_record_t));
  if (record == NULL)
    return 0;

  record->eip = 0;
  record->esp = 0;
  record->is_global = is_global;
  record->cr3 = cr3;
  record->fnhook = hookfn;
  record->sizeof_opaque = sizeof_opaque;
  record->opaque = opaque;
  //LOK: Make sure that the callback handle is NULL
  record->cbhandle = DECAF_NULL_HANDLE;

  fnhook_info_t info;
  info.module = module_name;
  info.function = function_name;
  info.record = record;
  fun_to_hook.push_back(info);

  return (uintptr_t)record;
}


void check_unresolved_hooks()
{
	list<fnhook_info_t>::iterator iter;
	for(iter=fun_to_hook.begin(); iter!=fun_to_hook.end(); iter++) {
		target_ulong pc = funcmap_get_pc(iter->module.c_str(), iter->function.c_str(), iter->record->cr3);
		if (pc == 0) continue;

		//since the entry is found - we update the eip
                iter->record->eip = pc;
                //LOK: We also register for the new optimized block begin callback
                iter->record->cbhandle = DECAF_registerOptimizedBlockBeginCallback(&hookapi_check_hook, NULL, pc, OCB_CONST);
                if (iter->record->cbhandle == DECAF_NULL_HANDLE)
                {
                  //if for some reason we couldn't register it - it is probably a realy bad sign
                  //Should we continue?
                  fprintf(stderr, "ERROR: in check_unresolved_hooks: Could not register the new block begin callback\n");
                  iter->record->eip = 0;
                  continue;
                }
                hookapi_insert(iter->record); 

		monitor_printf(default_mon, "Hooking %s::%s at 0x%08x\n",
				iter->module.c_str(), iter->function.c_str(), pc);
//LOK: We can still just do an erase here since the remove will free the record
		iter = fun_to_hook.erase(iter);
		iter--;
	}
}

// uintptr_t hookapi_hook_function_byname(const char *mod_name, const char *fun_name, int is_global, target_ulong cr3, hook_proc_t fnhook, void *opaque, uint32_t sizeof_opaque) __attribute__((optimize("O0")));
uintptr_t hookapi_hook_function_byname(const char *mod_name, const char *fun_name,
                    int is_global, target_ulong cr3, hook_proc_t fnhook, void *opaque, uint32_t sizeof_opaque)
{
  target_ulong pc = funcmap_get_pc(mod_name, fun_name, cr3);
  if (pc == 0) {
    printf("Deferring hooking of %s::%s\n", mod_name, fun_name);
    return (add_unresolved_hook(mod_name, fun_name, fnhook, is_global, cr3, opaque, sizeof_opaque));
  }
  else {
    printf("Hooking %s::%s @ 0x%x\n", mod_name, fun_name, pc);
    return (hookapi_hook_function(is_global, pc, cr3, fnhook, opaque, sizeof_opaque));
  }
}




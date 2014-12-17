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
 * hookapi.h
 * Author: Heng Yin <heyin@syr.edu>
 *
 * This file is responsible for registering and calling function hooks.
 * 
 */ 
////////////////////////////////////
/// @defgroup hookapi hookapi: Function Hooking Facility
///

#ifndef HOOKAPI_H_INCLUDED
#define HOOKAPI_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void (*hook_proc_t)(void *opaque);

void init_hookapi(void);
void hookapi_cleanup(void);

/// @ingroup hookapi
/// install a hook at the function entry
/// @param is_global flag specifies if this hook should be invoked globally or only in certain execution context
/// @param pc address of function to be hooked (has to be the start of a basic block
/// @param cr3 the memory space that this hook is installed. 0 for all memory spaces.
/// @param fnhook address of function hook
/// @param opaque address of an opaque structure provided by caller (has to be globally allocated)
//  @param sizeof_opaque size of the opaque structure (if opaque is an integer, not a pointer to a structure, sizeof_opaque must be zero) 
/// @return a handle that uniquely specifies this hook 
uintptr_t
hookapi_hook_function(
               int is_global,
               target_ulong pc,
               target_ulong cr3,
               hook_proc_t fnhook, 
               void *opaque, 
               uint32_t sizeof_opaque
               );

/// @ingroup hookapi
/// install a hook at the function exit
/// @param pc function return address
/// @param fnhook address of function hook
/// @param opaque address of an opaque structure provided by caller (has to be globally allocated)
//  @param sizeof_opaque size of the opaque structure (if opaque is an integer, not a pointer to a structure, sizeof_opaque must be zero) 
/// @return a handle that uniquely specifies this hook
/// This function is supposed to be called on function entry. It hooks a function
/// return.
/// The implementation of this function handles multi-threading. It means, when
/// multple threads call the same function and register the same return address, 
/// we can distinguish different hooks, and invoke them correctly.
uintptr_t
hookapi_hook_return(
               target_ulong pc,
               hook_proc_t fnhook, 
               void *opaque, 
               uint32_t sizeof_opaque
               );

/// @ingroup hookapi
/// remove a hook
/// @param handle hook handle returned by hookapi_hook_function or hookapi_hook_return  
/// This function will not free the opaque structure provided by the caller 
/// when installing a hook. The caller is responsible for freeing it.
/// The exception is when the user's plugin is unloaded, the opaque structure is
/// freed automatically to avoid memory leakage. Likewise, when a vm state 
/// (i.e. snapshot) is loaded, the opaque structure is allocated and 
/// restored automatically.  
void hookapi_remove_hook(uintptr_t handle);


/// @ingroup hookapi
/// install a hook at the function entry by specifying module name and function name
/// @param mod module name that this function is located in
/// @param func function name
/// @param is_global flag specifies if this hook should be invoked globally or only in certain execution context (when should_monitor is true)
/// @param cr3 the memory space that this hook is installed. 0 for all memory spaces.
/// @param fnhook address of function hook
/// @param opaque address of an opaque structure provided by caller (has to be globally allocated)
//  @param sizeof_opaque size of the opaque structure (if opaque is an integer, not a pointer to a structure, sizeof_opaque must be zero) 
/// @return a handle that uniquely identifies this hook
/// Note that the handle that is returned, might not actually be active yet - you can check the eip value of the handle to find out
/// the default value is 0.
uintptr_t hookapi_hook_function_byname(
		const char *mod,
		const char *func,
		int is_global,
		target_ulong cr3,
		hook_proc_t fnhook,
		void *opaque,
		uint32_t sizeof_opaque);

//Below are functions called internally within the framework
void check_unresolved_hooks(void);

/* Function to flush all the hooks registered by the plugin (if any) */
void hookapi_flush_hooks(char *plugin_path);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // HOOKAPI_H_INCLUDED


/*
 * vmi_callback.h
 *
 *  Created on: Dec 11, 2013
 *      Author: Xunchao hu
 */

#ifndef VMI_CALLBACK_H_
#define VMI_CALLBACK_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "DECAF_types.h"
typedef enum {
  VMI_CREATEPROC_CB = 0,
  VMI_REMOVEPROC_CB,
  VMI_LOADMODULE_CB,
  VMI_REMOVEMODULE_CB,
  VMI_LOADMAINMODULE_CB,
  VMI_PROCESSBEGIN_CB = VMI_LOADMAINMODULE_CB, //alias
  VMI_LAST_CB, //place holder for the last position, no other uses.
} VMI_callback_type_t;

typedef struct _VMI_CreateProc_Params {
	  uint32_t pid;
	  uint32_t cr3;
	  char *name;
}VMI_CreateProc_Params;

typedef VMI_CreateProc_Params VMI_RemoveProc_Params;

typedef struct _VMI_LoadModule_Params {
	  uint32_t pid;
	  uint32_t cr3;
	  char* name;
	  gva_t base;
	  uint32_t size;
	  char* full_name;
}VMI_LoadModule_Params;

typedef VMI_LoadModule_Params VMI_RemoveModule_Params;

typedef union _VMI_Callback_Params
{
  VMI_CreateProc_Params cp;
  VMI_RemoveProc_Params rp;
  VMI_LoadModule_Params lm;
  VMI_RemoveModule_Params rm;
  //LoadMainModule_Params lmm;
} VMI_Callback_Params;


typedef void (*VMI_callback_func_t) (VMI_Callback_Params* params);

DECAF_Handle VMI_register_callback(
                VMI_callback_type_t cb_type,
                VMI_callback_func_t cb_func,
                int *cb_cond
                );

int VMI_unregister_callback(VMI_callback_type_t cb_type, DECAF_Handle handle);

#ifdef __cplusplus
}
#endif
#endif /* VMI_CALLBACK_H_ */

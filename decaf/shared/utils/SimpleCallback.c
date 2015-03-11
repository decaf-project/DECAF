
/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU LGPL, version 2.1 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
/*
 * SimpleCallback.c
 *
 *  Created on: Oct 16, 2012
 *      Author: lok
 */

#include "SimpleCallback.h"

#ifndef LIST_FOREACH_SAFE
#define LIST_FOREACH_SAFE(var, head, field, tvar)                       \
          for ((var) = LIST_FIRST((head));                                \
              (var) && ((tvar) = LIST_NEXT((var), field), 1);             \
              (var) = (tvar))
#endif

SimpleCallback_t* SimpleCallback_new(void)
{
  SimpleCallback_t* pList = (SimpleCallback_t*) malloc(sizeof(SimpleCallback_t));
  if (pList == NULL)
  {
    return (NULL);
  }
  LIST_INIT(pList);
  return (pList);
}

DECAF_errno_t SimpleCallback_init(SimpleCallback_t* pList)
{
  if (pList == NULL)
  {
    return (NULL_POINTER_ERROR);
  }
  LIST_INIT(pList);
  return (0);
}


DECAF_errno_t SimpleCallback_clear(SimpleCallback_t* pList)
{
  SimpleCallback_entry_t *cb_struct = NULL;

  if (pList == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  while (!LIST_EMPTY(pList))
  {
    LIST_REMOVE(LIST_FIRST(pList), link);
    free(cb_struct);
  }

  return (0);
}

DECAF_errno_t SimpleCallback_delete(SimpleCallback_t* pList)
{
  if (pList == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  SimpleCallback_clear(pList);

  free(pList);

  return (0);
}

//this is for backwards compatibility -
// for block begin and end - we make a call to the optimized versions
// for insn begin and end we just use the old logic
DECAF_Handle SimpleCallback_register(
    SimpleCallback_t* pList,
    SimpleCallback_func_t cb_func,
    int *cb_cond)
{
  if (pList == NULL)
  {
    return (DECAF_NULL_HANDLE);
  }

  SimpleCallback_entry_t* cb_struct = (SimpleCallback_entry_t*)malloc(sizeof(SimpleCallback_entry_t));
  if (cb_struct == NULL)
  {
    return (DECAF_NULL_HANDLE);
  }

  cb_struct->callback = cb_func;
  cb_struct->enabled = cb_cond;

  LIST_INSERT_HEAD(pList, cb_struct, link);

  return (DECAF_Handle)cb_struct;
}

DECAF_errno_t SimpleCallback_unregister(SimpleCallback_t* pList, DECAF_Handle handle)
{
  SimpleCallback_entry_t *cb_struct = NULL, *cb_temp;

  if (pList == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  //FIXME: not thread safe
  LIST_FOREACH_SAFE(cb_struct, pList, link, cb_temp) {
    if((DECAF_Handle)cb_struct != handle)
      continue;

    LIST_REMOVE(cb_struct, link);
    free(cb_struct);

    return 0;
  }

  return (ITEM_NOT_FOUND_ERROR);
}

void SimpleCallback_dispatch(SimpleCallback_t* pList, void* params)
{
  SimpleCallback_entry_t *cb_struct, *cb_temp;

  if (pList == NULL)
  {
    return; // (NULL_POINTER_ERROR);
  }

  //FIXME: not thread safe
  LIST_FOREACH_SAFE(cb_struct, pList, link, cb_temp) {
          // If it is a global callback or it is within the execution context,
          // invoke this callback
          if(!cb_struct->enabled || *cb_struct->enabled)
                  cb_struct->callback(params);
  }
}

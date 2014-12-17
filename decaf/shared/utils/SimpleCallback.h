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
 * SimpleCallback.h
 *
 *  Created on: Oct 16, 2012
 *      Author: lok
 *  The idea behind this file is to create a simple data structure for
 *  maintaining a simple callback interface. In this way the core developers
 *  can expose their own callbacks. For example, procmod can now expose
 *  callbacks for loadmodule instead of having the plugin directly set the
 *  single loadmodule_notifier inside procmod, which is messy
 */

#ifndef SIMPLECALLBACK_H_
#define SIMPLECALLBACK_H_

#include <sys/queue.h>
#include "cpu.h"
#include "shared/DECAF_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*SimpleCallback_func_t) (void* params);

typedef struct SimpleCallback_entry{
        int *enabled;
        SimpleCallback_func_t callback;
        LIST_ENTRY(SimpleCallback_entry) link;
}SimpleCallback_entry_t;

typedef LIST_HEAD(SimpleCallback_list_head, SimpleCallback_entry) SimpleCallback_t;

DECAF_Handle SimpleCallback_register(
    SimpleCallback_t* pList,
    SimpleCallback_func_t cb_func,
    int *cb_cond);

DECAF_errno_t SimpleCallback_unregister(SimpleCallback_t* pList, DECAF_Handle handle);

SimpleCallback_t* SimpleCallback_new(void);

DECAF_errno_t SimpleCallback_init(SimpleCallback_t* pList);

DECAF_errno_t SimpleCallback_delete(SimpleCallback_t* pList);

DECAF_errno_t SimpleCallback_clear(SimpleCallback_t* pList);

void SimpleCallback_dispatch(SimpleCallback_t* pList, void* params);

#ifdef __cplusplus
}
#endif

#endif /* SIMPLECALLBACK_H_ */

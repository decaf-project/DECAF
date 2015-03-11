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
/**
 * DECAF_types.h
 * Some defines for commonly used types
 * @author: Lok Yan
 * @date: 19 SEP 2012
*/

#ifndef DECAF_TYPES_H
#define DECAF_TYPES_H
#ifdef __cplusplus
#define __STDC_LIMIT_MACROS 1
#endif /* __cplusplus */
#include <stdint.h>
#include "qemu-common.h"

typedef target_ulong gva_t;
//Interestingly enough - target_phys_addr_t is defined as uint64 - what to do?
typedef target_ulong gpa_t;

//to determine the HOST type - we use the definitions from TCG
// We use the same logic as defined in tcg.h
//typedef tcg_target_ulong hva_t;
//typedef tcg_target_ulong hpa_t;
//#if UINTPTR_MAX == UINT32_MAX
#if __WORDSIZE == 64
  typedef uint64_t hva_t;
  typedef uint64_t hpa_t;
#else
  typedef uint32_t hva_t;
  typedef uint32_t hpa_t;
#endif

typedef uintptr_t DECAF_Handle;
#define DECAF_NULL_HANDLE ((uintptr_t)NULL)

//Used for addresses since -1 is a rarely used-if ever 32-bit address
#define INV_ADDR (-1) //0xFFFFFFFF is only for 32-bit

/**
 * ERRORCODES
 */

typedef int DECAF_errno_t;
/**
 * Returned when a pointer is NULL when it should not have been
 */
#define NULL_POINTER_ERROR (-101)

/**
 * Returned when a pointer already points to something, although the function is expected to malloc a new area of memory.
 * This is used to signify that there is a potential for a memory leak.
 */
#define NON_NULL_POINTER_ERROR (-102)

/**
 * Returned when malloc fails. Out of memory.
 */
#define OOM_ERROR (-103)

/**
 * Returned when there is an error reading memory - for the guest.
 */
#define MEM_READ_ERROR (-104)

#define FILE_OPEN_ERROR (-105)
#define FILE_READ_ERROR (-105)
#define FILE_WRITE_ERROR (-105)

/**
 * Returned by functions that needed to search for an item before it can continue, but couldn't find it.
 */
#define ITEM_NOT_FOUND_ERROR (-106)

/**
 * Returned when one of the parameters into the function doesn't check out.
 */
#define PARAMETER_ERROR (-107)


#ifndef LIST_FOREACH_SAFE
#define LIST_FOREACH_SAFE(var, head, field, tvar)                       \
          for ((var) = LIST_FIRST((head));                                \
              (var) && ((tvar) = LIST_NEXT((var), field), 1);             \
              (var) = (tvar))
#endif

#endif

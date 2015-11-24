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
/*
 * linux_procinfo.cpp
 *
 *  Created on: September, 2013
 *      Author: Kevin Wang, Lok Yan
 */

#include <inttypes.h>
#include <string>
#include <list>
#include <vector>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/lexical_cast.hpp>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <queue>
#include <sys/time.h>
#include <math.h>
#include <glib.h>
#include <mcheck.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "cpu.h"
#include "config.h"
#include "hw/hw.h" // AWH  
#include "DECAF_main.h"
#include "DECAF_target.h"
#ifdef __cplusplus
};
#endif /* __cplusplus */

#include "linux_procinfo.h"
#include "hookapi.h"
#include "function_map.h"
#include "shared/vmi.h"
#include "DECAF_main.h"
#include "shared/utils/SimpleCallback.h"



#ifdef TARGET_I386
  #define T_FMT ""
  #define PI_R_EAX "eax"
  #define PI_R_ESP "esp"
/*
  #define isPrintableASCII(_x) ( ((_x & 0x80808080) == 0)   \
                                && ((_x & 0xE0E0E0E0) != 0) )
*/
#elif defined(TARGET_ARM)
  #define T_FMT ""
  #define PI_R_EAX "r0"
  #define PI_R_ESP "sp"

/*
  #define isPrintableASCII(_x) ( ((_x & 0x80808080) == 0)   \
                                && ((_x & 0xE0E0E0E0) != 0) )
*/
#else
  #define T_FMT "ll"
  #define PI_R_EAX "rax"
  #define PI_R_ESP "rsp"
/*
  #define isPrintableASCII(_x) ( ((_x & 0x8080808080808080) == 0)   \
                                && ((_x & 0xE0E0E0E0E0E0E0E0) != 0) )
*/
#endif

static inline int isPrintableASCII(target_ulong x)
{
  int i = 0;
  char c = 0;
  int ret = 0;
  
  do 
  {
    c = (x >> i) & 0xFF; //get the next character
    if ( ((c & 0x80) == 0) && ((c & 0xE0) != 0) )
    {
      ret = 1;
    }
    else if (c != 0)
    {
      return (0); //we found a non printable non NULL character so end it
    }
    i+=8; //shift it over 1 byte
  } while ( (c != 0) && (i < 64) ); //while its not the NULL character

  return (ret);
}

typedef target_ptr gva_t;
typedef target_ulong gpa_t;
typedef target_int target_pid_t;

//Here are some definitions straight from page_types.h

#define INV_ADDR ((target_ulong) -1)
#define INV_OFFSET ((target_ulong) -1)
#define INV_UINT ((target_uint) -1)

#if defined(TARGET_I386) || defined(TARGET_ARM)//  || defined(TARGET_MIPS)
  //this is the default value - but keep in mind that a custom built
  // kernel can change this
  #define TARGET_PAGE_OFFSET 0xC0000000

  //defined this extra constant here so that the code
  // for isKernelAddress can be standardized
  #define TARGET_KERNEL_IMAGE_START TARGET_PAGE_OFFSET
  #define TARGET_MIN_STACK_START 0xA0000000 //trial and error?
  #define TARGET_KERNEL_IMAGE_SIZE (0)
#elif defined(TARGET_MIPS)
  #define TARGET_PAGE_OFFSET  0x80000000UL
  #define TARGET_KERNEL_IMAGE_START TARGET_PAGE_OFFSET
  #define TARGET_MIN_STACK_START 0xA0000000UL //trial and error?
  #define TARGET_KERNEL_IMAGE_SIZE (0)
#else
  //See: http://lxr.linux.no/#linux+v3.11/Documentation/x86/x86_64/mm.txt
  // for the memory regions
  //These ranges seem to work for 2.6.32 as well
  //these definitions are in page_64_types.h

  //2.6.28 uses 81 to c0ffff -- 46bits of memory
  //2.6.29 uses 88 - c0ffff -- 57TB it says
  //2.6.31 uses 88 - c7ffff -- 64tb of memory - man they sure like to change these things!!!!
  #define TARGET_PAGE_OFFSET  0xFFFF880000000000
 
  //this is consistent from 2.6.28
  #define TARGET_KERNEL_IMAGE_START 0xFFFFFFFF80000000
  #define TARGET_MIN_STACK_START 0x0000000100000000 //trial and error?
  #define TARGET_KERNEL_IMAGE_SIZE (512 * 1024 * 1024)
#endif //target_i386


//straight from the kernel in processor.h
#define TARGET_TASK_SIZE TARGET_PAGE_OFFSET
#define TARGET_KERNEL_START TARGET_TASK_SIZE

#if defined(TARGET_I386) 
  //got this value from testing - not necessarily true though
  //might be some devices mapped into physical memory
  // that will screw things up a bit
  #define TARGET_KERNEL_END (0xF8000000 - 1)
#elif defined(TARGET_ARM)
  // NOTICE in ARM when RAM size is less than 896, then high_memory is equal to actual RAM size
  // check this link: http://www.arm.linux.org.uk/developer/memory.txt
  #define TARGET_KERNEL_END (TARGET_PAGE_OFFSET + ((ram_size < 896 * 1024 * 1024) ? (ram_size - 1) : (0xF8000000 - 1)))
#else
  //same here - in fact the global stuff (from the kernel image) are defined in higher addresses
  #define TARGET_KERNEL_END 0xFFFFC80000000000
#endif //target_i386

//some definitions to help limit how much to search
// these will likely have to be adjusted for 64 bit, 20, 4k and 100 works for 32
#define MAX_THREAD_INFO_SEARCH_SIZE 20
#define MAX_TASK_STRUCT_SEARCH_SIZE 4000 
#define MAX_MM_STRUCT_SEARCH_SIZE 500
#define MAX_VM_AREA_STRUCT_SEARCH_SIZE 500
#define MAX_CRED_STRUCT_SEARCH_SIZE 200
#define MAX_DENTRY_STRUCT_SEARCH_SIZE 200

//the list head contains two pointers thus
#define SIZEOF_LIST_HEAD (sizeof(target_ptr) + sizeof(target_ptr))
#define SIZEOF_COMM ((target_ulong)16)

#define TARGET_PGD_MASK TARGET_PAGE_MASK
#define TARGET_PGD_TO_CR3(_pgd) (_pgd - TARGET_KERNEL_START) //this is a guess
// but at any rate, PGD virtual address, which must be converted into 
// a physical address - see load_cr3 function in arch/x86/include/asm/processor.h



//here is a simple function that I wrote for
// use in this kernel module, but you get the idea
// the problem is that not all addresses above
// 0xC0000000 are valid, some are not 
// depending on whether the virtual address range is used
// we can figure this out by searching through the page tables
static inline
int isKernelAddress(gva_t addr)
{
  return (
    //the normal kernel memory area
    ( (addr >= TARGET_KERNEL_START) && (addr < TARGET_KERNEL_END) )
    //OR the kernel image area - in case the kernel image was mapped to some
    // other virtual address region - as from x86_64
    || ( (addr >= TARGET_KERNEL_IMAGE_START) && (addr < (TARGET_KERNEL_IMAGE_START + TARGET_KERNEL_IMAGE_SIZE)) )
#ifdef TARGET_X86_64
    //needed to incorporate the vmalloc/ioremap space according to the 
    // mm.txt in documentation/x86/x86_64 of the source
    //this change has been incorporated since 2.6.31
    //2.6.30 used c2 - e1ffff as the range.
    || ( (addr >= 0xFFFFC90000000000) && (addr < 0xFFFFe90000000000) )
#endif
  );
}

static inline int isKernelAddressOrNULL(gva_t addr)
{
  return ( (addr == (gva_t)0) || (isKernelAddress(addr)) ); 
}

inline int isStructKernelAddress(gva_t addr, target_ulong structSize)
{
  return ( isKernelAddress(addr) && isKernelAddress(addr + structSize) );
}

target_ulong getESP(CPUState *env)
{
  return DECAF_getESP(env);
}

gpa_t getPGD(CPUState *env)
{
  return (DECAF_getPGD(env) & TARGET_PGD_MASK);
}

//We will have to replace this function with another one - such as
// read_mem in DECAF
static inline target_ulong get_target_ulong_at(CPUState *env, gva_t addr)
{
  target_ulong val;
  if (DECAF_read_mem(env, addr, sizeof(target_ulong), &val) < 0)
    return (INV_ADDR);
  return val;
}

static inline target_uint get_uint32_at(CPUState *env, gva_t addr)
{
  target_uint val;
  if (DECAF_read_mem(env, addr, sizeof(uint32_t), &val) < 0)
    return (INV_UINT);
  return val;
}

//Dangerous memcpy
static inline int get_mem_at(CPUState *env, gva_t addr, void* buf, size_t count)
{
  return DECAF_read_mem(env, addr, count, buf) < 0 ? 0 : count;
}


//The idea is to go through the data structures and find an
// item that points back to the threadinfo
//ASSUMES PTR byte aligned
// gva_t findTaskStructFromThreadInfo(CPUState * env, gva_t threadinfo, ProcInfo* pPI, int bDoubleCheck) __attribute__((optimize("O0")));
gva_t  findTaskStructFromThreadInfo(CPUState * env, gva_t threadinfo, ProcInfo* pPI, int bDoubleCheck)
{
  int bFound = 0;
  target_ulong i = 0;
  target_ulong j = 0;
  gva_t temp = 0;
  gva_t temp2 = 0;
  gva_t candidate = 0;
  gva_t ret = INV_ADDR;
 
  if (pPI == NULL)
  {
    return (INV_ADDR);
  }
 
  //iterate through the thread info structure
  for (i = 0; i < MAX_THREAD_INFO_SEARCH_SIZE; i+= sizeof(target_ptr))
  {
    temp = (threadinfo + i);
    candidate = 0;
    // candidate = (get_target_ulong_at(env, temp));
    DECAF_read_ptr(env, temp, &candidate);
    //if it looks like a kernel address
    if (isKernelAddress(candidate))
    {
      //iterate through the potential task struct 
      for (j = 0; j < MAX_TASK_STRUCT_SEARCH_SIZE; j+= sizeof(target_ptr))
      {
        temp2 = (candidate + j);
        //if there is an entry that has the same 
        // value as threadinfo then we are set
        target_ulong val = 0;
        DECAF_read_ptr(env, temp2, &val);
        if (val == threadinfo)
        {
          if (bFound)
          {
            //printk(KERN_INFO "in findTaskStructFromThreadInfo: Double Check failed\n");
            return (INV_ADDR);
          }

          pPI->ti_task = i;
          pPI->ts_stack = j;
          ret = candidate;

          if (!bDoubleCheck)
          {
            return (ret);
          }
          else
          {
            //printk(KERN_INFO "TASK STRUCT @ [0x%"T_FMT"x] FOUND @ offset %"T_FMT"d\n", candidate, j);
            bFound = 1;
          }
        }
      }
    }
  }
  return (ret);
}

gpa_t findPGDFromMMStruct(CPUState * env, gva_t mm, ProcInfo* pPI, int bDoubleCheck)
{
  target_ulong i = 0;
  gpa_t temp = 0;
  gpa_t pgd = getPGD(env);

  if (pPI == NULL)
  {
    return (INV_ADDR);
  }

  if ( !isStructKernelAddress(mm, MAX_MM_STRUCT_SEARCH_SIZE) )
  {
    return (INV_ADDR);
  } 

  for (i = 0; i < MAX_MM_STRUCT_SEARCH_SIZE; i += sizeof(target_ulong))
  {
    temp = get_target_ulong_at(env, mm + i);
	if (temp == INV_ADDR)
	  return (INV_ADDR);
    if (pgd == TARGET_PGD_TO_CR3((temp & TARGET_PGD_MASK))) 
    {
      if (pPI->mm_pgd == INV_OFFSET)
        pPI->mm_pgd = i;
      return (temp);
    }
  }
 
  return (INV_ADDR);
}

gva_t findMMStructFromTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI, int bDoubleCheck)
{  
  if (pPI == NULL)
  {
    return (INV_ADDR);
  }
  if ( !isStructKernelAddress(ts, MAX_TASK_STRUCT_SEARCH_SIZE) )
  {
    return (INV_ADDR);
  }
  
  for (target_ulong i = 0, temp = INV_ADDR, last_mm; i < MAX_TASK_STRUCT_SEARCH_SIZE; i += sizeof(target_ulong))
  {
    last_mm = temp;	// record last mm value we read, so that we don't need read it twice
    temp = get_target_ulong_at(env, ts + i);
    if (isKernelAddress(temp))
    {
      if (findPGDFromMMStruct(env, temp, pPI, bDoubleCheck) != INV_ADDR)
      {
        // ATTENTION!! One interesting thing here is, when the system just starts, highly likely
        // we will grab the active_mm instead of mm, as the mm is null for kernel thread
        if (pPI->ts_mm == INV_OFFSET) {
          // do a test, if this is mm, then active_mm should be equal to mm
          if (get_target_ulong_at(env, ts + i + sizeof(target_ulong)) == temp) {
	        pPI->ts_mm = i;
            //monitor_printf(default_mon, "mm and active_mm is the same! \n");
          }
          else if (last_mm == 0) {	// current mm may be active_mm
	        pPI->ts_mm = i - sizeof(target_ulong);
            //monitor_printf(default_mon, "mm is null, active_mm detected! \n");
          }
          else
	    break;	// something is wrong
        }
        return (temp);
      }  
    } 
  }

  return (INV_ADDR);
}

//the characteristic of task struct list is that next is followed by previous
//both of which are pointers
// furthermore, next->previous should be equal to self
// same with previous->next
//lh is the list_head (or supposedly list head)
int isListHead(CPUState * env, gva_t lh)
{
  gva_t pPrev = lh + sizeof(target_ulong);
  gva_t next = 0;
  gva_t prev = 0;

  if ( !isKernelAddress(lh) || !isKernelAddress(pPrev) )
  {
    return (0);
  }

  //if both lh and lh+target_ulong (previous pointer) are pointers
  // then we can dereference them
  next = get_target_ulong_at(env, lh);
  prev = get_target_ulong_at(env, pPrev);

  if ( !isKernelAddress(next) || !isKernelAddress(prev) )
  {
    return (0);
  }
 
  // if the actual dereferences are also pointers (because they should be)
  // then we can check if the next pointer's previous pointer are the same 
  if ( (get_target_ulong_at(env, prev) == lh)
       && (get_target_ulong_at(env, next + sizeof(target_ulong)) == lh)
     )
  {
    return (1);
  }

  return (0);
}
 
//TODO: DoubleCheck
//In this case, because there are so many different list_head
// definitions, we are going to use the first
// list head when searching backwards from the mm struct
//The signature that we use to find the task struct is the following (checked against
// version 3.9.5 and 2.6.32)
// depending on whether SMP is configured or not (in 3.9.5) we should see the following
// list_head (tasks) //8 bytes
// int PRIO (if SMP in 3.9.5) //4 bytes
// list_head (plist_node->plist_head->list_head in 2.6.32, and if SMP in 3.9.5) // 8 bytes
// list_head (same // 8 bytes
// spinlock* (optional in 2.6.32 if CONFIG_DEBUG_PI_LIST is set)
//So the idea is that we will see if we have a listhead followed by an int followed by 
// 2 list heads followed by mm struct (basically we search backwards from mmstruct
// if this pattern is found, then we should have the task struct offset
gva_t findTaskStructListFromTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI, int bDoubleCheck)
{
  target_ulong i = 0;
  gva_t temp = 0;

  if (pPI == NULL)
  {
    return (INV_ADDR);
  }

  //this works for -1 as well (the default value) since we are using target_ulong
  if (pPI->ts_mm >= MAX_TASK_STRUCT_SEARCH_SIZE)
  {
    return (INV_ADDR);
  }
 
  //must check the whole range (includes overflow)
  if ( !isStructKernelAddress(ts, MAX_TASK_STRUCT_SEARCH_SIZE) )
  {
    return (INV_ADDR);
  }

  //We use the first such doubly linked list that comes before the
  // mmstruct pointer, 28 is the size of the template
  // 3 list heads plus an int
  //The reason for this type of search is because in 2.6 we have
  // list_head tasks, followed by plist_head (which is an int followed
  // by two list heads - might even have an additional pointer even - TODO - handle that case
  //in kernel version 3, the plist_node is wrapped by a CONFIG_SMP #def
  //TODO: Double check that target_ulong is the right way to go
  // the idea is that plist_node actually uses an int - however in 64 bit
  // systems, the fact that list_head defines a pointer - it would imply that
  // the int prio should take up 8 bytes anyways (so that things are aligned properly)
  for (i = (SIZEOF_LIST_HEAD * 3 + sizeof(target_ulong)); i < pPI->ts_mm; i+=sizeof(target_ulong))
  {
    temp = ts + pPI->ts_mm - i;
    //if its a list head - then we can be sure that this should work
    if (isListHead(env, temp))
    {
      //printk(KERN_INFO "[i = %"T_FMT"d] %d, %d, %d, --- \n", i, isListHead(env, temp)
       //  , isListHead(env, temp + SIZEOF_LIST_HEAD + sizeof(target_ulong))
      //   , isListHead(env, temp + SIZEOF_LIST_HEAD + SIZEOF_LIST_HEAD + sizeof(target_ulong))
      //   );
      
      if ( isListHead(env, temp + SIZEOF_LIST_HEAD + sizeof(target_ulong))
         && isListHead(env, temp + SIZEOF_LIST_HEAD +SIZEOF_LIST_HEAD + sizeof(target_ulong))
         )
      {
        //printk(KERN_INFO "FOUND task_struct_list offset [%d]\n", (uint32_t)temp - ts);
        pPI->ts_tasks = temp - ts;
        return (get_target_ulong_at(env, temp)); 
      }
    }
  }

  //if we are here - then that means we did not find the pattern - which could be because
  // we don't have smp configured on a 3 kernel
  //TODO: enable and test this part - needs a second level check later just in case
  // this was incorrect
  for (i = sizeof(target_ulong); i < pPI->ts_mm; i += sizeof(target_ulong))
  {
    temp = ts + pPI->ts_mm - i;
    if (isListHead(env, temp))
    {
      pPI->ts_tasks = temp - ts;
      return (get_target_ulong_at(env, temp));
    }
  }

  return (INV_ADDR);
}

//basically uses the threadinfo test to see if the current is a task struct
//We also use the task_list as an additional precaution since
// the offset of the threadinfo (i.e., stack) is 4 and the offset of 
// the task_struct in threadinfo is 0 which just happens to correspond
// to previous and next if this ts was the address of a list_head
// instead
//TODO: Find another invariance instead of the tasks list?
int isTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI)
{
  gva_t temp = 0;
  gva_t temp2 = 0;

  if (pPI == NULL)
  {
    return (0);
  }

  if (!isStructKernelAddress(ts, MAX_TASK_STRUCT_SEARCH_SIZE))
  {
    return (0);
  }

  if ( (pPI->ts_stack == INV_OFFSET) || (pPI->ti_task == INV_OFFSET) )
  {
    return (0);
  }

  temp = ts + pPI->ts_stack;

  //dereference temp to get to the TI and then add the offset to get back
  // the pointer to the task struct
  temp2 = get_target_ulong_at(env, temp) + pPI->ti_task;
  if ( !isKernelAddress(temp2) )
  {
    return (0);
  }
 
  //now see if the tasks is correct
  if ( !isListHead(env, ts + pPI->ts_tasks) )
  {
    return (0);
  }

  return (1);
}

//the signature for real_parent is that this is the
// first item where two task_struct pointers are together
// real_parent is the first one (this should most likely
// be the same as parent, although not necessarily true)
//NOTE: We can also use the follow on items which are
// two list_heads for "children" and "sibling" as well 
// as the final one which is a task_struct for "group_leader"
gva_t findRealParentGroupLeaderFromTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI)
{
  target_ulong i = 0;
  
  if (pPI == NULL)
  {
    return (INV_ADDR);
  }

  for (i = 0; i < MAX_TASK_STRUCT_SEARCH_SIZE; i += sizeof(target_ulong))
  {
    if ( isTaskStruct(env, get_target_ulong_at(env, ts+i), pPI) //real_parent
       && isTaskStruct(env, get_target_ulong_at(env, ts+i+sizeof(target_ulong)), pPI) //parent
       && isListHead(env, ts+i+sizeof(target_ulong)+sizeof(target_ulong)) //children
       && isListHead(env, ts+i+sizeof(target_ulong)+sizeof(target_ulong)+SIZEOF_LIST_HEAD) //sibling
       && isTaskStruct(env, get_target_ulong_at(env, ts+i+sizeof(target_ulong)+sizeof(target_ulong)+SIZEOF_LIST_HEAD+SIZEOF_LIST_HEAD), pPI) //group_leader
       )
    {
      if (pPI->ts_real_parent == INV_OFFSET)
      {
        pPI->ts_real_parent = i;
      }
      if (pPI->ts_group_leader == INV_OFFSET)
      {
        pPI->ts_group_leader = i+sizeof(target_ulong)+sizeof(target_ulong)+SIZEOF_LIST_HEAD+SIZEOF_LIST_HEAD;
      }
      return (ts+i);
    }
  }
  return (INV_ADDR);
}

//The characteristics of the init_task that we use are
//The mm struct pointer is NULL - since it shouldn't be scheduled?
//The parent and real_parent is itself
int isInitTask(CPUState * env, gva_t ts, ProcInfo* pPI, int bDoubleCheck)
{
  int bMMCheck = 0;
  int bRPCheck = 0;

  if ( (pPI == NULL) || !isStructKernelAddress(ts, MAX_TASK_STRUCT_SEARCH_SIZE) )
  {
    return (0);
  }

  //if we have the mm offset already then just check it
  if (pPI->ts_mm != INV_OFFSET)
  {
    if ( get_target_ulong_at(env, ts + pPI->ts_mm) == 0)
    {
      bMMCheck = 1;
    }
  }
  if (pPI->ts_real_parent != INV_OFFSET)
  {
    if (get_target_ulong_at(env, ts + pPI->ts_real_parent) == ts)
    {
      bRPCheck = 1;
    }
  }
  if ( (bDoubleCheck && bMMCheck && bRPCheck)
     || (!bDoubleCheck && (bMMCheck || bRPCheck))
     )
  {
    if (pPI->init_task_addr == INV_OFFSET)
    {
      pPI->init_task_addr = ts;  
    }
    return (1);
  }

  return (0);
}

//To find the "comm" field, we look for the name of
// init_task which is "swapper" -- we don't check for "swapper/0" or anything else
gva_t findCommFromTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI)
{
  target_ulong i = 0;
  //char* temp = NULL; //not used yet, because we are using the int comparison instead
  target_ulong temp2 = 0;
  //char* strInit = "swapper";
  uint32_t intSWAP = 0x70617773; //p, a, w, s
  uint32_t intPER = 0x2f726570; ///, r, e, p
  if (pPI == NULL)
  {
    return (INV_ADDR);
  }

  if (!isInitTask(env, ts, pPI, 0))
  {
    return (INV_ADDR);
  }

  //once again we are assuming that things are aligned
  for (i = 0; i < MAX_TASK_STRUCT_SEARCH_SIZE; i+=sizeof(target_ulong)) 
  {
    temp2 = ts + i;
    if (get_uint32_at(env, temp2) == intSWAP)
    {
      temp2 += 4; //move to the next item
      if ((get_uint32_at(env, temp2) & 0x00FFFFFF) == (intPER & 0x00FFFFFF))
      {
        if (pPI->ts_comm == INV_OFFSET)
        {
          pPI->ts_comm = i;
        }
        return (ts+i);
      }
    }
  }

  return (INV_ADDR);
}

//The signature for thread_group is
// that thread_group comes after an array of pid_links
// and PID_links contains an hlist_node (which 2 pointers)
// followed by a pid pointer - this means 3 points
//So we are looking for 3 pointers followed by
// a list_head for the thread group
//Turns out the simple signature above doesn't work
// because there are too many points
// so we will use a signature for the
// hlist instead
//The uniqueness of an hlist (as opposed to the list)
// is that the second parameter is a pointer to a pointer (**prev)
//At the lowest level, this doesn't work either since
// it is hard to distinguish between a pointer to a pointer
// from a pointer - which means a list will look like an hlist
//Sigh - so instead we are going to use the cputime_t
// fields as part of the signature instead - this can be useful
// especially if we do this kind of test early on during
// the boot process
//So the new signature is list_head followed by 3 pointers
// followed by cputimes
//This one didn't work either, so going to just use 
// a signature based on initTask
// which happens to have the whole array of pid_link
// pid_link consists of hlist, a basically another couple of pointers
// and a pointer to a pid (which seems to be the same value)
gva_t findThreadGroupFromTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI)
{
  target_ulong i = 0;

  if ( (pPI == NULL) )
  {
    return (INV_ADDR);
  }

  if (pPI->ts_group_leader == INV_OFFSET) 
  {
    i = 0;
  } //we can start from the group_leader as a shortcut
  else 
  {
    i = pPI->ts_group_leader;
  }

  if (!isInitTask(env, ts, pPI, 0))
  {
    return (INV_ADDR);
  }

  for ( ; i < MAX_TASK_STRUCT_SEARCH_SIZE; i+=sizeof(target_ulong))
  {
    /*
    printk(KERN_INFO "%d === %"T_FMT"x, %"T_FMT"x, %"T_FMT"x, %"T_FMT"x, %"T_FMT"x, %d,%d,%d,%d,%d\n", i, get_target_ulong_at(env, ts + i),
       get_target_ulong_at(env, ts + i + sizeof(target_ulong)),
       get_target_ulong_at(env, ts+i+sizeof(target_ulong)+sizeof(target_ulong)),
       get_target_ulong_at(env, ts+i+(sizeof(target_ulong)*3)),
       get_target_ulong_at(env, ts+i+(sizeof(target_ulong)*4)),
    (get_target_ulong_at(env, ts + i) == 0),
       (get_target_ulong_at(env, ts + i + sizeof(target_ulong)) == 0),
       isKernelAddress(get_target_ulong_at(env, ts+i+sizeof(target_ulong)+sizeof(target_ulong))),
       isListHead(env, get_target_ulong_at(env, ts+i+(sizeof(target_ulong)*3))), //is a list head
       (get_target_ulong_at(env, ts+i+(sizeof(target_ulong)*4)) == 0) //this is the entry for vfork_done ?
    );
    */
    //for init task the pid_link list should be all NULLS

/* This doesn't exactly work because of how different implementations
 populate the pid_link array. So we will use the new signature below
    if ( (get_target_ulong_at(env, ts + i) == 0)
       && (get_target_ulong_at(env, ts + i + sizeof(target_ulong)) == 0)
       && isKernelAddress(get_target_ulong_at(env, ts+i+sizeof(target_ulong)+sizeof(target_ulong)))
       && isListHead(env, get_target_ulong_at(env, ts+i+(sizeof(target_ulong)*3))) //is a list head
       && (get_target_ulong_at(env, ts+i+(sizeof(target_ulong)*3)+SIZEOF_LIST_HEAD) == 0) //this is the entry for vfork_done ?
       )
*/
//Through some additional testing, I see that completion, set_child_tid and clear_child_tid should all be null - utime should be 0 too since it doens't use any time for "userspace" -- its a kernel process. Finally, stime - the system time, should be nonzero for startup, but a very small number since it doesn't get scheduled after the start. 
// Given this fact, we are going to look for the list_head (which is thread_group) followed by 3 nulls. THe only trick here is that - in some architectures - the list head might be NULL pointers and in others it might be a pointer to itself. .
    if ( ( isListHead(env, get_target_ulong_at(env, ts+i)) 
           || ( (get_target_ulong_at(env, ts+i) == 0) && (get_target_ulong_at(env, ts+i+sizeof(target_ulong)) == 0) ) )
       && (get_target_ulong_at(env, ts+i+SIZEOF_LIST_HEAD) == 0)
       && (get_target_ulong_at(env, ts+i+SIZEOF_LIST_HEAD+sizeof(target_ulong)) == 0)
       && (get_target_ulong_at(env, ts+i+SIZEOF_LIST_HEAD+(sizeof(target_ulong)*2)) == 0)
       && (get_target_ulong_at(env, ts+i+SIZEOF_LIST_HEAD+(sizeof(target_ulong)*3)) == 0) //utime = 0
       && (get_target_ulong_at(env, ts+i+SIZEOF_LIST_HEAD+(sizeof(target_ulong)*4)) != 0) //stime != 0
       )
    {
      if ( pPI->ts_thread_group == INV_OFFSET )
      {
        pPI->ts_thread_group = i;
      }
      return (ts + i);
    }
  }
  return (INV_ADDR);
}

//we find cred by searching backwards starting from comm
//The signature is that we have an array of list heads (which is for
// the cpu_timers
// followed by real_cred and cred (both of which are pointers)
// followed by stuff (in 2.6.32) and then comm
gva_t findCredFromTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI)
{
  target_ulong i = 0;
  if ((pPI == NULL) || (pPI->ts_comm == INV_OFFSET) )
  {
    return (INV_ADDR);
  }
  if (!isStructKernelAddress(ts, MAX_TASK_STRUCT_SEARCH_SIZE))
  {
    return (INV_ADDR);
  }

  //we start at 16 because of the list_head followed by
  // the two pointers
  for (i = (sizeof(target_ulong)*4); i < pPI->ts_comm; i+=sizeof(target_ulong))
  {
    if ( isListHead(env, get_target_ulong_at(env, ts + pPI->ts_comm - i))
        && isKernelAddress(get_target_ulong_at(env, ts + pPI->ts_comm - i + SIZEOF_LIST_HEAD))
        && isKernelAddress(get_target_ulong_at(env, ts + pPI->ts_comm - i + SIZEOF_LIST_HEAD + sizeof(target_ulong)))
       )
    {
      if (pPI->ts_real_cred == INV_OFFSET)
      {
        pPI->ts_real_cred = pPI->ts_comm - i + SIZEOF_LIST_HEAD;
      }
      if (pPI->ts_cred == INV_OFFSET)
      {
        pPI->ts_cred = pPI->ts_comm - i + SIZEOF_LIST_HEAD + sizeof(target_ulong);
      }
      return (ts + pPI->ts_comm - i + SIZEOF_LIST_HEAD + sizeof(target_ulong));
    }
  }
  return (INV_ADDR);
}

#if defined(TARGET_I386) || defined (TARGET_ARM)
  #define STACK_CANARY_MASK 0xFFFF0000
#else
  #define STACK_CANARY_MASK 0xFFFF0000FFFF0000 
#endif
//pid and tgid are pretty much right on top of
// the real_parent, except for the case when a stack
// canary might be around. We will try to see
// if the canary is there - because canaries are supposed
// to be random - which is different from tgid and pid
// both of which are small numbers - so we try it this
// way
gva_t findPIDFromTaskStruct(CPUState * env, gva_t ts, ProcInfo* pPI)
{
  target_ulong offset = 0;
  target_ulong temp = 0;
  if ( (pPI == NULL) || (pPI->ts_real_parent == INV_OFFSET) )
  {
    return (INV_ADDR);
  }
  if (!isStructKernelAddress(ts, MAX_TASK_STRUCT_SEARCH_SIZE))
  {
    return (INV_ADDR);
  }

  if ( pPI->ts_group_leader == INV_ADDR )
  {
    return (INV_ADDR);
  }

  ts = get_target_ulong_at(env, ts + pPI->ts_group_leader);
  if (ts == INV_ADDR)
    return (INV_ADDR);

  //the signature is fairly simple - both pid and tgid are going to be the same
  // as long as the task in question is a group_leader 
  //Now there is a potential for 
  // the stack canary to interfere - in which case we will
  // check for the existence of the stack canary by looking at the values
  //Since the canary is supposed to be random it should not be like pids
  // which are expected to be low values
  //Also notice that since pid_t is defined as 
  // an int - it should be 4 bytes. Thus, either the previous eight bytes
  // look like a canary or not - see the masks that mask out the low bits
  temp = get_target_ulong_at(env, ts+pPI->ts_real_parent-sizeof(target_ulong));
  if (temp & STACK_CANARY_MASK)
  {
    offset = sizeof(target_ulong);
  }

  if (pPI->ts_pid == INV_OFFSET)
  {
    pPI->ts_pid = pPI->ts_real_parent - sizeof(target_pid_t)*2 - offset;
  }
  if (pPI->ts_tgid == INV_OFFSET)
  {
    pPI->ts_tgid = pPI->ts_real_parent - sizeof(target_pid_t) - offset;
  }

  //as it turns out there is also a potential alignment problem on 64 bit
  // x86 - since the pointer for real_parent should be 8 bytes aligned
  // and so should the stack_canary! 
  //To see if there is a problem - we check to see if the "expected"
  // pid and tgid's match
  if (get_uint32_at(env, ts + pPI->ts_pid) != get_uint32_at(env, ts + pPI->ts_tgid))
  {
    pPI->ts_pid -= 4;
    pPI->ts_tgid -= 4;  
  }

  if (get_uint32_at(env, ts + pPI->ts_pid) != get_uint32_at(env, ts + pPI->ts_tgid))
  {
    //printk(KERN_INFO "UH OH THEY ARE NOT THE SAME [%d], [%d]\n", get_uint32_at(env, pPI->ts_pid), get_uint32_at(env, pPI->ts_tgid));
  }
  return (ts + pPI->ts_pid);
}

//we should be able to populate all of the mm struct field at once
// since we are mostly interested in the vma, and the start stack, brk and etc
// areas
//So basically what we are going to rely on is the fact that
// we have 11 unsigned longs:
// startcode, endcode, startdata, enddata (4)
// startbrk, brk, startstack (3)
// argstart, argend, envstart, envend (4)
//Meaning we have a lot of fields with relative 
// addresses in the same order as defined - except for brk
int isStartCodeInMM(CPUState * env, target_ulong* temp, target_ulong expectedStackStart)
{
  if (temp == NULL)
  {
    return (0);
  }

  if ( 
       (temp[0] > temp[1]) //startcode > endcode
       || (temp[1] > temp[2]) //endcode > startdata
       || (temp[2] > temp[3]) //startdata > enddata
       || (temp[3] > temp[4]) //enddata > startbrk 
       || (temp[4] > temp[6]) //startbrk > startstack
       || (temp[6] > temp[7]) //startstack > argstart
       || (temp[7] > temp[8]) //argstart > argend
       || (temp[8] > temp[9]) //argend > envstart
       || (temp[9] > temp[10]) //envstart > envend
       || (temp[8] != temp[9]) //argend != envstart (the same?)
       || (temp[6] < expectedStackStart) 
     )
  {
    return (0);
  }
/*
  for (i = 0; i < 11; i++)
  {
    printk(KERN_INFO "CUR [%d] %16"T_FMT"x\n", i, ((target_ulong*)(&current->mm->start_code))[i]);
  }
  for (i = 0; i < 11; i++)
  {
    printk(KERN_INFO "[%d] %16"T_FMT"x\n", i, temp[i]);
  }
*/
  return (1);
}

#define MM_TEMP_BUF_SIZE 100
int populate_mm_struct_offsets(CPUState * env, gva_t mm, ProcInfo* pPI)
{
  static bool isOffsetPopulated = false;
  if (isOffsetPopulated)
    return (0);

  target_ulong temp[MM_TEMP_BUF_SIZE + 11];
  target_ulong* pTemp = temp;
  target_ulong count = 0;
  target_ulong numRead = 0;

  if (pPI == NULL)
  {
    return (-1);
  }

  if (!isStructKernelAddress(mm, MAX_MM_STRUCT_SEARCH_SIZE))
  {
    return (-1); 
  }

  //mmap always comes first it looks like
  if (pPI->mm_mmap == INV_OFFSET)
  {
    pPI->mm_mmap = 0;
  }

  memset(temp, 0, sizeof(temp));
  //grab a 11 ulong block

  for (count = 0; count < (MAX_MM_STRUCT_SEARCH_SIZE / sizeof(target_ulong)); count++)
  {
    if ( (count % MM_TEMP_BUF_SIZE) == 0)
    {
      //if at the end of the buffer reset the pTemp pointer
      // to the beginning
      pTemp = temp;
      //if we are at the beginning then grab 11 ulongs at a time
      if (get_mem_at(env, mm+(sizeof(target_ulong)*count), pTemp, 11*sizeof(target_ulong)) != (11*sizeof(target_ulong)))
      {
        return (-1);
      }
      numRead += 11;
    }
    else //if not then just read the next value
    {
      //increment pTemp
      pTemp++;
      //10 is the 11th element
      pTemp[10] = get_target_ulong_at(env, mm+(sizeof(target_ulong) * (numRead)));
      numRead++;
    }
    
    if (isStartCodeInMM(env, pTemp, TARGET_MIN_STACK_START))
    {
      break;
    }
  }

  if (count >= (MAX_MM_STRUCT_SEARCH_SIZE / sizeof(target_ulong)))
  {
    return (-1);
  }

  //if we are here that means we found it
  if (pPI->mm_start_brk == INV_OFFSET)
  {
    pPI->mm_start_brk = sizeof(target_ulong)*(count+4);
  }
  if (pPI->mm_brk == INV_OFFSET)
  {
    pPI->mm_brk = sizeof(target_ulong)*(count+5);
  }
  if (pPI->mm_start_stack == INV_OFFSET)
  {
    pPI->mm_start_stack = sizeof(target_ulong)*(count+6);
  }
  if (pPI->mm_arg_start == INV_OFFSET)
  {
    pPI->mm_arg_start = sizeof(target_ulong)*(count+7);
  }

  isOffsetPopulated = true;
  return (0);
}

//determines whether the address belongs to an RB Node
// RB as in Red Black Tree - it should work for any tree really
// maybe?
int isRBNode(CPUState * env, gva_t vma)
{
  target_ulong parent = 0;
  target_ulong left = 0;
  target_ulong right = 0;
  target_ulong parent_mask = ~0x3;

  if (!isKernelAddress(vma))
  {
    return (0);
  }

  parent = get_target_ulong_at(env, vma);
  right = get_target_ulong_at(env, vma+ sizeof(target_ulong));
  left = get_target_ulong_at(env, vma+ (sizeof(target_ulong)*2));
  
  if (!isKernelAddress(parent))
  {
    return (0);
  }

  //see if either left or right is NULL and if not NULL
  // then see if they point back to parent
  if (left != 0) 
  {
    if ( !isKernelAddress(left) )
    {
      return (0);
    }
    //now check to see if right and left point back to parent
    // here we are going to simply ignore the least significant 2 bits
    // While it is not perfect (rb_node is long aligned) it should be good enough
    //the minimum size of rb_node is 12 bytes anwways.
    if ( (get_target_ulong_at(env, left) & (parent_mask)) != (parent & (parent_mask)) )
    {
      return (0);
    }
  }

  if (right != 0) 
  {
    if ( !isKernelAddress(right) )
    {
      return (0);
    }
    if ( (get_target_ulong_at(env, right) & (parent_mask)) != (parent & (parent_mask)) )
    {
      return (0);
    }
  }

  return (1);
  //finally we can check to see if the current node is one of the paren'ts left
  //TODO:
}

//This signature is different for 2.6 and for 3 
// The basic idea is that in 2.6 we have the mm_struct* vm_mm first
// followed by vm_start and vm_end (both ulongs)
// In 3 we have vm_start and vm_end first and vm_mm will come much later
//Now since vm_start is supposed to be the starting address of 
// the vm area - it must be a userspace virtual address. This is a perfect
// test to see which version of the kernel we are dealing with since
// the mm_struct* would be a kernel address
int populate_vm_area_struct_offsets(CPUState * env, gva_t vma, ProcInfo* pPI)
{
  static bool isOffsetPopulated = false;
  if (isOffsetPopulated)
    return (0);

  int is26 = 0;
  target_ulong i = 0;

  if (pPI == NULL)
  {
    return (-1);
  }

  if (!isStructKernelAddress(vma, MAX_VM_AREA_STRUCT_SEARCH_SIZE))
  {
    return (-1);
  }

  if (isKernelAddress(get_target_ulong_at(env, vma)))
  {
    //if its a kernel pointer then we are dealing with 2.6
    is26 = 1;
    if (pPI->vma_vm_start == INV_OFFSET)
    {
      pPI->vma_vm_start = sizeof(target_ulong);
    }
    if (pPI->vma_vm_end == INV_OFFSET)
    {
      pPI->vma_vm_end = sizeof(target_ulong)*2;
    }
    if (pPI->vma_vm_next == INV_OFFSET)
    {
      pPI->vma_vm_next = sizeof(target_ulong)*3;
    } 
  }
  else
  {
    if (pPI->vma_vm_start == INV_OFFSET)
    {
      pPI->vma_vm_start = 0;
    }
    if (pPI->vma_vm_end == INV_OFFSET)
    {
      pPI->vma_vm_end = sizeof(target_ulong);
    }
    if (pPI->vma_vm_next == INV_OFFSET)
    {
      pPI->vma_vm_next = sizeof(target_ulong)*2;
    } 
  }

  //now that we have populated vm_start, vm_end and vm_next, we need to find the rest
  //to find vm_flags - we use two different signatures
  // in 2.6 it is right before the first rb_node
  for (i = 0; i < MAX_VM_AREA_STRUCT_SEARCH_SIZE; i+=sizeof(target_ulong))
  {
    if (isRBNode(env, vma + i))
    {
      if (pPI->vma_vm_flags == INV_OFFSET)
      {
        if (is26)
        {
          pPI->vma_vm_flags = i - sizeof(target_ulong); //- sizeof(vm_flags)
        }
        else
        {
          //for version 3 we look for the rb_node and add 3 target_ulongs
          // 1 first the rb_subtree_gap, 1 for vm_mm pointer and one for vm_page_prot
          //we also have to add in the size of the rb_node which is another 3 target_ulongs
          pPI->vma_vm_flags = i + (sizeof(target_ulong)*6);
        }
      }
      break;
    }
  }
  
  //to get the vm_file we look for list_head (anon_vma_chain)
  // followed by the anon_vma* followed by the vm_operations_struct pointer
  // followed by a non-poiner since its a page offset (which should be 
  // usespace address -- this is the important one
  // followed by a pointer the vmfile (and another pointer) 
  //we just continue from where we left off before in the search since
  // file comes after flags
  for ( ; i < MAX_VM_AREA_STRUCT_SEARCH_SIZE; i += sizeof(target_ulong))
  {
    target_ulong vm_pgoff;
    // NOTICE the vm_pgoff shouldn't be zero, as in ARM, the pointer in list_head can be NULL,
    // there is possibility we are getting the first node of anon_vma_node as vm_file
    if ( isListHead(env, vma+i) 
//once again it looks like in the ARM test case, listhead is NULL instead of pointing to self
#ifdef TARGET_ARM
         || ( (get_target_ulong_at(env, vma+i) == 0) && (get_target_ulong_at(env, vma+i+sizeof(target_ulong)) == 0) )
#endif
    ) {
      //first we see if the short circuiting works - if it does then we are set
      if (
           isKernelAddress(get_target_ulong_at(env, vma + i + SIZEOF_LIST_HEAD + sizeof(target_ulong)))
           && !isKernelAddress(vm_pgoff = get_target_ulong_at(env, vma + i + SIZEOF_LIST_HEAD + sizeof(target_ulong) + sizeof(target_ulong)))
           && vm_pgoff != 0
         )
      {
        if (pPI->vma_vm_file == INV_OFFSET)
        {
          pPI->vma_vm_file = i + SIZEOF_LIST_HEAD + (sizeof(target_ulong) * 3);
          pPI->vma_vm_pgoff = pPI->vma_vm_file - (sizeof(target_ulong));
        }
        break;
      }
    }
  } 

  isOffsetPopulated = true;
  return (0);
}

//dentry is simple enough its just a pointer away
// first is the union of list_head and rcu_head
// list head is 2 pointers and rcu_head is 2 pointers
// one for rcu_head and another for the function pointer
//then struct path is itself two pointers thus
// its a constant - 3 pointers away
int getDentryFromFile(CPUState * env, gva_t file, ProcInfo* pPI)
{
  static bool isOffsetPopulated = false;
  if (isOffsetPopulated)
    return (0);

  if (pPI == NULL)
  {
    return (-1);
  }
  if (pPI->file_dentry == INV_OFFSET)
  {
    pPI->file_dentry = sizeof(target_ulong) * 3;
  }
  isOffsetPopulated = true;
  return (0);
}


//the cred struct hasn't changed from 2.6 so its the same
// the struct is also quite simple in that there is an atomic_t (its an int) in front
// followed by the possibility of a bunch of other declarations including a pointer
// and a magic number
// This is all followed by the entries of interest to us
// thus they either start at an offset 4 (for the atomic_t) or something else
//What we do here is going to just keep searching until we see a bunch of values
// that look like uids. The only problem is that uids can possibly be 16 bits
// in length instead of 32 bits in length. So that can be a little bit tricky.
//We can actually tell the difference by seeing if the higher bits are zero
// in most cases we will see that the uid is going to be low numbers
//Furthermore, we can also look for cred in a known to be root process
// such as init.
#define IS_UID(_x) (_x < 0x0000FFFF)
int populate_cred_struct_offsets(CPUState * env, gva_t cred, ProcInfo* pPI)
{
  target_ulong i = sizeof(target_int);

  if (pPI == NULL)
  {
    return (-1);
  }

  if (!isStructKernelAddress(cred, MAX_CRED_STRUCT_SEARCH_SIZE))
  {
    return (-1);
  }

  for (i = sizeof(target_int); i < MAX_CRED_STRUCT_SEARCH_SIZE; i += sizeof(target_int))
  {
    if (
         IS_UID(get_uint32_at(env, cred + i)) //uid
         && IS_UID(get_uint32_at(env, cred + i + sizeof(target_int))) //gid
         && IS_UID(get_uint32_at(env, cred + i + (sizeof(target_int) * 2))) //suid
         && IS_UID(get_uint32_at(env, cred + i + (sizeof(target_int) * 3))) //sgid
         && IS_UID(get_uint32_at(env, cred + i + (sizeof(target_int) * 4))) //euid
         && IS_UID(get_uint32_at(env, cred + i + (sizeof(target_int) * 5))) //egid
       )
    {
      if (pPI->cred_uid == INV_OFFSET)
      {
        pPI->cred_uid = i;
      }
      if (pPI->cred_gid == INV_OFFSET)
      {
        pPI->cred_gid = i + sizeof(target_int);
      }
      if (pPI->cred_euid == INV_OFFSET)
      {
        pPI->cred_euid = i + (sizeof(target_int) * 4);
      }
      if (pPI->cred_egid == INV_OFFSET)
      {
        pPI->cred_egid = i + (sizeof(target_int) * 5);
      }
      break;
    }
  }

  return (0);
}

//d_parent is the third pointer - since it is after hlist_bl_node -
// which contains two pointers
//Then d_name is right after d_parent
// finally d_iname is after d_name and another pointer which is d_inode
// d_iname is a qstr which is a data structure with a HASH plus a 
// pointer to the name
// basically we can use a bunch of pointers to help find the offsets
// of interest
//The only problem is that the pointers can be NULL - 
// so the question is how to handle these situations?
//For now I am just going to use some hardcoded offsets
// since the only unknown in this the seqcount - which is an unsigned
// so basically it is 4 bytes
//TODO: Try to figure this out - the only problem
// is that not all vmarea structs have files and dentry objects
// which means to do this properly - we will likely need to
// search for the first executable page (where the binary is loaded)
// and then look at the name for that dentry - that way
// we can be sure that at least d_iname is defined - and can look
// for the cstring there.
int populate_dentry_struct_offsets(CPUState *env, gva_t dentry, ProcInfo* pPI)
{
  static bool isOffsetPopulated = false;
  if (isOffsetPopulated)
    return (0);

  target_ulong i = 0;
  target_ulong parent = 0;
  target_ulong chr = 0;

  if (pPI == NULL)
  {
    return (-1);
  }

  if (!isStructKernelAddress(dentry, MAX_DENTRY_STRUCT_SEARCH_SIZE))
  {
    return (-1);
  }

  // Kevin W: Here we try to find the d_iname.  The trick is to find the string which
  // contains "ld-linux.so", which is the dynamic library loader for linux
  for (i = 0; i < MAX_DENTRY_STRUCT_SEARCH_SIZE; i+=sizeof(target_ulong))
  {
    char lib_name[11];
    if (DECAF_read_mem(env, dentry+i, sizeof(lib_name), lib_name) < 0)
      return (-1); // once the memory read fails, we won't want to populate offsets any more

    if ( !strncmp(lib_name, "ld-linux.so", 11) )
    {
      if (pPI->dentry_d_iname == INV_OFFSET)
      {
        pPI->dentry_d_iname = i;
      }
    }
  }

  if (pPI->dentry_d_iname == INV_OFFSET)
  {
    return (-1);
  }

  //now find d_parent 
  for (i = 0; i < MAX_DENTRY_STRUCT_SEARCH_SIZE; i+=sizeof(target_ulong))
  {
    //d_parent is preceded by an hlist - so we look for that first
    if ( 
         !isKernelAddressOrNULL(get_target_ulong_at(env, dentry+i)) 
         || !isKernelAddressOrNULL(get_target_ulong_at(env, dentry+i+sizeof(target_ulong)))
       )
    {
      continue;
    }

    parent = get_target_ulong_at(env, dentry+i+sizeof(target_ulong) + sizeof(target_ulong));
    if (isStructKernelAddress(parent, MAX_DENTRY_STRUCT_SEARCH_SIZE))
    {
      if (DECAF_read_mem(env, parent + pPI->dentry_d_iname, sizeof(target_ulong), &chr) < 0)
        return (-1); // once the memory read fails, we won't want to populate offsets any more

      if (isPrintableASCII(chr))
      {
        //monitor_printf(default_mon, "CHAR2 %c \n", chr);
        if (pPI->dentry_d_parent == INV_OFFSET)
        {
          pPI->dentry_d_parent = i + (sizeof(target_ulong)*2);
        }
        if (pPI->dentry_d_name == INV_OFFSET)
        {
          pPI->dentry_d_name = i + (sizeof(target_ulong)*3);
        }
        isOffsetPopulated = true;
        return (0);
      }  
    }
  }
  return (-1);
}

//runs through the guest's memory and populates the offsets within the
// ProcInfo data structure. Returns the number of elements/offsets found
// or -1 if error
int populate_kernel_offsets(CPUState *env, gva_t threadinfo, ProcInfo* pPI)
{
  static bool isOffsetPopulated = false;
  //first we will try to get the threadinfo structure and etc
  gva_t taskstruct = 0;
  gva_t mmstruct = 0;
  gva_t vmastruct = 0;
  gva_t vmfile = 0;
  gva_t dentrystruct = 0;
  gva_t realcred = 0;
 
  gva_t ret = 0;
  gva_t tempTask = 0;

  gva_t gl = 0;

  if (pPI == NULL)
  {
    return (-1);
  }
  else if (isOffsetPopulated) {
    return (0);
  }

  if ((taskstruct = findTaskStructFromThreadInfo(env, threadinfo, pPI, 0)) == INV_ADDR)
    return (-1);

  //monitor_printf(default_mon,  "ThreadInfo @ [0x%"T_FMT"x]\n", threadinfo);
  //monitor_printf(default_mon,  "task_struct @ [0x%"T_FMT"x] TSOFFSET = %"T_FMT"d, TIOFFSET = %"T_FMT"d\n", taskstruct, pPI->ti_task, pPI->ts_stack);
  if ((mmstruct = findMMStructFromTaskStruct(env, taskstruct, pPI, 0)) == INV_ADDR)
    return (-1);
  //monitor_printf(default_mon,  "mm_struct @ [0x%"T_FMT"x] mmOFFSET = %"T_FMT"d, pgdOFFSET = %"T_FMT"d\n", mmstruct, pPI->ts_mm, pPI->mm_pgd);
  if (findTaskStructListFromTaskStruct(env, taskstruct, pPI, 0) == INV_ADDR)
    return (-1);
  //monitor_printf(default_mon,  "task_struct offset = %"T_FMT"d\n", pPI->ts_tasks);

  if (findRealParentGroupLeaderFromTaskStruct(env, taskstruct, pPI) == INV_ADDR)
    return (-1);
  //monitor_printf(default_mon,  "real_parent = %"T_FMT"x, group_leader = %"T_FMT"x\n", pPI->ts_real_parent, pPI->ts_group_leader);

  //we need the group leader - since current might just be a thread - we need a real task
  gl = get_target_ulong_at(env, taskstruct + pPI->ts_group_leader);
  ret = findCommFromTaskStruct(env, gl, pPI);

  //don't forget to to get back to the head of the task struct
  // by subtracting ts_tasks offset
  tempTask = get_target_ulong_at(env, gl + pPI->ts_tasks) - pPI->ts_tasks;
  while ((ret == INV_ADDR) && (tempTask != gl) && (isKernelAddress(tempTask)))
  {
    ret = findCommFromTaskStruct(env, tempTask, pPI);
    //move to the next task_struct
    tempTask = get_target_ulong_at(env, tempTask + pPI->ts_tasks) - pPI->ts_tasks;
  }

  if (ret != INV_ADDR)
  {
    //printk(KERN_INFO "Comm offset is = %"T_FMT"d, %s \n", pPI->ts_comm, (char*)(taskstruct + pPI->ts_comm));
  }
  else {
    return (-1);
  }

  if (findCredFromTaskStruct(env, taskstruct, pPI) == INV_ADDR)
    return (-1);
  //printk(KERN_INFO "real_cred = %"T_FMT"d, cred = %"T_FMT"d \n", pPI->ts_real_cred, pPI->ts_cred);

  if (findPIDFromTaskStruct(env, taskstruct, pPI) == INV_ADDR)
    return (-1);
  //printk(KERN_INFO "pid = %"T_FMT"d, tgid = %"T_FMT"d \n", pPI->ts_pid, pPI->ts_tgid);

  //For this next test, I am just going to use the task struct lists
  if (findThreadGroupFromTaskStruct(env, pPI->init_task_addr, pPI) == INV_ADDR)
    return (-1);
  //printk(KERN_INFO "Thread_group offset is %"T_FMT"d\n", pPI->ts_thread_group);

  realcred = get_target_ulong_at(env, taskstruct + pPI->ts_real_cred);
  if (populate_cred_struct_offsets(env, realcred, pPI) != 0)
    return (-1);

  isOffsetPopulated = true;
  return (0);
}


int printProcInfo(ProcInfo* pPI)
{
  if (pPI == NULL)
  {
    return (-1);
  }

  monitor_printf(default_mon,
      "    {  \"%s\", /* entry name */\n"
      "       0x%08"T_FMT"X, /* init_task address */\n"
      "       %"T_FMT"d, /* size of task_struct */\n"
      "       %"T_FMT"d, /* offset of task_struct list */\n"
      "       %"T_FMT"d, /* offset of pid */\n"
      "       %"T_FMT"d, /* offset of tgid */\n"
      "       %"T_FMT"d, /* offset of group_leader */\n"
      "       %"T_FMT"d, /* offset of thread_group */\n"
      "       %"T_FMT"d, /* offset of real_parent */\n"
      "       %"T_FMT"d, /* offset of mm */\n"
      "       %"T_FMT"d, /* offset of stack */\n"
      "       %"T_FMT"d, /* offset of real_cred */\n"
      "       %"T_FMT"d, /* offset of cred */\n"
      "       %"T_FMT"d, /* offset of comm */\n"
      "       %"T_FMT"d, /* size of comm */\n",
	  "       %"T_FMT"d, /* inode index number*/\n",
      pPI->strName,
      pPI->init_task_addr,
      pPI->init_task_size,
      pPI->ts_tasks,
      pPI->ts_pid,
      pPI->ts_tgid,
      pPI->ts_group_leader,
      pPI->ts_thread_group,
      pPI->ts_real_parent,
      pPI->ts_mm,
      pPI->ts_stack,
      pPI->ts_real_cred,
      pPI->ts_cred,
      pPI->ts_comm,
      SIZEOF_COMM,
      pPI->inode_ino
  );
  
  monitor_printf(default_mon,
      "       %"T_FMT"d, /* offset of uid cred */\n"
      "       %"T_FMT"d, /* offset of gid cred */\n"
      "       %"T_FMT"d, /* offset of euid cred */\n"
      "       %"T_FMT"d, /* offset of egid cred */\n",
      pPI->cred_uid,
      pPI->cred_gid,
      pPI->cred_euid,
      pPI->cred_egid
  );

  monitor_printf(default_mon,
      "       %"T_FMT"d, /* offset of mmap in mm */\n"
      "       %"T_FMT"d, /* offset of pgd in mm */\n"
      "       %"T_FMT"d, /* offset of arg_start in mm */\n"
      "       %"T_FMT"d, /* offset of start_brk in mm */\n"
      "       %"T_FMT"d, /* offset of brk in mm */\n"
      "       %"T_FMT"d, /* offset of start_stack in mm */\n",
      pPI->mm_mmap,
      pPI->mm_pgd,
      pPI->mm_arg_start,
      pPI->mm_start_brk,
      pPI->mm_brk,
      pPI->mm_start_stack
  );

  monitor_printf(default_mon,
      "       %"T_FMT"d, /* offset of vm_start in vma */\n"
      "       %"T_FMT"d, /* offset of vm_end in vma */\n"
      "       %"T_FMT"d, /* offset of vm_next in vma */\n"
      "       %"T_FMT"d, /* offset of vm_file in vma */\n"
      "       %"T_FMT"d, /* offset of vm_flags in vma */\n",
      pPI->vma_vm_start,
      pPI->vma_vm_end,
      pPI->vma_vm_next,
      pPI->vma_vm_file,
      pPI->vma_vm_flags
  );

  monitor_printf(default_mon,
      "       %"T_FMT"d, /* offset of dentry in file */\n"
      "       %"T_FMT"d, /* offset of d_name in dentry */\n"
      "       %"T_FMT"d, /* offset of d_iname in dentry */\n"
      "       %"T_FMT"d, /* offset of d_parent in dentry */\n",
      pPI->file_dentry,
      pPI->dentry_d_name,
      pPI->dentry_d_iname,
      pPI->dentry_d_parent
  );
  
  monitor_printf(default_mon,
      "       %"T_FMT"d, /* offset of task in thread info */\n",
      pPI->ti_task
  );

  return (0);
}

void get_executable_directory(string &sPath)
{
  int rval;
  char szPath[1024];
  sPath = "";
  rval = readlink("/proc/self/exe", szPath, sizeof(szPath)-1);
  if(-1 == rval)
  {
    monitor_printf(default_mon, "can't get path of main executable.\n");
    return;
  }
  szPath[rval-1] = '\0';
  sPath = szPath;
  sPath = sPath.substr(0, sPath.find_last_of('/'));
  sPath += "/";
  return;
}

void get_procinfo_directory(string &sPath)
{
  get_executable_directory(sPath);
  sPath += "../shared/kernelinfo/procinfo_generic/";
  return;
}

// given the section number, load the offset values
#define FILL_TARGET_ULONG_FIELD(field) pi.field = pt.get(sSectionNum + #field, INVALID_VAL)
void _load_one_section(const boost::property_tree::ptree &pt, int iSectionNum, ProcInfo &pi)
{
    string sSectionNum;

    sSectionNum = boost::lexical_cast<string>(iSectionNum);
    sSectionNum += ".";

    // fill strName field
    string sName;
    const int SIZE_OF_STR_NAME = 32;
    sName = pt.get<string>(sSectionNum + "strName");
    strncpy(pi.strName, sName.c_str(), SIZE_OF_STR_NAME);
    pi.strName[SIZE_OF_STR_NAME-1] = '\0';

    const target_ulong INVALID_VAL = -1;

    // fill other fields
    FILL_TARGET_ULONG_FIELD(init_task_addr  );
    FILL_TARGET_ULONG_FIELD(init_task_size  );
    FILL_TARGET_ULONG_FIELD(ts_tasks        );
    FILL_TARGET_ULONG_FIELD(ts_pid          );
    FILL_TARGET_ULONG_FIELD(ts_tgid         );
    FILL_TARGET_ULONG_FIELD(ts_group_leader );
    FILL_TARGET_ULONG_FIELD(ts_thread_group );
    FILL_TARGET_ULONG_FIELD(ts_real_parent  );
    FILL_TARGET_ULONG_FIELD(ts_mm           );
    FILL_TARGET_ULONG_FIELD(ts_stack        );
    FILL_TARGET_ULONG_FIELD(ts_real_cred    );
    FILL_TARGET_ULONG_FIELD(ts_cred         );
    FILL_TARGET_ULONG_FIELD(ts_comm         );

	FILL_TARGET_ULONG_FIELD(modules         );
	FILL_TARGET_ULONG_FIELD(module_name     );
	FILL_TARGET_ULONG_FIELD(module_init     );
	FILL_TARGET_ULONG_FIELD(module_size     );
	FILL_TARGET_ULONG_FIELD(module_list     );
			    
    FILL_TARGET_ULONG_FIELD(cred_uid        );
    FILL_TARGET_ULONG_FIELD(cred_gid        );
    FILL_TARGET_ULONG_FIELD(cred_euid       );
    FILL_TARGET_ULONG_FIELD(cred_egid       );
    FILL_TARGET_ULONG_FIELD(mm_mmap         );
    FILL_TARGET_ULONG_FIELD(mm_pgd          );
    FILL_TARGET_ULONG_FIELD(mm_arg_start    );
    FILL_TARGET_ULONG_FIELD(mm_start_brk    );
    FILL_TARGET_ULONG_FIELD(mm_brk          );
    FILL_TARGET_ULONG_FIELD(mm_start_stack  );
    FILL_TARGET_ULONG_FIELD(vma_vm_start    );
    FILL_TARGET_ULONG_FIELD(vma_vm_end      );
    FILL_TARGET_ULONG_FIELD(vma_vm_next     );
    FILL_TARGET_ULONG_FIELD(vma_vm_file     );
    FILL_TARGET_ULONG_FIELD(vma_vm_flags    );
    FILL_TARGET_ULONG_FIELD(vma_vm_pgoff    );
    FILL_TARGET_ULONG_FIELD(file_dentry     );
	FILL_TARGET_ULONG_FIELD(file_inode		);
    FILL_TARGET_ULONG_FIELD(dentry_d_name   );
    FILL_TARGET_ULONG_FIELD(dentry_d_iname  );
    FILL_TARGET_ULONG_FIELD(dentry_d_parent );
    FILL_TARGET_ULONG_FIELD(ti_task         );
	FILL_TARGET_ULONG_FIELD(inode_ino);

	FILL_TARGET_ULONG_FIELD(proc_fork_connector);
	FILL_TARGET_ULONG_FIELD(proc_exit_connector);
	FILL_TARGET_ULONG_FIELD(proc_exec_connector);
	FILL_TARGET_ULONG_FIELD(vma_link);
	FILL_TARGET_ULONG_FIELD(remove_vma);
	FILL_TARGET_ULONG_FIELD(vma_adjust);

	FILL_TARGET_ULONG_FIELD(trim_init_extable);
#ifdef TARGET_MIPS
    FILL_TARGET_ULONG_FIELD(mips_pgd_current);
#endif
}

// find the corresponding section for the current os and return the section number
int find_match_section(const boost::property_tree::ptree &pt, target_ulong tulInitTaskAddr)
{
    int cntSection = pt.get("info.total", 0);

    string sSectionNum;
    vector<int> vMatchNum;

    monitor_printf(default_mon, "Total Sections: %d\n", cntSection);

    for(int i = 1; i<=cntSection; ++i)
    {
      sSectionNum = boost::lexical_cast<string>(i);
      target_ulong tulAddr = pt.get<target_ulong>(sSectionNum + ".init_task_addr");
      if(tulAddr == tulInitTaskAddr)
      {
        vMatchNum.push_back(i);
      }
    }

    if(vMatchNum.size() > 1)
    {
      monitor_printf(default_mon, "Too many match sections in procinfo.ini\n");
      return 0;
      // for(int i=0;i<vMatchNum.size();++i)
      // {
      //   monitor_printf(default_mon, "[%d] ", vMatchNum[i]);
      // }
      // monitor_printf(default_mon, "\nPlease input the corresponding section number for the operating system\n");
      // int iNum = 100;
      // // cin >> iNum;
      // DECAF_stop_vm();
      // scanf("%d", &iNum);

      // cout << "iNum" << iNum << endl;
      // DECAF_start_vm();
      // bool bMatch = false;
      // for(int i=0;i<vMatchNum.size();++i)
      // {
      //   if(iNum == vMatchNum[i])
      //   {
      //     bMatch = true;
      //   }
      // }

      // if(bMatch)
      // {
      //   return iNum;
      // }
      // else
      // {
      //   monitor_printf(default_mon, "Invalid section number\n");
      //   return 0;
      // }
    }
    
    if(vMatchNum.size() <= 0)
    {
      monitor_printf(default_mon, "No match in procinfo.ini\n");
      return 0;        
    }

    return vMatchNum[0];
}



// infer init_task_addr, use the init_task_addr to search for the corresponding
// section in procinfo.ini. If found, fill the fields in ProcInfo struct.
int load_proc_info(CPUState * env, gva_t threadinfo, ProcInfo &pi)
{
  static bool bProcinfoMisconfigured = false;
  const int CANNOT_FIND_INIT_TASK_STRUCT = -1;
  const int CANNOT_OPEN_PROCINFO = -2;
  const int CANNOT_MATCH_PROCINFO_SECTION = -3;
  target_ulong tulInitTaskAddr;

  if(bProcinfoMisconfigured)
  {
   return CANNOT_MATCH_PROCINFO_SECTION;
  }

  // find init_task_addr
  tulInitTaskAddr = findTaskStructFromThreadInfo(env, threadinfo, &pi, 0);
  // tulInitTaskAddr = 2154330880;
  if (INV_ADDR == tulInitTaskAddr)
  {
    return CANNOT_FIND_INIT_TASK_STRUCT;
  }

  string sProcInfoPath;
  boost::property_tree::ptree pt;
  get_procinfo_directory(sProcInfoPath);
  sProcInfoPath += "procinfo.ini";
  monitor_printf(default_mon, "\nProcinfo path: %s\n",sProcInfoPath.c_str());
  // read procinfo.ini
  if (0 != access(sProcInfoPath.c_str(), 0))
  {
      monitor_printf(default_mon, "can't open %s\n", sProcInfoPath.c_str());
      return CANNOT_OPEN_PROCINFO;
  }
  boost::property_tree::ini_parser::read_ini(sProcInfoPath, pt);

  // find the match section using previously found init_task_addr
  int iSectionNum = find_match_section(pt, tulInitTaskAddr);
  // no match or too many match sections
  if(0 == iSectionNum)
  {
    monitor_printf(default_mon, "VMI won't work.\nPlease configure procinfo.ini and restart DECAF.\n");
    // exit(0);
    bProcinfoMisconfigured = true;
    return CANNOT_MATCH_PROCINFO_SECTION;
  }

  _load_one_section(pt, iSectionNum, pi);
  monitor_printf(default_mon, "Match %s\n", pi.strName);
  return 0;
}

class LibraryLoader
{
public:
  LibraryLoader(const char *strName)
  {
    if(init_property_tree(strName))
    {
      load();
    }
  }
private:

  bool init_property_tree(const char* strName)
  {
    string sLibConfPath;
    get_procinfo_directory(sLibConfPath);
    const string LIB_CONF_DIR = "lib_conf/";
    sLibConfPath = sLibConfPath + LIB_CONF_DIR + strName + ".ini";

    if (0 != access(sLibConfPath.c_str(), 0))
    {
        monitor_printf(default_mon, "\nCan't open %s\nLibrary function offset will not be loaded.\nGo head if you don't need to hook library functions.\n", sLibConfPath.c_str());
        return false;
    }

    boost::property_tree::ini_parser::read_ini(sLibConfPath, m_pt);
    return true;
  }


  void load()
  {
    // load every section
    int cntSection = m_pt.get("info.total", 0);
    string sSectionNum;

    monitor_printf(default_mon, "Lib Configuration Total Sections: %d\n", cntSection);

    for(int i = 1; i<=cntSection; ++i)
    {
      sSectionNum = boost::lexical_cast<string>(i);
      m_cur_libpath = m_pt.get<string>(sSectionNum + "." + LIBPATH_PROPERTY_NAME);
      m_cur_section = &m_pt.get_child(sSectionNum);
      load_cur_section();
    }
  }

  void load_cur_section()
  {
    monitor_printf(default_mon, "loading lib conf for %s\n", m_cur_libpath.c_str());
    // traverse the section
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, *m_cur_section)
    {
      if(!v.first.compare(LIBPATH_PROPERTY_NAME))
      {
        continue;
      }
      // insert function
      target_ulong addr = m_cur_section->get<target_ulong>(v.first);
      funcmap_insert_function(m_cur_libpath.c_str(), v.first.c_str(), addr, 0);
    }
  }


  boost::property_tree::ptree m_pt;
  string m_cur_libpath;
  boost::property_tree::ptree *m_cur_section;
public:
  static const string LIBPATH_PROPERTY_NAME;
};

const string LibraryLoader::LIBPATH_PROPERTY_NAME = "decaf_conf_libpath";

void load_library_info(const char *strName)
{
  LibraryLoader loader(strName);
}
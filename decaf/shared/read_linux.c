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
#include "config-host.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <libgen.h>
#include <unistd.h>
#include "DECAF_main.h"
#include "hookapi.h"
#include "read_linux.h"

char kernelinfo_filename[256] = SRC_PATH "/shared/kernelinfo/kernelinfo.conf";

target_ulong kernel_mem_start = 0xc0000000;

target_ulong hookingpoint = 0;
target_ulong hookingpoint2 = 0;
target_ulong taskaddr = 0;
int tasksize = 0;
int listoffset = 0;
int pidoffset = 0;
int mmoffset = 0;
int pgdoffset = 0;
int commoffset = 0;
int commsize = 0;
int vmstartoffset = 0;
int vmendoffset = 0;
int vmnextoffset = 0;
int vmfileoffset = 0;
int dentryoffset = 0;
int dnameoffset = 0;
int dinameoffset = 0;
target_ulong force_sig_info_hook = 0;

target_ulong next_task_struct(target_ulong addr)
{
  target_ulong retval;

  if (0xC031E000 == taskaddr)
    // this is for kernel 2.4.20
    DECAF_read_mem(NULL, addr + listoffset, sizeof(retval), &retval);
  else {
    // default is kernel 2.6
    target_ulong next;

    DECAF_read_mem(NULL, addr + listoffset + sizeof(target_ulong),
			sizeof(target_ulong), &next);
    retval = next - listoffset;
  }

  return retval;
}


target_ulong get_pid(target_ulong addr)
{
  target_ulong pid;

  DECAF_read_mem(NULL, addr + pidoffset, sizeof(pid), &pid);
  return pid;

}

target_ulong get_pgd(target_ulong addr)
{
  target_ulong mmaddr, pgd;
  DECAF_read_mem(NULL, addr + mmoffset, sizeof(mmaddr), &mmaddr);
  if (0 == mmaddr)
    DECAF_read_mem(NULL, addr + mmoffset + sizeof(mmaddr),
		sizeof(mmaddr), &mmaddr);

  if (0 != mmaddr)
    DECAF_read_mem(NULL, mmaddr + pgdoffset, sizeof(pgd), &pgd);
  else
    memset(&pgd, 0, sizeof(pgd));

  return pgd;
}

void get_name(target_ulong addr, char *buf, int size)
{
  DECAF_read_mem(NULL, addr + commoffset,
		size < commsize ? size : commsize, buf);
}

target_ulong get_first_mmap(target_ulong addr)
{
  target_ulong mmaddr, mmap;
  DECAF_read_mem(NULL, addr + mmoffset, sizeof(mmaddr), &mmaddr);
  if (0 == mmaddr)
    DECAF_read_mem(NULL, addr + mmoffset + sizeof(mmaddr),
                   sizeof(mmaddr), &mmaddr);

  if (0 != mmaddr)
    DECAF_read_mem(NULL, mmaddr, sizeof(mmap), &mmap);
  else
    memset(&mmap, 0, sizeof(mmap));

  return mmap;
}

target_ulong get_next_mmap(target_ulong addr)
{
  target_ulong mmap;
  DECAF_read_mem(NULL, addr + vmnextoffset, sizeof(mmap), &mmap);

  return mmap;
}

target_ulong get_vmstart(target_ulong addr)
{
  target_ulong vmstart;
  DECAF_read_mem(NULL, addr + vmstartoffset, sizeof(vmstart), &vmstart);
  return vmstart;
}

target_ulong get_vmend(target_ulong addr)
{
  target_ulong vmend;
  DECAF_read_mem(NULL, addr + vmendoffset, sizeof(vmend), &vmend);
  return vmend;
}

void get_mod_name(target_ulong addr, char *name, int size)
{
  //LOK: Need to update this to use CPUState* env
  target_ulong vmfile, dentry;
  if (DECAF_memory_rw(NULL, addr + vmfileoffset, &vmfile, sizeof(vmfile), 0) == -1
      || DECAF_memory_rw(NULL, vmfile + dentryoffset, &dentry, sizeof(dentry),
                        0) == -1
      || DECAF_memory_rw(NULL, dentry + dinameoffset, name, size < 36 ? size : 36,
                        0) == -1)
    name[0] = 0;
}

#define BUFFER_SIZE 1024

int init_kernel_offsets(void)
{
  int i = 0;
  int retval = -1;
  char buf[128];
  char version[128];
  int started = 0;
  int isbcomment = 0;
  int islcomment = 0;
  char pattern [BUFFER_SIZE];
  FILE * fd = fopen(kernelinfo_filename, "ro");
  if(!fd) {
    // It is not in the default location. Now let's try a different one: "%temu_path%/ini".
    bzero(kernelinfo_filename, sizeof(kernelinfo_filename));
    if(readlink ("/proc/self/exe", kernelinfo_filename, 
		sizeof(kernelinfo_filename)-1) == -1) {
      monitor_printf(default_mon, "Cannot read /proc/self/exe!\n");
      return -1;
    }

    dirname(kernelinfo_filename);
    strcat(kernelinfo_filename, "/ini/kernelinfo.conf");
    fd = fopen(kernelinfo_filename, "ro");
    if(!fd) {
	   monitor_printf(default_mon, "Cannot open %s\n", kernelinfo_filename);
       return -1;
    }
  }

again:

  for(i = 0, started = 0, isbcomment = 0, islcomment = 0; i < BUFFER_SIZE; ) {
    
    pattern[i] = fgetc(fd);
    
    if(pattern[i] == EOF)
      break;      

    switch(pattern[i]) {
    case '/':
      pattern[i] = fgetc(fd);
      if(pattern[i] == '*')
        isbcomment += 1;
      else if(pattern[i] == '/')
        islcomment = 1;
      break;
    case '*':      
      pattern[i] = fgetc(fd);
      if(pattern[i] == '/')
        isbcomment -= 1;
      break;
    case '\n':
      islcomment = 0;
      break;
    }
    
    if(isbcomment || islcomment)
      continue;
    if(pattern[i] == '{') {
      assert(!started || isbcomment || islcomment);
      started = 1;
    } else if(pattern[i] == '}') {
      started = 0;
      pattern[i] = 0;
      int count = sscanf(pattern, "%[^,],%x,%x,%x,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%x",
          version,
          &hookingpoint,
          &hookingpoint2,
          &taskaddr,
          &tasksize,
          &listoffset,
          &pidoffset,
          &mmoffset,
          &pgdoffset,
          &commoffset,
          &commsize,
          &vmstartoffset,
          &vmendoffset,
          &vmnextoffset,
          &vmfileoffset,
          &dentryoffset,
          &dnameoffset,
          &dinameoffset,
          &force_sig_info_hook);
      if(count == 19 /*the newer version of kernelinfo structure*/|| count == 18) {
        DECAF_memory_rw(NULL, taskaddr + commoffset, buf, 128, 0);
        if (strcmp(buf, "swapper") == 0) {
          //now we detect the correct kernel
          kernel_mem_start = 0xC0000000;
          retval = i;
          break;
        }
      }
      goto again;
    }
    if(!started)
      continue;
    if(isdigit(pattern[i]) || isalpha(pattern[i]) || pattern[i] == ',' 
        || pattern[i] == '_' || pattern[i] == '.' || pattern[i] == '-')
      i++;
  }

  fclose(fd);

  return retval;
}



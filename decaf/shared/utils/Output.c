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
 * Output.c
 *
 *  Created on: Sep 29, 2011
 *      Author: lok
 */

#include "Output.h"

//file pointers should never be in the kernel memory range so this should work
static const void* DECAF_OUTPUT_MONITOR_FD = (void*)0xFEEDBEEF;

FILE* ofp = NULL;
Monitor* pMon = NULL;

void DECAF_printf(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  DECAF_vprintf(ofp, fmt, ap);
  va_end(ap);
}

void DECAF_fprintf(FILE* fp, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  if ( (pMon != NULL) && (((void*)fp == (void*)pMon) || (fp == DECAF_OUTPUT_MONITOR_FD)) )
  {
    monitor_vprintf(pMon, fmt, ap);
  }
  else
  {
    DECAF_vprintf(fp, fmt, ap);
  }
  va_end(ap);
}

void DECAF_mprintf(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  if (pMon != NULL)
  {
    monitor_vprintf(pMon, fmt, ap);
  }
  else
  {
    vprintf(fmt, ap);
  }
  va_end(ap);
}

void DECAF_vprintf(FILE* fp, const char *fmt, va_list ap)
{
  if (fp == NULL)
  {
    //that means either use stdout or monitor
    if (pMon != NULL)
    {
      monitor_vprintf(pMon, fmt, ap);
    }
    else
    {
      vprintf(fmt, ap);
    }
  }
  else
  {
    vfprintf(fp, fmt, ap);
  }
}

void DECAF_flush(void)
{
  DECAF_fflush(ofp);
}

void DECAF_fflush(FILE* fp)
{
  if (fp == NULL)
  {
    if (pMon != NULL)
    {
      //nothing to do here
    }
    else
    {
      fflush(stdout);
    }
  }
  else
  {
    fflush(fp);
  }
}

void DECAF_do_set_output_file(Monitor* mon, const char* fileName)
{
  if (ofp != NULL)
  {
    return;
  }

  if (strcmp(fileName, "stdout") == 0)
  {
    DECAF_output_cleanup();
    return;
  }
  pMon = mon; //make a local copy of the monitor
  //open the file
  ofp = fopen(fileName, "w");
  if (ofp == NULL)
  {
    DECAF_printf("Could not open the file [%s]\n", fileName);
  }
}

void DECAF_output_init(Monitor* mon)
{
  if (mon != NULL)
  {
    pMon = mon;
  }
  else
  {
    pMon = default_mon;
  }
}

void DECAF_output_cleanup(void)
{
  if (ofp != NULL)
  {
    fflush(ofp);
    fclose(ofp);
  }
  ofp = NULL;
  pMon = NULL;
}


FILE* DECAF_get_output_fp(void)
{
  return (ofp);
}

Monitor* DECAF_get_output_mon(void)
{
  return (pMon);
}

const FILE* DECAF_get_monitor_fp(void)
{
  return (DECAF_OUTPUT_MONITOR_FD);
}

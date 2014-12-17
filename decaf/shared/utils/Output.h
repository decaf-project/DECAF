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
 * Output.h
 *
 *  Created on: Sep 29, 2011
 *      Author: lok
 */

#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <stdio.h>
#include "monitor.h"

#ifdef __cplusplus
extern "C"
{
#endif

void DECAF_printf(const char* fmt, ...);
void DECAF_mprintf(const char* fmt, ...);
void DECAF_fprintf(FILE* fp, const char* fmt, ...);
void DECAF_vprintf(FILE* fp, const char* fmt, va_list ap);
void DECAF_flush(void);
void DECAF_fflush(FILE* fp);

FILE* DECAF_get_output_fp(void);
Monitor* DECAF_get_output_mon(void);
const FILE* DECAF_get_monitor_fp(void);

void DECAF_do_set_output_file(Monitor* mon, const char* fileName);
void DECAF_output_init(Monitor* mon);
void DECAF_output_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* OUTPUT_H_ */

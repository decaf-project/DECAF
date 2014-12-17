/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>
This is a plugin of DECAF. You can redistribute and modify it
under the terms of BSD license but it is made available
WITHOUT ANY WARRANTY. See the top-level COPYING file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
/*
 * custom_handlers.c
 *
 *
 *      Author: Xunchao Hu
 */

#include <stdio.h>
#include <stdlib.h>

#include "custom_handlers.h"
#include "win_constants.h"


/*
 * Custom API Handlers
 */
void ZwOpenKey_ret_handler(void *opaque)
{
	printf("handler ZwOpenKey\n");
}

/*
 *

NTSTATUS NtCreateFile(
  __out     PHANDLE FileHandle,
  __in      ACCESS_MASK DesiredAccess,
  __in      POBJECT_ATTRIBUTES ObjectAttributes,
  __out     PIO_STATUS_BLOCK IoStatusBlock,
  __in_opt  PLARGE_INTEGER AllocationSize,
  __in      ULONG FileAttributes,
  __in      ULONG ShareAccess,
  __in      ULONG CreateDisposition,
  __in      ULONG CreateOptions,
  __in      PVOID EaBuffer,
  __in      ULONG EaLength
);
 *
 */
void NtCreateFile_ret_handler(void *opaque)
{
	monitor_printf(default_mon, "NtCreateFile_ret\n");
	uint32_t esp, offset;
	uint32_t i,j;
	CPUState *cpu;
	cpu = cpu_single_env;

	UNICODE_STRING ObjectName;
	char *Buffer = NULL;
	ULONG Length=0;
	uint8_t unicode_str[1024];

	POBJECT_ATTRIBUTES po = NULL;


	/* Populate out arguments */
	esp = cpu->regs[R_ESP];
	//offset = esp;
	uint32_t stack_data[11];
	DECAF_read_mem(cpu,esp,4*11,(void *)stack_data);
	uint64_t data_temp=(uint64_t)stack_data[2];
	po = malloc(sizeof(OBJECT_ATTRIBUTES)); ;
	DECAF_read_mem(cpu,stack_data[2],sizeof(OBJECT_ATTRIBUTES),(void *)po);

		if(po->ObjectName){
			DECAF_read_mem(cpu,(uint32_t)po->ObjectName, sizeof(UNICODE_STRING), (void *)&ObjectName);

			monitor_printf(default_mon, "\t\tObjectName.Length: %d\n", ObjectName.Length);
			monitor_printf(default_mon, "\t\tObjectName.MaximumLength: %d\n", ObjectName.MaximumLength);
			monitor_printf(default_mon, "\t\tObjectName.Buffer: 0x%p\n", ObjectName.Buffer);
			if(ObjectName.Buffer && ObjectName.Length){
				Buffer = (char *) malloc (sizeof(char) * ObjectName.Length);
				if(Buffer != '\0') {
					cpu_memory_rw_debug(cpu_single_env, (target_ulong)ObjectName.Buffer, (uint8_t *)&unicode_str, ObjectName.Length, 0);
					for (i = 0, j = 0; i < ObjectName.Length; i+=2, j++)
						Buffer[j] = unicode_str[i];
					Buffer[j] = '\0';
					DECAF_printf("%s\n",Buffer);
					monitor_printf(default_mon, "\t\t\tBuffer: %s \n", Buffer);
					free(Buffer);
				}
			}
		}


}

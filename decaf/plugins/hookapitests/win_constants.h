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
 * win_constants.h
 *
 *  Created on: Jul 16, 2012
 *      Author: ek
 */

#ifndef WIN_CONSTANTS_H_
#define WIN_CONSTANTS_H_
/*
typedef unsigned short wchar_t;
typedef wchar_t WCHAR;
*/
typedef unsigned short WCHAR;
typedef void *PVOID;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef unsigned short USHORT;

typedef PVOID HANDLE;
typedef WCHAR *PWSTR;

/*
 * Windows Data Structures
 */
typedef struct _PROCESS_INFORMATION {
  HANDLE hProcess;
  HANDLE hThread;
  DWORD  dwProcessId;//DWORD
  DWORD  dwThreadId;//DWORD
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct _LSA_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
  ULONG           Length;
  HANDLE          RootDirectory;
  PUNICODE_STRING ObjectName;
  ULONG           Attributes;
  PVOID           SecurityDescriptor;
  PVOID           SecurityQualityOfService;
}  OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

#endif /* WIN_CONSTANTS_H_ */

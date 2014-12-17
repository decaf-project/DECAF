/* Copyright (C) 2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#ifndef _ANDROID_OPTION_H
#define _ANDROID_OPTION_H

/* a structure used to model a linked list of parameters
 */
typedef struct ParamList {
    char*              param;
    struct ParamList*  next;
} ParamList;

/* define a structure that will hold all option variables
 */
typedef struct {
#define OPT_LIST(n,t,d)    ParamList*  n;
#define OPT_PARAM(n,t,d)   char*  n;
#define OPT_FLAG(n,d)      int    n;
#include "android/cmdline-options.h"
} AndroidOptions;


/* parse command-line arguments options and remove them from (argc,argv)
 * 'opt' will be set to the content of parsed options
 * returns 0 on success, -1 on error (unknown option)
 */
extern int
android_parse_options( int  *pargc, char**  *pargv, AndroidOptions*  opt );

/* name of default keyset file */
#define  KEYSET_FILE    "default.keyset"

/* the default device DPI if none is specified by the skin
 */
#define  DEFAULT_DEVICE_DPI  165

#endif /* _ANDROID_OPTION_H */

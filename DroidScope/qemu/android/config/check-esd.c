/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* this file is used to test that we can use libesd with lazy dynamic linking */

#include <esd.h>
#include <dlfcn.h>
#include <stdio.h>

#define  D(...)  fprintf(stderr,__VA_ARGS__)
#define  STRINGIFY(x)  _STRINGIFY(x)
#define _STRINGIFY(x)  #x

#define  ESD_SYMBOLS   \
    ESD_FUNCTION(int,esd_play_stream,(esd_format_t,int,const char*,const char*))   \
    ESD_FUNCTION(int,esd_record_stream,(esd_format_t,int,const char*,const char*)) \
    ESD_FUNCTION(int,esd_open_sound,( const char *host )) \
    ESD_FUNCTION(int,esd_close,(int)) \

/* define pointers to library functions we're going to use */
#define  ESD_FUNCTION(ret,name,sig)   \
    static ret  (*func_ ## name)sig;

ESD_SYMBOLS

#undef  ESD_FUNCTION
static void*    esd_lib;

int main( void ) 
{
    int  fd;

    esd_lib = dlopen( "libesd.so", RTLD_NOW );
    if (esd_lib == NULL)
        esd_lib = dlopen( "libesd.so.0", RTLD_NOW );

    if (esd_lib == NULL) {
        D("could not find libesd on this system\n");
        return 1;
    }

#undef  ESD_FUNCTION
#define ESD_FUNCTION(ret,name,sig)                                               \
    do {                                                                         \
        (func_ ##name) = dlsym( esd_lib, STRINGIFY(name) );                      \
        if ((func_##name) == NULL) {                                             \
            D("could not find %s in libesd\n", STRINGIFY(name));   \
            return 1;                                               \
        }                                                                        \
    } while (0);

    ESD_SYMBOLS

    return 0;
}

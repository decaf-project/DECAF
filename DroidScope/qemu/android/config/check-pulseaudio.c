/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define  PULSEAUDIO_SYMBOLS \
    PULSEAUDIO_SYMBOLS(pa_simple*,pa_simple_new,(const char* server,const char* name, pa_stream_direction_t dir, const char* dev, const char* stream_name, const pa_sample_spec* ss, const pa_channel_map* map, const pa_buffer_attr *attr, int *error)) \
    PULSEAUDIO_SYMBOLS(void,pa+simple_free,(pa_simple* s))\
    PULSEAUDIO_SYMBOLS(int,pa_simple_write,(pa_simple* s, const void* data, size_t bytes, int* error))\
    PULSEAUDIO_SYMBOLS(int,pa_simple_read,(pa_simple* s,void* data, size_t bytes, int* error))\
    PULSEAUDIO_SYMBOLS(const char*,pa_strerror,(int error))

/* define pointers to library functions we're going to use */
#define  PULSEAUDIO_FUNCTION(ret,name,sig)   \
    static ret  (*func_ ## name)sig;

PULSEAUDIO_SYMBOLS

#undef  PULSEAUDIO_FUNCTION
static void*    pa_lib;

int main( void ) 
{
    int  fd;

    pa_lib = dlopen( "libpulse-simple.so", RTLD_NOW );
    if (pa_lib == NULL)
        pa_lib = dlopen( "libpulse-simple.so.0", RTLD_NOW );

    if (pa_lib == NULL) {
        D("could not find libpulse on this system");
        return 1;
    }

#undef  PULSEAUDIO_FUNCTION
#define PULSEAUDIO_FUNCTION(ret,name,sig)                                               \
    do {                                                                         \
        (func_ ##name) = dlsym( pa_lib, STRINGIFY(name) );                      \
        if ((func_##name) == NULL) {                                             \
            D("could not find %s in libpulse\n", STRINGIFY(name));   \
            return 1;                                               \
        }                                                                        \
    } while (0);

    PULSEAUDIO_SYMBOLS

    return 0;
}

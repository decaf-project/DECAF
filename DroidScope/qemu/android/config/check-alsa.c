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
#include <dlfcn.h>
#include <stdio.h>
#include <alsa/asoundlib.h>

#define  D(...)  fprintf(stderr,__VA_ARGS__)
#define  STRINGIFY(x)  _STRINGIFY(x)
#define _STRINGIFY(x)  #x

#define  DYN_SYMBOLS   \
    DYN_FUNCTION(size_t,snd_pcm_sw_params_sizeof,(void))    \
    DYN_FUNCTION(int,snd_pcm_hw_params_current,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)) \
    DYN_FUNCTION(int,snd_pcm_sw_params_set_start_threshold,(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val))  \
    DYN_FUNCTION(int,snd_pcm_sw_params,(snd_pcm_t *pcm, snd_pcm_sw_params_t *params))  \
    DYN_FUNCTION(int,snd_pcm_sw_params_current,(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)) \
    DYN_FUNCTION(size_t,snd_pcm_hw_params_sizeof,(void))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_any,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_access,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_format,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_rate_near,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_channels_near,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_buffer_time_near,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir))  \
    DYN_FUNCTION(int,snd_pcm_hw_params,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_get_buffer_size,(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val))  \
    DYN_FUNCTION(int,snd_pcm_prepare,(snd_pcm_t *pcm))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_get_period_size,(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_get_period_size_min,(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_period_size,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int dir))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_get_buffer_size_min,(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)) \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_buffer_size,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val))  \
    DYN_FUNCTION(int,snd_pcm_hw_params_set_period_time_near,(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir))  \
    DYN_FUNCTION(snd_pcm_sframes_t,snd_pcm_avail_update,(snd_pcm_t *pcm)) \
    DYN_FUNCTION(int,snd_pcm_drop,(snd_pcm_t *pcm))  \
    DYN_FUNCTION(snd_pcm_sframes_t,snd_pcm_writei,(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size))  \
    DYN_FUNCTION(snd_pcm_sframes_t,snd_pcm_readi,(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size))  \
    DYN_FUNCTION(snd_pcm_state_t,snd_pcm_state,(snd_pcm_t *pcm))  \
    DYN_FUNCTION(const char*,snd_strerror,(int errnum)) \
    DYN_FUNCTION(int,snd_pcm_open,(snd_pcm_t **pcm, const char *name,snd_pcm_stream_t stream, int mode)) \
    DYN_FUNCTION(int,snd_pcm_close,(snd_pcm_t *pcm))  \



/* define pointers to library functions we're going to use */
#define  DYN_FUNCTION(ret,name,sig)   \
    static ret  (*func_ ## name)sig;

DYN_SYMBOLS

#undef  DYN_FUNCTION

#define func_snd_pcm_hw_params_alloca(ptr) \
    do { assert(ptr); *ptr = (snd_pcm_hw_params_t *) alloca(func_snd_pcm_hw_params_sizeof()); memset(*ptr, 0, func_snd_pcm_hw_params_sizeof()); } while (0)

#define func_snd_pcm_sw_params_alloca(ptr) \
    do { assert(ptr); *ptr = (snd_pcm_sw_params_t *) alloca(func_snd_pcm_sw_params_sizeof()); memset(*ptr, 0, func_snd_pcm_sw_params_sizeof()); } while (0)

static void*  alsa_lib;

int  main(void)
{
    int  result = 1;

    alsa_lib = dlopen( "libasound.so", RTLD_NOW );
    if (alsa_lib == NULL)
        alsa_lib = dlopen( "libasound.so.2", RTLD_NOW );

    if (alsa_lib == NULL) {
        D("could not find libasound on this system\n");
        return 1;
    }

#undef  DYN_FUNCTION
#define DYN_FUNCTION(ret,name,sig)                                               \
    do {                                                                         \
        (func_ ##name) = dlsym( alsa_lib, STRINGIFY(name) );                     \
        if ((func_##name) == NULL) {                                             \
            D("could not find %s in libasound\n", STRINGIFY(name)); \
            goto Fail;                                                           \
        }                                                                        \
    } while (0);

    DYN_SYMBOLS

    result = 0;
    goto Exit;

Fail:
    D("failed to open library\n");

Exit:
    dlclose(alsa_lib);
    return result;
}

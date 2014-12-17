/* Copyright (C) 2010 The Android Open Source Project
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
#include "audio/audio.h"
#include "android/utils/debug.h"

/* This source file contains a small test audio virtual device that
 * can be used to check that the emulator properly plays sound on
 * the host system without having to boot a full system.
 */

#define SAMPLE_SIZE  16384

typedef struct {
    QEMUSoundCard  card;
    SWVoiceOut    *voice;
    int            pos;
    short          sample[SAMPLE_SIZE];
} TestAudio;

static void
testAudio_audio_callback(void* opaque, int free)
{
    TestAudio* ta = opaque;

    //printf("%s: pos=%d free=%d\n", __FUNCTION__, ta->pos, free);

    while (free > 0) {
        int  avail = SAMPLE_SIZE - ta->pos;
        if (avail > free)
            avail = free;

        AUD_write(ta->voice, ta->sample + ta->pos, avail);
        ta->pos += avail;
        if (ta->pos >= SAMPLE_SIZE)
            ta->pos = 0;

        free -= avail;
    }
}

static int
testAudio_init( TestAudio* ta )
{
    struct audsettings as;

    AUD_register_card("test_audio", &ta->card);

    as.freq       = 16000;
    as.nchannels  = 1;
    as.fmt        = AUD_FMT_S16;
    as.endianness = AUDIO_HOST_ENDIANNESS;

    ta->voice = AUD_open_out(
        &ta->card,
        ta->voice,
        "test_audio",
        ta,
        testAudio_audio_callback,
        &as);

    if (!ta->voice) {
        dprint("Cannot open test audio!");
        return -1;
    }
    ta->pos = 0;

    /* Initialize samples */
    int nn;
    for (nn = 0; nn < SAMPLE_SIZE; nn++) {
        ta->sample[nn] = (short)(((nn % (SAMPLE_SIZE/4))*65536/(SAMPLE_SIZE/4)) & 0xffff);
    }

    AUD_set_active_out(ta->voice, 1);
    return 0;
}

static TestAudio*  testAudio;

int
android_audio_test_start_out(void)
{
    if (!testAudio) {
        testAudio = malloc(sizeof(*testAudio));
        if (testAudio_init(testAudio) < 0) {
            free(testAudio);
            testAudio = NULL;
            fprintf(stderr, "Could not start audio test!\n");
            return -1;
        } else {
            printf("Audio test started!\n");
        }
    }
    return 0;
}

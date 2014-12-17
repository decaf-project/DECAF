/*
 * QEMU OS X CoreAudio audio driver
 *
 * Copyright (c) 2008 The Android Open Source Project
 * Copyright (c) 2005 Mike Kronenberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <CoreAudio/CoreAudio.h>
#include <string.h>             /* strerror */
#include <pthread.h>            /* pthread_X */

#include "qemu-common.h"
#include "audio.h"

#define AUDIO_CAP "coreaudio"
#include "audio_int.h"

#if 0
#  define  D(...)  fprintf(stderr, __VA_ARGS__)
#else
#  define  D(...)  ((void)0)
#endif

struct {
    int out_buffer_frames;
    int out_nbuffers;
    int in_buffer_frames;
    int in_nbuffers;
    int isAtexit;
} conf = {
    .out_buffer_frames = 512,
    .out_nbuffers = 4,
    .in_buffer_frames = 512,
    .in_nbuffers = 4,
    .isAtexit = 0
};

/***************************************************************************************/
/***************************************************************************************/
/***                                                                                 ***/
/***       U T I L I T Y   R O U T I N E S                                           ***/
/***                                                                                 ***/
/***************************************************************************************/
/***************************************************************************************/

static void coreaudio_logstatus (OSStatus status)
{
    char *str = "BUG";

    switch(status) {
    case kAudioHardwareNoError:
        str = "kAudioHardwareNoError";
        break;

    case kAudioHardwareNotRunningError:
        str = "kAudioHardwareNotRunningError";
        break;

    case kAudioHardwareUnspecifiedError:
        str = "kAudioHardwareUnspecifiedError";
        break;

    case kAudioHardwareUnknownPropertyError:
        str = "kAudioHardwareUnknownPropertyError";
        break;

    case kAudioHardwareBadPropertySizeError:
        str = "kAudioHardwareBadPropertySizeError";
        break;

    case kAudioHardwareIllegalOperationError:
        str = "kAudioHardwareIllegalOperationError";
        break;

    case kAudioHardwareBadDeviceError:
        str = "kAudioHardwareBadDeviceError";
        break;

    case kAudioHardwareBadStreamError:
        str = "kAudioHardwareBadStreamError";
        break;

    case kAudioHardwareUnsupportedOperationError:
        str = "kAudioHardwareUnsupportedOperationError";
        break;

    case kAudioDeviceUnsupportedFormatError:
        str = "kAudioDeviceUnsupportedFormatError";
        break;

    case kAudioDevicePermissionsError:
        str = "kAudioDevicePermissionsError";
        break;

    default:
        AUD_log (AUDIO_CAP, "Reason: status code %ld\n", status);
        return;
    }

    AUD_log (AUDIO_CAP, "Reason: %s\n", str);
}

static void GCC_FMT_ATTR (2, 3) coreaudio_logerr (
    OSStatus status,
    const char *fmt,
    ...
    )
{
    va_list ap;

    va_start (ap, fmt);
    AUD_log (AUDIO_CAP, fmt, ap);
    va_end (ap);

    coreaudio_logstatus (status);
}

static void GCC_FMT_ATTR (3, 4) coreaudio_logerr2 (
    OSStatus status,
    const char *typ,
    const char *fmt,
    ...
    )
{
    va_list ap;

    AUD_log (AUDIO_CAP, "Could not initialize %s\n", typ);

    va_start (ap, fmt);
    AUD_vlog (AUDIO_CAP, fmt, ap);
    va_end (ap);

    coreaudio_logstatus (status);
}

/***************************************************************************************/
/***************************************************************************************/
/***                                                                                 ***/
/***       S H A R E D   I N / O U T   V O I C E                                     ***/
/***                                                                                 ***/
/***************************************************************************************/
/***************************************************************************************/

typedef struct coreAudioVoice {
    pthread_mutex_t              mutex;
    AudioDeviceID                deviceID;
    Boolean                      isInput;
    UInt32                       bufferFrameSize;
    AudioStreamBasicDescription  streamBasicDescription;
    AudioDeviceIOProc            ioproc;
    int                          live;
    int                          decr;
    int                          pos;
} coreaudioVoice;

static inline UInt32
coreaudio_voice_isPlaying (coreaudioVoice *core)
{
    OSStatus status;
    UInt32 result = 0;
    UInt32 propertySize = sizeof(core->deviceID);
    status = AudioDeviceGetProperty(
        core->deviceID, 0, core->isInput,
        kAudioDevicePropertyDeviceIsRunning, &propertySize, &result);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr(status,
                         "Could not determine whether Device is playing\n");
    }
    return result;
}

static void coreaudio_atexit (void)
{
    conf.isAtexit = 1;
}

static int coreaudio_voice_lock (coreaudioVoice *core, const char *fn_name)
{
    int err;

    err = pthread_mutex_lock (&core->mutex);
    if (err) {
        dolog ("Could not lock voice for %s\nReason: %s\n",
               fn_name, strerror (err));
        return -1;
    }
    return 0;
}

static int
coreaudio_voice_unlock (coreaudioVoice *core, const char *fn_name)
{
    int err;

    err = pthread_mutex_unlock (&core->mutex);
    if (err) {
        dolog ("Could not unlock voice for %s\nReason: %s\n",
               fn_name, strerror (err));
        return -1;
    }
    return 0;
}

static int
coreaudio_voice_ctl (coreaudioVoice*  core, int cmd)
{
    OSStatus status;

    switch (cmd) {
    case VOICE_ENABLE:
        /* start playback */
        D("%s: %s started\n", __FUNCTION__, core->isInput ? "input" : "output");
        if (!coreaudio_voice_isPlaying(core)) {
            status = AudioDeviceStart(core->deviceID, core->ioproc);
            if (status != kAudioHardwareNoError) {
                coreaudio_logerr (status, "Could not resume playback\n");
            }
        }
        break;

    case VOICE_DISABLE:
        /* stop playback */
        D("%s: %s stopped\n", __FUNCTION__, core->isInput ? "input" : "output");
        if (!conf.isAtexit) {
            if (coreaudio_voice_isPlaying(core)) {
                status = AudioDeviceStop(core->deviceID, core->ioproc);
                if (status != kAudioHardwareNoError) {
                    coreaudio_logerr (status, "Could not pause playback\n");
                }
            }
        }
        break;
    }
    return 0;
}

static void
coreaudio_voice_fini (coreaudioVoice*  core)
{
    OSStatus status;
    int err;

    if (!conf.isAtexit) {
        /* stop playback */
        coreaudio_voice_ctl(core, VOICE_DISABLE);

        /* remove callback */
        status = AudioDeviceRemoveIOProc(core->deviceID, core->ioproc);
        if (status != kAudioHardwareNoError) {
            coreaudio_logerr (status, "Could not remove IOProc\n");
        }
    }
    core->deviceID = kAudioDeviceUnknown;

    /* destroy mutex */
    err = pthread_mutex_destroy(&core->mutex);
    if (err) {
        dolog("Could not destroy mutex\nReason: %s\n", strerror (err));
    }
}


static int
coreaudio_voice_init (coreaudioVoice*    core,
                      struct audsettings*  as,
                      int                frameSize,
                      AudioDeviceIOProc  ioproc,
                      void*              hw,
                      int                input)
{
    OSStatus  status;
    UInt32    propertySize;
    int       err;
    int       bits = 8;
    AudioValueRange frameRange;
    const char*  typ = input ? "input" : "playback";

    core->isInput = input ? true : false;

    /* create mutex */
    err = pthread_mutex_init(&core->mutex, NULL);
    if (err) {
        dolog("Could not create mutex\nReason: %s\n", strerror (err));
        return -1;
    }

    if (as->fmt == AUD_FMT_S16 || as->fmt == AUD_FMT_U16) {
        bits = 16;
    }

    // TODO: audio_pcm_init_info (&hw->info, as);
    /* open default output device */
   /* note: we use DefaultSystemOutputDevice because DefaultOutputDevice seems to
    * always link to the internal speakers, and not the ones selected through system properties
    * go figure...
    */
    propertySize = sizeof(core->deviceID);
    status = AudioHardwareGetProperty(
        input ? kAudioHardwarePropertyDefaultInputDevice :
                kAudioHardwarePropertyDefaultSystemOutputDevice,
        &propertySize,
        &core->deviceID);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr2 (status, typ,
                           "Could not get default %s device\n", typ);
        return -1;
    }
    if (core->deviceID == kAudioDeviceUnknown) {
        dolog ("Could not initialize %s - Unknown Audiodevice\n", typ);
        return -1;
    }

    /* get minimum and maximum buffer frame sizes */
    propertySize = sizeof(frameRange);
    status = AudioDeviceGetProperty(
        core->deviceID,
        0,
        core->isInput,
        kAudioDevicePropertyBufferFrameSizeRange,
        &propertySize,
        &frameRange);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr2 (status, typ,
                           "Could not get device buffer frame range\n");
        return -1;
    }

    if (frameRange.mMinimum > frameSize) {
        core->bufferFrameSize = (UInt32) frameRange.mMinimum;
        dolog ("warning: Upsizing Output Buffer Frames to %f\n", frameRange.mMinimum);
    }
    else if (frameRange.mMaximum < frameSize) {
        core->bufferFrameSize = (UInt32) frameRange.mMaximum;
        dolog ("warning: Downsizing Output Buffer Frames to %f\n", frameRange.mMaximum);
    }
    else {
        core->bufferFrameSize = frameSize;
    }

    /* set Buffer Frame Size */
    propertySize = sizeof(core->bufferFrameSize);
    status = AudioDeviceSetProperty(
        core->deviceID,
        NULL,
        0,
        core->isInput,
        kAudioDevicePropertyBufferFrameSize,
        propertySize,
        &core->bufferFrameSize);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr2 (status, typ,
                           "Could not set device buffer frame size %ld\n",
                           core->bufferFrameSize);
        return -1;
    }

    /* get Buffer Frame Size */
    propertySize = sizeof(core->bufferFrameSize);
    status = AudioDeviceGetProperty(
        core->deviceID,
        0,
        core->isInput,
        kAudioDevicePropertyBufferFrameSize,
        &propertySize,
        &core->bufferFrameSize);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr2 (status, typ,
                           "Could not get device buffer frame size\n");
        return -1;
    }
    // TODO: hw->samples = *pNBuffers * core->bufferFrameSize;

    /* get StreamFormat */
    propertySize = sizeof(core->streamBasicDescription);
    status = AudioDeviceGetProperty(
        core->deviceID,
        0,
        core->isInput,
        kAudioDevicePropertyStreamFormat,
        &propertySize,
        &core->streamBasicDescription);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr2 (status, typ,
                           "Could not get Device Stream properties\n");
        core->deviceID = kAudioDeviceUnknown;
        return -1;
    }

    /* set Samplerate */
    core->streamBasicDescription.mSampleRate = (Float64) as->freq;
    propertySize = sizeof(core->streamBasicDescription);
    status = AudioDeviceSetProperty(
        core->deviceID,
        0,
        0,
        core->isInput,
        kAudioDevicePropertyStreamFormat,
        propertySize,
        &core->streamBasicDescription);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr2 (status, typ, "Could not set samplerate %d\n",
                           as->freq);
        core->deviceID = kAudioDeviceUnknown;
        return -1;
    }

    /* set Callback */
    core->ioproc = ioproc;
    status = AudioDeviceAddIOProc(core->deviceID, ioproc, hw);
    if (status != kAudioHardwareNoError) {
        coreaudio_logerr2 (status, typ, "Could not set IOProc\n");
        core->deviceID = kAudioDeviceUnknown;
        return -1;
    }

    /* start Playback */
    if (!input && !coreaudio_voice_isPlaying(core)) {
        status = AudioDeviceStart(core->deviceID, core->ioproc);
        if (status != kAudioHardwareNoError) {
            coreaudio_logerr2 (status, typ, "Could not start playback\n");
            AudioDeviceRemoveIOProc(core->deviceID, core->ioproc);
            core->deviceID = kAudioDeviceUnknown;
            return -1;
        }
    }

    return 0;
}


/***************************************************************************************/
/***************************************************************************************/
/***                                                                                 ***/
/***       O U T P U T   V O I C E                                                   ***/
/***                                                                                 ***/
/***************************************************************************************/
/***************************************************************************************/

typedef struct coreaudioVoiceOut {
    HWVoiceOut                   hw;
    coreaudioVoice               core[1];
} coreaudioVoiceOut;

#define  CORE_OUT(hw)  ((coreaudioVoiceOut*)(hw))->core


static int coreaudio_run_out (HWVoiceOut *hw, int live)
{
    int decr;
    coreaudioVoice *core = CORE_OUT(hw);

    if (coreaudio_voice_lock (core, "coreaudio_run_out")) {
        return 0;
    }

    if (core->decr > live) {
        ldebug ("core->decr %d live %d core->live %d\n",
                core->decr,
                live,
                core->live);
    }

    decr = audio_MIN (core->decr, live);
    core->decr -= decr;

    core->live = live - decr;
    hw->rpos = core->pos;

    coreaudio_voice_unlock (core, "coreaudio_run_out");
    return decr;
}

/* callback to feed audiooutput buffer */
static OSStatus audioOutDeviceIOProc(
    AudioDeviceID inDevice,
    const AudioTimeStamp* inNow,
    const AudioBufferList* inInputData,
    const AudioTimeStamp* inInputTime,
    AudioBufferList* outOutputData,
    const AudioTimeStamp* inOutputTime,
    void* hwptr)
{
    UInt32 frame, frameCount;
    float *out = outOutputData->mBuffers[0].mData;
    HWVoiceOut *hw = hwptr;
    coreaudioVoice *core = CORE_OUT(hw);
    int rpos, live;
    struct st_sample *src;
#ifndef FLOAT_MIXENG
#ifdef RECIPROCAL
    const float scale = 1.f / UINT_MAX;
#else
    const float scale = UINT_MAX;
#endif
#endif

    if (coreaudio_voice_lock (core, "audioDeviceIOProc")) {
        inInputTime = 0;
        return 0;
    }

    frameCount = core->bufferFrameSize;
    live = core->live;

    /* if there are not enough samples, set signal and return */
    if (live < frameCount) {
        inInputTime = 0;
        coreaudio_voice_unlock (core, "audioDeviceIOProc(empty)");
        return 0;
    }

    rpos = core->pos;
    src = hw->mix_buf + rpos;

    /* fill buffer */
    for (frame = 0; frame < frameCount; frame++) {
#ifdef FLOAT_MIXENG
        *out++ = src[frame].l; /* left channel */
        *out++ = src[frame].r; /* right channel */
#else
#ifdef RECIPROCAL
        *out++ = src[frame].l * scale; /* left channel */
        *out++ = src[frame].r * scale; /* right channel */
#else
        *out++ = src[frame].l / scale; /* left channel */
        *out++ = src[frame].r / scale; /* right channel */
#endif
#endif
    }

    rpos = (rpos + frameCount) % hw->samples;
    core->decr += frameCount;
    core->pos = rpos;

    coreaudio_voice_unlock (core, "audioDeviceIOProc");
    return 0;
}

static int coreaudio_write (SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}

static int coreaudio_init_out (HWVoiceOut *hw, struct audsettings *as)
{
    coreaudioVoice*  core = CORE_OUT(hw);
    int err;

    audio_pcm_init_info (&hw->info, as);

    err = coreaudio_voice_init (core, as, conf.out_buffer_frames, audioOutDeviceIOProc, hw, 0);
    if (err < 0)
        return err;

    hw->samples = core->bufferFrameSize * conf.out_nbuffers;
    return 0;
}

static void coreaudio_fini_out (HWVoiceOut *hw)
{
    coreaudioVoice *core = CORE_OUT(hw);

    coreaudio_voice_fini (core);
}

static int
coreaudio_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    coreaudioVoice *core = CORE_OUT(hw);

    return coreaudio_voice_ctl (core, cmd);
}

/***************************************************************************************/
/***************************************************************************************/
/***                                                                                 ***/
/***       I N P U T   V O I C E                                                     ***/
/***                                                                                 ***/
/***************************************************************************************/
/***************************************************************************************/



typedef struct coreaudioVoiceIn {
    HWVoiceIn        hw;
    coreaudioVoice   core[1];
} coreaudioVoiceIn;

#define  CORE_IN(hw)  ((coreaudioVoiceIn *) (hw))->core


static int coreaudio_run_in (HWVoiceIn *hw, int live)
{
    int decr;

    coreaudioVoice *core = CORE_IN(hw);

    if (coreaudio_voice_lock (core, "coreaudio_run_in")) {
        return 0;
    }
    D("%s: core.decr=%d core.pos=%d\n", __FUNCTION__, core->decr, core->pos);
    decr        = core->decr;
    core->decr -= decr;
    hw->wpos    = core->pos;

    coreaudio_voice_unlock (core, "coreaudio_run_in");
    return decr;
}


/* callback to feed audiooutput buffer */
static OSStatus audioInDeviceIOProc(
    AudioDeviceID inDevice,
    const AudioTimeStamp* inNow,
    const AudioBufferList* inInputData,
    const AudioTimeStamp* inInputTime,
    AudioBufferList* outOutputData,
    const AudioTimeStamp* inOutputTime,
    void* hwptr)
{
    UInt32 frame, frameCount;
    float *in = inInputData->mBuffers[0].mData;
    HWVoiceIn *hw = hwptr;
    coreaudioVoice *core = CORE_IN(hw);
    int wpos, avail;
    struct st_sample *dst;
#ifndef FLOAT_MIXENG
#ifdef RECIPROCAL
    const float scale = 1.f / UINT_MAX;
#else
    const float scale = UINT_MAX;
#endif
#endif

    if (coreaudio_voice_lock (core, "audioDeviceIOProc")) {
        inInputTime = 0;
        return 0;
    }

    frameCount = core->bufferFrameSize;
    avail      = hw->samples - hw->total_samples_captured - core->decr;

    D("%s: enter avail=%d core.decr=%d core.pos=%d hw.samples=%d hw.total_samples_captured=%d frameCount=%d\n",
      __FUNCTION__, avail, core->decr, core->pos, hw->samples, hw->total_samples_captured, (int)frameCount);

    /* if there are not enough samples, set signal and return */
    if (avail < frameCount) {
        inInputTime = 0;
        coreaudio_voice_unlock (core, "audioDeviceIOProc(empty)");
        return 0;
    }

    wpos = core->pos;
    dst  = hw->conv_buf + wpos;

    /* fill buffer */
    for (frame = 0; frame < frameCount; frame++) {
#ifdef FLOAT_MIXENG
        dst[frame].l = *in++; /* left channel */
        dst[frame].r = *in++; /* right channel */
#else
#ifdef RECIPROCAL
        dst[frame].l = *in++ * scale; /* left channel */
        dst[frame].r = *in++ * scale; /* right channel */
#else
        dst[frame].l = *in++ / scale; /* left channel */
        dst[frame].r = *in++ / scale; /* right channel */
#endif
#endif
    }

    wpos = (wpos + frameCount) % hw->samples;
    core->decr += frameCount;
    core->pos   = wpos;

    D("exit: core.decr=%d core.pos=%d\n", core->decr, core->pos);
    coreaudio_voice_unlock (core, "audioDeviceIOProc");
    return 0;
}

static int
coreaudio_read (SWVoiceIn *sw, void *buf, int len)
{
    int  result = audio_pcm_sw_read (sw, buf, len);
    D("%s: audio_pcm_sw_read(%d) returned %d\n", __FUNCTION__, len, result);
    return result;
}

static int
coreaudio_init_in (HWVoiceIn *hw, struct audsettings *as)
{
    coreaudioVoice*  core = CORE_IN(hw);
    int              err;

    audio_pcm_init_info (&hw->info, as);

    err = coreaudio_voice_init (core, as, conf.in_buffer_frames, audioInDeviceIOProc, hw, 1);
    if (err < 0) {
        return err;
    }

    hw->samples = core->bufferFrameSize * conf.in_nbuffers;
    return 0;
}

static void
coreaudio_fini_in (HWVoiceIn *hw)
{

    coreaudioVoice*  core = CORE_IN(hw);

    coreaudio_voice_fini(core);
}

static int
coreaudio_ctl_in (HWVoiceIn *hw, int cmd, ...)
{
    coreaudioVoice*  core = CORE_IN(hw);

    return coreaudio_voice_ctl(core, cmd);
}

static void *coreaudio_audio_init (void)
{
    atexit(coreaudio_atexit);
    return &coreaudio_audio_init;
}

static void coreaudio_audio_fini (void *opaque)
{
    (void) opaque;
}

static struct audio_option coreaudio_options[] = {
    {
        .name  = "OUT_BUFFER_SIZE",
        .tag   = AUD_OPT_INT,
        .valp  = &conf.out_buffer_frames,
        .descr = "Size of the output buffer in frames"
    },
    {
        .name  = "OUT_BUFFER_COUNT",
        .tag   = AUD_OPT_INT,
        .valp  = &conf.out_nbuffers,
        .descr = "Number of output buffers"
    },
    {
        .name  = "IN_BUFFER_SIZE",
        .tag   = AUD_OPT_INT,
        .valp  = &conf.in_buffer_frames,
        .descr = "Size of the input buffer in frames"
    },
    {
        .name  = "IN_BUFFER_COUNT",
        .tag   = AUD_OPT_INT,
        .valp  = &conf.in_nbuffers,
        .descr = "Number of input buffers"
    },
    { /* End of list */ }
};

static struct audio_pcm_ops coreaudio_pcm_ops = {
    .init_out = coreaudio_init_out,
    .fini_out = coreaudio_fini_out,
    .run_out  = coreaudio_run_out,
    .write    = coreaudio_write,
    .ctl_out  = coreaudio_ctl_out,

    .init_in = coreaudio_init_in,
    .fini_in = coreaudio_fini_in,
    .run_in  = coreaudio_run_in,
    .read    = coreaudio_read,
    .ctl_in  = coreaudio_ctl_in
};

struct audio_driver coreaudio_audio_driver = {
    .name           = "coreaudio",
    .descr          = "CoreAudio http://developer.apple.com/audio/coreaudio.html",
    .options        = coreaudio_options,
    .init           = coreaudio_audio_init,
    .fini           = coreaudio_audio_fini,
    .pcm_ops        = &coreaudio_pcm_ops,
    .can_be_default = 1,
    .max_voices_out = 1,
    .max_voices_in  = 1,
    .voice_size_out = sizeof (coreaudioVoiceOut),
    .voice_size_in  = sizeof (coreaudioVoiceIn),
};

/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* Allow access to a raw mixing buffer */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_dibaudio.h"
#if defined(_WIN32_WCE) && (_WIN32_WCE < 300)
#include "win_ce_semaphore.h"
#endif


/* Audio driver functions */
static int DIB_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void DIB_ThreadInit(_THIS);
static void DIB_WaitAudio(_THIS);
static Uint8 *DIB_GetAudioBuf(_THIS);
static void DIB_PlayAudio(_THIS);
static void DIB_WaitDone(_THIS);
static void DIB_CloseAudio(_THIS);

int volatile   dibaudio_thread_debug = 0;
int volatile   dibaudio_cb_debug     = 0;
int volatile   dibaudio_main_debug   = 0;

/* Audio driver bootstrap functions */

static int Audio_Available(void)
{
	return(1);
}

static void Audio_DeleteDevice(SDL_AudioDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_AudioDevice *Audio_CreateDevice(int devindex)
{
	SDL_AudioDevice *this;

	/* Initialize all variables that we clean on shutdown */
	this = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
	if ( this ) {
		SDL_memset(this, 0, (sizeof *this));
		this->hidden = (struct SDL_PrivateAudioData *)
				SDL_malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( this ) {
			SDL_free(this);
		}
		return(0);
	}
	SDL_memset(this->hidden, 0, (sizeof *this->hidden));

	/* Set the function pointers */
	this->OpenAudio = DIB_OpenAudio;
	this->ThreadInit = DIB_ThreadInit;
	this->WaitAudio = DIB_WaitAudio;
	this->PlayAudio = DIB_PlayAudio;
	this->GetAudioBuf = DIB_GetAudioBuf;
	this->WaitDone = DIB_WaitDone;
	this->CloseAudio = DIB_CloseAudio;

	this->free = Audio_DeleteDevice;

	return this;
}

AudioBootStrap WAVEOUT_bootstrap = {
	"waveout", "Win95/98/NT/2000 WaveOut",
	Audio_Available, Audio_CreateDevice
};


/* The Win32 callback for filling the WAVE device */
static void CALLBACK FillSound(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
						DWORD dwParam1, DWORD dwParam2)
{
	SDL_AudioDevice *this = (SDL_AudioDevice *)dwInstance;

	/* Only service "buffer done playing" messages */
	if ( uMsg != WOM_DONE )
		return;

	/* Signal that we are done playing a buffer */
	dibaudio_cb_debug = 1;
	EnterCriticalSection( &audio_cs );
	dibaudio_cb_debug = 2;
	SetEvent( audio_event );
	dibaudio_cb_debug = 3;
	LeaveCriticalSection( &audio_cs );
	dibaudio_cb_debug = 4;
}

static void SetMMerror(char *function, MMRESULT code)
{
	size_t len;
	char errbuf[MAXERRORLENGTH];
#ifdef _WIN32_WCE
	wchar_t werrbuf[MAXERRORLENGTH];
#endif

	SDL_snprintf(errbuf, SDL_arraysize(errbuf), "%s: ", function);
	len = SDL_strlen(errbuf);

#ifdef _WIN32_WCE
	/* UNICODE version */
	waveOutGetErrorText(code, werrbuf, MAXERRORLENGTH-len);
	WideCharToMultiByte(CP_ACP,0,werrbuf,-1,errbuf+len,MAXERRORLENGTH-len,NULL,NULL);
#else
	waveOutGetErrorText(code, errbuf+len, (UINT)(MAXERRORLENGTH-len));
#endif

	SDL_SetError("%s",errbuf);
}

/* Set high priority for the audio thread */
static void DIB_ThreadInit(_THIS)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

void DIB_WaitAudio(_THIS)
{
	/* Wait for an audio chunk to finish */
	dibaudio_thread_debug = 1;
	WaitForSingleObject(audio_event, INFINITE);
	dibaudio_thread_debug = 2;
	ResetEvent( audio_event );
	dibaudio_thread_debug = 3;
}

Uint8 *DIB_GetAudioBuf(_THIS)
{
    Uint8 *retval;

	dibaudio_thread_debug = 4;
    EnterCriticalSection( &audio_cs );
	dibaudio_thread_debug = 5;
	retval = (Uint8 *)(wavebuf[next_buffer].lpData);
	return retval;
}

void DIB_PlayAudio(_THIS)
{
	/* Queue it up */
	dibaudio_thread_debug = 6;
	waveOutWrite(sound, &wavebuf[next_buffer], sizeof(wavebuf[0]));
	dibaudio_thread_debug = 7;
	next_buffer = (next_buffer+1)%NUM_BUFFERS;
	LeaveCriticalSection( &audio_cs );
	dibaudio_thread_debug = 8;
}

void DIB_WaitDone(_THIS)
{
	int i, left, tries = 5;
	
	dibaudio_thread_debug = 9;

   /* give some time for the wave output to send the last buffer,
	* but don't hang if one was cancelled
	*/
	do {
		left = NUM_BUFFERS;
		for ( i=0; i<NUM_BUFFERS; ++i ) {
			if ( wavebuf[i].dwFlags & WHDR_DONE ) {
				--left;
			}
		}
		if ( left == 0 )
			break;

		SDL_Delay(100);
	} while ( --tries > 0 );

	dibaudio_thread_debug = 10;
}

void DIB_CloseAudio(_THIS)
{
	int i;

	/* Close up audio */
	if ( audio_event != INVALID_HANDLE_VALUE ) {
#if defined(_WIN32_WCE) && (_WIN32_WCE < 300)
		CloseSynchHandle(audio_event);
#else
		CloseHandle(audio_event);
#endif
	}
	if ( sound ) {
	    waveOutReset(sound);
	}

	/* Clean up mixing buffers */
	for ( i=0; i<NUM_BUFFERS; ++i ) {
		if ( wavebuf[i].dwUser != 0xFFFF ) {
			waveOutUnprepareHeader(sound, &wavebuf[i],
						sizeof(wavebuf[i]));
			wavebuf[i].dwUser = 0xFFFF;
		}
	}
	/* Free raw mixing buffer */
	if ( mixbuf != NULL ) {
		SDL_free(mixbuf);
		mixbuf = NULL;
	}

	if ( sound )
		waveOutClose(sound);
}

int DIB_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
	MMRESULT result;
	int i;
	WAVEFORMATEX waveformat;

	/* Initialize the wavebuf structures for closing */
	sound = NULL;
	InitializeCriticalSection( &audio_cs );
	audio_event = INVALID_HANDLE_VALUE;
	for ( i = 0; i < NUM_BUFFERS; ++i )
		wavebuf[i].dwUser = 0xFFFF;
	mixbuf = NULL;

	/* Set basic WAVE format parameters */
	SDL_memset(&waveformat, 0, sizeof(waveformat));
	waveformat.wFormatTag = WAVE_FORMAT_PCM;

	/* Determine the audio parameters from the AudioSpec */
	switch ( spec->format & 0xFF ) {
		case 8:
			/* Unsigned 8 bit audio data */
			spec->format = AUDIO_U8;
			waveformat.wBitsPerSample = 8;
			break;
		case 16:
			/* Signed 16 bit audio data */
			spec->format = AUDIO_S16;
			waveformat.wBitsPerSample = 16;
			break;
		default:
			SDL_SetError("Unsupported audio format");
			return(-1);
	}
	waveformat.nChannels = spec->channels;
	waveformat.nSamplesPerSec = spec->freq;
	waveformat.nBlockAlign =
		waveformat.nChannels * (waveformat.wBitsPerSample/8);
	waveformat.nAvgBytesPerSec = 
		waveformat.nSamplesPerSec * waveformat.nBlockAlign;

	/* Check the buffer size -- minimum of 1/4 second (word aligned) */
	if ( spec->samples < (spec->freq/4) )
		spec->samples = ((spec->freq/4)+3)&~3;

	/* Update the fragment size as size in bytes */
	SDL_CalculateAudioSpec(spec);

	/* Open the audio device */
	result = waveOutOpen(&sound, WAVE_MAPPER, &waveformat,
			(DWORD_PTR)FillSound, (DWORD_PTR)this, CALLBACK_FUNCTION);
	if ( result != MMSYSERR_NOERROR ) {
		SetMMerror("waveOutOpen()", result);
		return(-1);
	}

#ifdef SOUND_DEBUG
	/* Check the sound device we retrieved */
	{
		WAVEOUTCAPS caps;

		result = waveOutGetDevCaps((UINT)sound, &caps, sizeof(caps));
		if ( result != MMSYSERR_NOERROR ) {
			SetMMerror("waveOutGetDevCaps()", result);
			return(-1);
		}
		printf("Audio device: %s\n", caps.szPname);
	}
#endif

	/* Create the audio buffer semaphore */
	audio_event = CreateEvent( NULL, TRUE, FALSE, NULL );
	if ( audio_event == NULL ) {
		SDL_SetError("Couldn't create semaphore");
		return(-1);
	}

	/* Create the sound buffers */
	mixbuf = (Uint8 *)SDL_malloc(NUM_BUFFERS*spec->size);
	if ( mixbuf == NULL ) {
		SDL_SetError("Out of memory");
		return(-1);
	}
	for ( i = 0; i < NUM_BUFFERS; ++i ) {
		SDL_memset(&wavebuf[i], 0, sizeof(wavebuf[i]));
		wavebuf[i].lpData = (LPSTR) &mixbuf[i*spec->size];
		wavebuf[i].dwBufferLength = spec->size;
		wavebuf[i].dwFlags = WHDR_DONE;
		result = waveOutPrepareHeader(sound, &wavebuf[i],
							sizeof(wavebuf[i]));
		if ( result != MMSYSERR_NOERROR ) {
			SetMMerror("waveOutPrepareHeader()", result);
			return(-1);
		}
	}

	/* Ready to go! */
	next_buffer = 0;
	return(0);
}

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
#ifndef ANDROID_AUDIO_TEST_H
#define ANDROID_AUDIO_TEST_H

/* Start the audio test output, this will register a virtual sound
 * device that sends a saw signal to the audio sub-system. This is
 * used to test that the audio backend works without having to boot
 * a full system and launch a music application.
 */
extern int android_audio_test_start_out(void);

#endif /* ANDROID_AUDIO_TEST_H */

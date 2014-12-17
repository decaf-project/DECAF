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

/*
 * Contains declarations of routines and that are used in the course
 * of the emulator's core initialization.
 */

#ifndef QEMU_ANDROID_CORE_INIT_UI_UTILS_H_
#define QEMU_ANDROID_CORE_INIT_UI_UTILS_H_

/* Formats and sends back to the UI message indicating successful completion
 * of the core initialization.
 */
void android_core_init_completed(void);

/* Builds and sends initialization failure message back to the UI that started
 * the core. After that exits the process.
 * Param:
 *  Parameters match those that are passed to sprintf routine to format an
 *  error message for the error that has occured.
 */
void android_core_init_failure(const char* fmt, ...);

/* Sends successful initialization completion message back to the UI, and exits
 * the application.
 * Param:
 *  exit_status Exit status, that will be passed to the "exit" routine.
 */
void android_core_init_exit(int exit_status);

#endif  // QEMU_ANDROID_CORE_INIT_UI_UTILS_H_

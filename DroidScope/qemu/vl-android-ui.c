/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
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

/* the following is needed on Linux to define ptsname() in stdlib.h */
#if defined(__linux__)
#define _GNU_SOURCE 1
#endif

#ifndef _WIN32
#include <sys/wait.h>
#endif  // _WIN32

#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>
#endif

#include "qemu-common.h"
#include "net.h"
#include "console.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "block.h"
#include "sockets.h"
#include "audio/audio.h"

#include "android/android.h"
#include "charpipe.h"
#include "android/globals.h"
#include "android/utils/bufprint.h"
#include "android/utils/system.h"
#include "android/protocol/core-connection.h"
#include "android/protocol/attach-ui-impl.h"
#include "android/protocol/fb-updates-impl.h"
#include "android/protocol/user-events-proxy.h"
#include "android/protocol/core-commands-proxy.h"
#include "android/protocol/ui-commands-impl.h"
#include "android/qemulator.h"

static Looper*  mainLooper;

/***********************************************************/
/* I/O handling */

typedef struct IOHandlerRecord {
    LoopIo  io[1];
    IOHandler* fd_read;
    IOHandler* fd_write;
    int        running;
    int        deleted;
    void*      opaque;
    struct IOHandlerRecord *next;
} IOHandlerRecord;

static IOHandlerRecord *first_io_handler;

static void ioh_callback(void* opaque, int fd, unsigned events)
{
    IOHandlerRecord* ioh = opaque;
    ioh->running = 1;
    if ((events & LOOP_IO_READ) != 0) {
        ioh->fd_read(ioh->opaque);
    }
    if (!ioh->deleted && (events & LOOP_IO_WRITE) != 0) {
        ioh->fd_write(ioh->opaque);
    }
    ioh->running = 0;
    if (ioh->deleted) {
        loopIo_done(ioh->io);
        free(ioh);
    }
}

int qemu_set_fd_handler(int fd,
                        IOHandler *fd_read,
                        IOHandler *fd_write,
                        void *opaque)
{
    IOHandlerRecord **pioh, *ioh;

    if (!fd_read && !fd_write) {
        pioh = &first_io_handler;
        for(;;) {
            ioh = *pioh;
            if (ioh == NULL)
                return 0;
            if (ioh->io->fd == fd) {
                break;
            }
            pioh = &ioh->next;
        }
        if (ioh->running) {
            ioh->deleted = 1;
        } else {
            *pioh = ioh->next;
            loopIo_done(ioh->io);
            free(ioh);
        }
    } else {
        for(ioh = first_io_handler; ioh != NULL; ioh = ioh->next) {
            if (ioh->io->fd == fd)
                goto found;
        }
        ANEW0(ioh);
        ioh->next = first_io_handler;
        first_io_handler = ioh;
        loopIo_init(ioh->io, mainLooper, fd, ioh_callback, ioh);
    found:
        ioh->fd_read  = fd_read;
        ioh->fd_write = fd_write;
        ioh->opaque   = opaque;

        if (fd_read != NULL)
            loopIo_wantRead(ioh->io);
        else
            loopIo_dontWantRead(ioh->io);

        if (fd_write != NULL)
            loopIo_wantWrite(ioh->io);
        else
            loopIo_dontWantWrite(ioh->io);
    }
    return 0;
}

/***********************************************************/
/* main execution loop */

static LoopTimer  gui_timer[1];

static void gui_update(void *opaque)
{
    LoopTimer* timer = opaque;
    qframebuffer_pulse();
    loopTimer_startRelative(timer, GUI_REFRESH_INTERVAL);
}

static void init_gui_timer(Looper* looper)
{
    loopTimer_init(gui_timer, looper, gui_update, gui_timer);
    loopTimer_startRelative(gui_timer, 0);
    qframebuffer_invalidate_all();
}

/* Called from qemulator.c */
void qemu_system_shutdown_request(void)
{
    looper_forceQuit(mainLooper);
}

#ifndef _WIN32

static void termsig_handler(int signal)
{
    qemu_system_shutdown_request();
}

static void sigchld_handler(int signal)
{
    waitpid(-1, NULL, WNOHANG);
}

static void sighandler_setup(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = termsig_handler;
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &act, NULL);
}

#endif

#ifdef _WIN32
static BOOL WINAPI qemu_ctrl_handler(DWORD type)
{
    exit(STATUS_CONTROL_C_EXIT);
    return TRUE;
}
#endif

int qemu_main(int argc, char **argv, char **envp)
{
#ifndef _WIN32
    {
        struct sigaction act;
        sigfillset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &act, NULL);
    }
#else
    SetConsoleCtrlHandler(qemu_ctrl_handler, TRUE);
    /* Note: cpu_interrupt() is currently not SMP safe, so we force
       QEMU to run on a single CPU */
    {
        HANDLE h;
        DWORD mask, smask;
        int i;
        h = GetCurrentProcess();
        if (GetProcessAffinityMask(h, &mask, &smask)) {
            for(i = 0; i < 32; i++) {
                if (mask & (1 << i))
                    break;
            }
            if (i != 32) {
                mask = 1 << i;
                SetProcessAffinityMask(h, mask);
            }
        }
    }
#endif

#ifdef _WIN32
    socket_init();
#endif

#ifndef _WIN32
    /* must be after terminal init, SDL library changes signal handlers */
    sighandler_setup();
#endif

    mainLooper = looper_newGeneric();

    /* Register a timer to call qframebuffer_pulse periodically */
    init_gui_timer(mainLooper);

    // Connect to the core's framebuffer service
    if (fbUpdatesImpl_create(attachUiImpl_get_console_socket(), "-raw",
                             qemulator_get_first_framebuffer(qemulator_get()),
                             mainLooper)) {
        return -1;
    }

    // Attach the recepient of UI commands.
    if (uiCmdImpl_create(attachUiImpl_get_console_socket(), mainLooper)) {
        return -1;
    }

    looper_run(mainLooper);

    fbUpdatesImpl_destroy();
    userEventsProxy_destroy();
    coreCmdProxy_destroy();
    uiCmdImpl_destroy();
    attachUiImpl_destroy();

    return 0;
}

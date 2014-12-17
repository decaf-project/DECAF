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

#include "android/utils/assert.h"
#include "android/utils/reflist.h"
#include "android/utils/refset.h"
#include "android/utils/system.h"
#include "android/looper.h"
#include "iolooper.h"
#include "sockets.h"
#include <inttypes.h>
#include <limits.h>
#include <errno.h>

/**********************************************************************
 **********************************************************************
 *****
 *****  T I M E R S
 *****
 **********************************************************************
 **********************************************************************/

typedef struct GLoopTimer GLoopTimer;
typedef struct GLoopIo    GLoopIo;
typedef struct GLooper    GLooper;

struct GLoopTimer {
    Duration      deadline;
    LoopTimerFunc callback;
    void*         opaque;
    GLooper*      looper;
    GLoopTimer*   activeNext;
};

static Duration glooper_now(Looper* ll);

static void glooper_addActiveTimer(GLooper* looper, GLoopTimer* timer);
static void glooper_delActiveTimer(GLooper* looper, GLoopTimer* timer);
static void glooper_addTimer(GLooper* looper, GLoopTimer* timer);
static void glooper_delTimer(GLooper* looper, GLoopTimer* timer);

static void
glooptimer_stop(void* impl)
{
    GLoopTimer*  tt = impl;
    if (tt->deadline != DURATION_INFINITE) {
        glooper_delActiveTimer(tt->looper, tt);
        tt->deadline = DURATION_INFINITE;
    }
}

static void
glooptimer_startAbsolute(void* impl, Duration deadline_ms)
{
    GLoopTimer*  tt = impl;

    /* Stop the timer if it was active */
    if (tt->deadline != DURATION_INFINITE)
        glooptimer_stop(tt);

    /* Another way to stop a timer */
    if (deadline_ms == DURATION_INFINITE)
        return;

    tt->deadline = deadline_ms;
    glooper_addActiveTimer(tt->looper, tt);
}

static void
glooptimer_startRelative(void* impl, Duration  timeout_ms)
{
    GLoopTimer*  tt = impl;

    if (timeout_ms == DURATION_INFINITE) {  /* another way to stop the timer */
        glooptimer_stop(tt);
    } else {
        glooptimer_startAbsolute(tt, timeout_ms + glooper_now((Looper*)tt->looper));
    }
}

static int
glooptimer_isActive(void* impl)
{
    GLoopTimer*  tt = impl;
    return (tt->deadline != DURATION_INFINITE);
}

static void
glooptimer_free(void* impl)
{
    GLoopTimer*  tt = impl;

    if (tt->deadline != DURATION_INFINITE)
        glooptimer_stop(tt);

    glooper_delTimer(tt->looper, tt);
    AFREE(tt);
}

static const LoopTimerClass  glooptimer_class = {
    glooptimer_startRelative,
    glooptimer_startAbsolute,
    glooptimer_stop,
    glooptimer_isActive,
    glooptimer_free
};

static void
glooper_timer_init(Looper*       looper,
                   LoopTimer*    timer,
                   LoopTimerFunc callback,
                   void*         opaque)
{
    GLoopTimer* tt;

    ANEW0(tt);

    tt->deadline = DURATION_INFINITE;
    tt->callback = callback;
    tt->opaque   = opaque;
    tt->looper   = (GLooper*) looper;

    glooper_addTimer(tt->looper, tt);

    timer->impl  = tt;
    timer->clazz = (LoopTimerClass*) &glooptimer_class;
}

/**********************************************************************
 **********************************************************************
 *****
 *****  I / O
 *****
 **********************************************************************
 **********************************************************************/

struct GLoopIo {
    int         fd;
    LoopIoFunc  callback;
    void*       opaque;
    unsigned    wanted;
    unsigned    ready;
    GLooper*    looper;
};

static void glooper_delPendingIo(GLooper* looper, GLoopIo* io);
static void glooper_addIo(GLooper* looper, GLoopIo* io);
static void glooper_delIo(GLooper* looper, GLoopIo* io);
static void glooper_modifyFd(GLooper* looper, int fd, int oldwanted, int newwanted);

/* used to indicate that the set of wanted flags has changed */
static void
gloopio_modify(GLoopIo* io, unsigned wanted)
{
    /* If nothing changed, return */
    if (io->wanted == wanted)
        return;

    /* If we are pending, and we're not interested by the
     * current ready flags, remove from list */
    if (io->ready != 0 && (io->ready & wanted) == 0) {
        glooper_delPendingIo(io->looper, io);
    }
    io->ready &= wanted;
    glooper_modifyFd(io->looper, io->fd, io->wanted, wanted);
    io->wanted = wanted;
}

static void
gloopio_wantRead(void* impl)
{
    GLoopIo* io = impl;
    gloopio_modify(io, io->wanted | LOOP_IO_READ);
}

static void
gloopio_wantWrite(void* impl)
{
    GLoopIo* io = impl;
    gloopio_modify(io, io->wanted | LOOP_IO_WRITE);
}

static void
gloopio_dontWantRead(void* impl)
{
    GLoopIo* io = impl;
    gloopio_modify(io, io->wanted & ~LOOP_IO_READ);
}

static void
gloopio_dontWantWrite(void* impl)
{
    GLoopIo* io = impl;
    gloopio_modify(io, io->wanted & ~LOOP_IO_WRITE);
}

static unsigned
gloopio_poll(void* impl)
{
    GLoopIo* io = impl;
    return io->ready;
}

static void
gloopio_free(void* impl)
{
    GLoopIo* io = impl;
    if (io->ready != 0)
        glooper_delPendingIo(io->looper, io);

    glooper_delIo(io->looper, io);
    AFREE(io);
}

static LoopIoClass  gloopio_class = {
    gloopio_wantRead,
    gloopio_wantWrite,
    gloopio_dontWantRead,
    gloopio_dontWantWrite,
    gloopio_poll,
    gloopio_free
};

static void
glooper_io_init(Looper* looper, LoopIo* user, int fd, LoopIoFunc callback, void* opaque)
{
    GLooper*  gg = (GLooper*)looper;
    GLoopIo*  io;

    ANEW0(io);
    io->fd       = fd;
    io->callback = callback;
    io->opaque   = opaque;
    io->looper   = (GLooper*) looper;
    io->wanted   = 0;
    io->ready    = 0;

    socket_set_nonblock(fd);

    glooper_addIo(gg, io);

    user->impl  = io;
    user->clazz = (LoopIoClass*) &gloopio_class;
}

/**********************************************************************
 **********************************************************************
 *****
 *****  L O O P E R
 *****
 **********************************************************************
 **********************************************************************/

struct GLooper {
    Looper       looper;
    ARefSet      timers[1];    /* set of all timers */
    GLoopTimer*  activeTimers; /* sorted list of active timers */

    ARefSet      ios[1];        /* set of all i/o waiters */
    ARefSet      pendingIos[1]; /* list of pending i/o waiters */
    int          numActiveIos;  /* number of active LoopIo objects */

    IoLooper*    iolooper;
    int          running;
};

static void
glooper_addTimer(GLooper* looper, GLoopTimer* tt)
{
    arefSet_add(looper->timers, tt);
}

static void
glooper_delTimer(GLooper* looper, GLoopTimer* tt)
{
    arefSet_del(looper->timers, tt);
}

static void
glooper_addActiveTimer(GLooper* looper, GLoopTimer* tt)
{
    Duration  deadline = tt->deadline;
    GLoopTimer** pnode = &looper->activeTimers;
    for (;;) {
        GLoopTimer* node = *pnode;
        if (node == NULL || node->deadline > deadline)
            break;
        pnode = &node->activeNext;
    }
    tt->activeNext = *pnode;
    *pnode         = tt;
}

static void
glooper_delActiveTimer(GLooper* looper, GLoopTimer* tt)
{
    GLoopTimer** pnode = &looper->activeTimers;
    for (;;) {
        if (*pnode == NULL)
            break;
        if (*pnode == tt) {
            *pnode = tt->activeNext;
            tt->activeNext = NULL;
            break;
        }
        pnode = &(*pnode)->activeNext;
    }
}

static void
glooper_addIo(GLooper* looper, GLoopIo* io)
{
    arefSet_add(looper->ios, io);
}

static void
glooper_delIo(GLooper* looper, GLoopIo* io)
{
    arefSet_del(looper->ios, io);
}

static void
glooper_delPendingIo(GLooper* looper, GLoopIo* io)
{
    arefSet_del(looper->pendingIos, io);
}

static void
glooper_modifyFd(GLooper* looper, int fd, int oldWanted, int newWanted)
{
    if (oldWanted == 0 && newWanted != 0)
        looper->numActiveIos += 1;

    if (oldWanted != 0 && newWanted == 0)
        looper->numActiveIos -= 1;

    iolooper_modify(looper->iolooper, fd, oldWanted, newWanted);
}

static Duration
glooper_now(Looper*  ll)
{
    return iolooper_now();
}

static void
glooper_forceQuit(Looper* ll)
{
    GLooper*  looper = (GLooper*)ll;
    looper->running = 0;
}

static int
glooper_run(Looper*  ll, Duration loop_deadline_ms)
{
    GLooper*   looper = (GLooper*) ll;
    IoLooper*  iol    = looper->iolooper;

    looper->running = 1;

    while (looper->running)
    {
        int ret;

        /* Exit prematurely if we detect that we don't have any active timer
         * and no active LoopIo
         */
        if (looper->numActiveIos == 0 && looper->activeTimers == NULL)
            return EWOULDBLOCK;

        /* First, compute next deadline */
        Duration  deadline = DURATION_INFINITE;

        if (looper->activeTimers != NULL)
            deadline = looper->activeTimers->deadline;

        if (deadline > loop_deadline_ms)
            deadline = loop_deadline_ms;

        ret = iolooper_wait_absolute(iol, deadline);
        if (ret < 0) { /* error, force stop ! */
            break;
        }
        if (ret > 0) {
            unsigned ready;
            GLoopIo* io;

            /* Add io waiters to the pending list */
            AREFSET_FOREACH(looper->ios, io, {
                if (io->wanted == 0)
                    continue;

                ready = 0;

                if (iolooper_is_read(iol, io->fd))
                    ready |= LOOP_IO_READ;

                if (iolooper_is_write(iol, io->fd))
                    ready |= LOOP_IO_WRITE;

                io->ready = ready;
                if (ready != 0) {
                    arefSet_add(looper->pendingIos, io);
                }
            });
        }

        /* Do we have any expired timers here ? */
        GLoopTimer*  pendingTimers = NULL;
        GLoopTimer** pendingLastP  = &pendingTimers;

        deadline = iolooper_now();
        for (;;) {
            GLoopTimer*  timer = looper->activeTimers;
            if (timer == NULL || timer->deadline > deadline)
                break;

            /* remove from active list, and append to pending one */
            timer->deadline      = DURATION_INFINITE;
            looper->activeTimers = timer->activeNext;

            *pendingLastP     = timer;
            timer->activeNext = NULL;
            pendingLastP      = &timer->activeNext;
        }

        /* Fire the pending timers, if any. We do that in a separate
         * step because the callbacks could modify the active list
         * by starting/stopping other timers.
         */
        {
            GLoopTimer*  timer;
            while ((timer = pendingTimers) != NULL) {
                pendingTimers     = timer->activeNext;
                timer->activeNext = NULL;
                timer->callback(timer->opaque);
            }
        }

        /* Now fire the pending ios */
        {
            GLoopIo* io;
            AREFSET_FOREACH(looper->pendingIos,io,{
                io->callback(io->opaque,io->fd,io->ready);
            });
            arefSet_clear(looper->pendingIos);
        }

        if (deadline > loop_deadline_ms)
            return ETIMEDOUT;
    }
    return 0;
}

static void
glooper_free(Looper* ll)
{
    GLooper* looper = (GLooper*)ll;

    arefSet_done(looper->timers);
    looper->activeTimers = NULL;

    arefSet_done(looper->ios);
    arefSet_done(looper->pendingIos);

    iolooper_free(looper->iolooper);
    looper->iolooper = NULL;

    AFREE(looper);
}

Looper*  looper_newGeneric(void)
{
    GLooper*  looper;

    ANEW0(looper);

    looper->iolooper = iolooper_new();

    looper->looper.now        = glooper_now;
    looper->looper.timer_init = glooper_timer_init;
    looper->looper.io_init    = glooper_io_init;
    looper->looper.run        = glooper_run;
    looper->looper.forceQuit  = glooper_forceQuit;
    looper->looper.destroy    = glooper_free;

    /* Our implementation depends on these values being equal */
    AASSERT_INT(LOOP_IO_READ,  IOLOOPER_READ);
    AASSERT_INT(LOOP_IO_WRITE, IOLOOPER_WRITE);

    return &looper->looper;
}

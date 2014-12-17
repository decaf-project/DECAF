/* Copyright (C) 2007-2008 The Android Open Source Project
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

#include "android/utils/filelock.h"
#include "android/utils/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#ifdef _WIN32
#  include <process.h>
#  include <windows.h>
#  include <tlhelp32.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#  include <signal.h>
#endif

#define  D(...)  ((void)0)

#ifndef CHECKED
#  ifdef _WIN32
#    define   CHECKED(ret, call)    (ret) = (call)
#  else
#    define   CHECKED(ret, call)    do { (ret) = (call); } while ((ret) < 0 && errno == EINTR)
#  endif
#endif

/** FILE LOCKS SUPPORT
 **
 ** a FileLock is useful to prevent several emulator instances from using the same
 ** writable file (e.g. the userdata.img disk images).
 **
 ** create a FileLock object with filelock_create(), ithis function should return NULL
 ** only if the corresponding file path could not be locked.
 **
 ** all file locks are automatically released and destroyed when the program exits.
 ** the filelock_lock() function can also detect stale file locks that can linger
 ** when the emulator crashes unexpectedly, and will happily clean them for you
 **
 **  here's how it works, three files are used:
 **     file  - the data file accessed by the emulator
 **     lock  - a lock file  (file + '.lock')
 **     temp  - a temporary file make unique with mkstemp
 **
 **  when locking:
 **      create 'temp' and store our pid in it
 **      attemp to link 'lock' to 'temp'
 **         if the link succeeds, we obtain the lock
 **      unlink 'temp'
 **
 **  when unlocking:
 **      unlink 'lock'
 **
 **
 **  on Windows, 'lock' is a directory name. locking is equivalent to
 **  creating it...
 **
 **/

struct FileLock
{
  const char*  file;
  const char*  lock;
  char*        temp;
  int          locked;
  FileLock*    next;
};

/* used to cleanup all locks at emulator exit */
static FileLock*   _all_filelocks;


#define  LOCK_NAME   ".lock"
#define  TEMP_NAME   ".tmp-XXXXXX"

#ifdef _WIN32
#define  PIDFILE_NAME  "pid"
#endif

/* returns 0 on success, -1 on failure */
static int
filelock_lock( FileLock*  lock )
{
    int    ret;
#ifdef _WIN32
    int  pidfile_fd = -1;

    ret = _mkdir( lock->lock );
    if (ret < 0) {
        if (errno == ENOENT) {
            D( "could not access directory '%s', check path elements", lock->lock );
            return -1;
        } else if (errno != EEXIST) {
            D( "_mkdir(%s): %s", lock->lock, strerror(errno) );
            return -1;
        }

        /* if we get here, it's because the .lock directory already exists */
        /* check to see if there is a pid file in it                       */
        D("directory '%s' already exist, waiting a bit to ensure that no other emulator instance is starting", lock->lock );
        {
            int  _sleep = 200;
            int  tries;

            for ( tries = 4; tries > 0; tries-- )
            {
                pidfile_fd = open( lock->temp, O_RDONLY );

                if (pidfile_fd >= 0)
                    break;

                Sleep( _sleep );
                _sleep *= 2;
            }
        }

        if (pidfile_fd < 0) {
            D( "no pid file in '%s', assuming stale directory", lock->lock );
        }
        else
        {
            /* read the pidfile, and check wether the corresponding process is still running */
            char            buf[16];
            int             len, lockpid;
            HANDLE          processSnapshot;
            PROCESSENTRY32  pe32;
            int             is_locked = 0;

            len = read( pidfile_fd, buf, sizeof(buf)-1 );
            if (len < 0) {
                D( "could not read pid file '%s'", lock->temp );
                close( pidfile_fd );
                return -1;
            }
            buf[len] = 0;
            lockpid  = atoi(buf);

            /* PID 0 is the IDLE process, and 0 is returned in case of invalid input */
            if (lockpid == 0)
                lockpid = -1;

            close( pidfile_fd );

            pe32.dwSize     = sizeof( PROCESSENTRY32 );
            processSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

            if ( processSnapshot == INVALID_HANDLE_VALUE ) {
                D( "could not retrieve the list of currently active processes\n" );
                is_locked = 1;
            }
            else if ( !Process32First( processSnapshot, &pe32 ) )
            {
                D( "could not retrieve first process id\n" );
                CloseHandle( processSnapshot );
                is_locked = 1;
            }
            else
            {
                do {
                    if (pe32.th32ProcessID == lockpid) {
                        is_locked = 1;
                        break;
                    }
                } while (Process32Next( processSnapshot, &pe32 ) );

                CloseHandle( processSnapshot );
            }

            if (is_locked) {
                D( "the file '%s' is locked by process ID %d\n", lock->file, lockpid );
                return -1;
            }
        }
    }

    /* write our PID into the pid file */
    pidfile_fd = open( lock->temp, O_WRONLY | O_CREAT | O_TRUNC );
    if (pidfile_fd < 0) {
        if (errno == EACCES) {
            if ( path_delete_file( lock->temp ) < 0 ) {
                D( "could not remove '%s': %s\n", lock->temp, strerror(errno) );
                return -1;
            }
            pidfile_fd = open( lock->temp, O_WRONLY | O_CREAT | O_TRUNC );
        }
        if (pidfile_fd < 0) {
            D( "could not create '%s': %s\n", lock->temp, strerror(errno) );
            return -1;
        }
    }

    {
        char  buf[16];
        sprintf( buf, "%ld", GetCurrentProcessId() );
        ret = write( pidfile_fd, buf, strlen(buf) );
        close(pidfile_fd);
        if (ret < 0) {
            D( "could not write PID to '%s'\n", lock->temp );
            return -1;
        }
    }

    lock->locked = 1;
    return 0;
#else
    int    temp_fd = -1;
    int    lock_fd = -1;
    int    rc, tries, _sleep;
    FILE*  f = NULL;
    char   pid[8];
    struct stat  st_temp;

    strcpy( lock->temp, lock->file );
    strcat( lock->temp, TEMP_NAME );
    temp_fd = mkstemp( lock->temp );

    if (temp_fd < 0) {
        D("cannot create locking temp file '%s'", lock->temp );
        goto Fail;
    }

    sprintf( pid, "%d", getpid() );
    ret = write( temp_fd, pid, strlen(pid)+1 );
    if (ret < 0) {
        D( "cannot write to locking temp file '%s'", lock->temp);
        goto Fail;
    }
    close( temp_fd );
    temp_fd = -1;

    CHECKED(rc, lstat( lock->temp, &st_temp ));
    if (rc < 0) {
        D( "can't properly stat our locking temp file '%s'", lock->temp );
        goto Fail;
    }

    /* now attempt to link the temp file to the lock file */
    _sleep = 0;
    for ( tries = 4; tries > 0; tries-- )
    {
        struct stat  st_lock;
        int          rc;

        if (_sleep > 0) {
            if (_sleep > 2000000) {
                D( "cannot acquire lock file '%s'", lock->lock );
                goto Fail;
            }
            usleep( _sleep );
        }
        _sleep += 200000;

        /* the return value of link() is buggy on NFS */
        CHECKED(rc, link( lock->temp, lock->lock ));

        CHECKED(rc, lstat( lock->lock, &st_lock ));
        if (rc == 0 &&
            st_temp.st_rdev == st_lock.st_rdev &&
            st_temp.st_ino  == st_lock.st_ino  )
        {
            /* SUCCESS */
            lock->locked = 1;
            CHECKED(rc, unlink( lock->temp ));
            return 0;
        }

        /* if we get there, it means that the link() call failed */
        /* check the lockfile to see if it is stale              */
        if (rc == 0) {
            char    buf[16];
            time_t  now;
            int     lockpid = 0;
            int     lockfd;
            int     stale = 2;  /* means don't know */
            struct stat  st;

            CHECKED(rc, time( &now));
            st.st_mtime = now - 120;

            CHECKED(lockfd, open( lock->lock,O_RDONLY ));
            if ( lockfd >= 0 ) {
                int  len;

                CHECKED(len, read( lockfd, buf, sizeof(buf)-1 ));
                buf[len] = 0;
                lockpid = atoi(buf);

                CHECKED(rc, fstat( lockfd, &st ));
                if (rc == 0)
                  now = st.st_atime;

                CHECKED(rc, close(lockfd));
            }
            /* if there is a PID, check that it is still alive */
            if (lockpid > 0) {
                CHECKED(rc, kill( lockpid, 0 ));
                if (rc == 0 || errno == EPERM) {
                    stale = 0;
                } else if (rc < 0 && errno == ESRCH) {
                    stale = 1;
                }
            }
            if (stale == 2) {
                /* no pid, stale if the file is older than 1 minute */
                stale = (now >= st.st_mtime + 60);
            }

            if (stale) {
                D( "removing stale lockfile '%s'", lock->lock );
                CHECKED(rc, unlink( lock->lock ));
                _sleep = 0;
                tries++;
            }
        }
    }
    D("file '%s' is already in use by another process", lock->file );

Fail:
    if (f)
        fclose(f);

    if (temp_fd >= 0) {
        close(temp_fd);
    }

    if (lock_fd >= 0) {
        close(lock_fd);
    }

    unlink( lock->lock );
    unlink( lock->temp );
    return -1;
#endif
}

void
filelock_release( FileLock*  lock )
{
    if (lock->locked) {
#ifdef _WIN32
        path_delete_file( (char*)lock->temp );
        rmdir( (char*)lock->lock );
#else
        unlink( (char*)lock->lock );
#endif
        lock->locked = 0;
    }
}

static void
filelock_atexit( void )
{
  FileLock*  lock;

  for (lock = _all_filelocks; lock != NULL; lock = lock->next)
     filelock_release( lock );
}

/* create a file lock */
FileLock*
filelock_create( const char*  file )
{
    int    file_len = strlen(file);
    int    lock_len = file_len + sizeof(LOCK_NAME);
#ifdef _WIN32
    int    temp_len = lock_len + 1 + sizeof(PIDFILE_NAME);
#else
    int    temp_len = file_len + sizeof(TEMP_NAME);
#endif
    int    total_len = sizeof(FileLock) + file_len + lock_len + temp_len + 3;

    FileLock*  lock = malloc(total_len);

    lock->file = (const char*)(lock + 1);
    memcpy( (char*)lock->file, file, file_len+1 );

    lock->lock = lock->file + file_len + 1;
    memcpy( (char*)lock->lock, file, file_len+1 );
    strcat( (char*)lock->lock, LOCK_NAME );

    lock->temp    = (char*)lock->lock + lock_len + 1;
#ifdef _WIN32
    snprintf( (char*)lock->temp, temp_len, "%s\\" PIDFILE_NAME, lock->lock );
#else
    lock->temp[0] = 0;
#endif
    lock->locked = 0;

    if (filelock_lock(lock) < 0) {
        free(lock);
        return NULL;
    }

    lock->next     = _all_filelocks;
    _all_filelocks = lock;

    if (lock->next == NULL)
        atexit( filelock_atexit );

    return lock;
}

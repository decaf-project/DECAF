#include "fs_tools.h"
#include "hfs.h"

void
hfs_jopen(FS_INFO * fs, INUM_T inum)
{
    error("jopen not implemented for HFS yet");

    return;
}

void
hfs_jentry_walk(FS_INFO * fs, int flags, FS_JENTRY_WALK_FN action,
    void *ptr)
{
    error("jentry_walk not implemented for HFS yet");

    return;
}

void
hfs_jblk_walk(FS_INFO * fs, DADDR_T start, DADDR_T end, int flags,
    FS_JBLK_WALK_FN action, void *ptr)
{

    error("jblk_walk not implemented for HFS yet");

    return;
}

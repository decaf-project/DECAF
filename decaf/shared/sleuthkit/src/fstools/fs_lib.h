
#ifndef _FS_LIB_H
#define _FS_LIB_H

#include "fs_tools.h"

#define DCALC_DD	0x1
#define DCALC_DLS	0x2
#define DCALC_SLACK	0x4
extern uint8_t fs_dcalc(FS_INFO * fs, uint8_t lclflags, DADDR_T cnt);


#define DCAT_HEX		0x1
#define DCAT_ASCII   0x2
#define DCAT_HTML	0x4
#define DCAT_STAT	0x8
extern uint8_t fs_dcat(FS_INFO * fs, uint8_t lclflags, DADDR_T addr,
    DADDR_T read_num_units);

#define DLS_CAT     0x01
#define DLS_LIST    0x02
#define DLS_SLACK   0x04
extern uint8_t fs_dls(FS_INFO * fs, uint8_t lclflags, DADDR_T bstart,
    DADDR_T bend, int flags);

extern uint8_t fs_dstat(FS_INFO * fs, uint8_t lclflags, DADDR_T addr,
    int flags);

#define FFIND_ALL 0x1
extern uint8_t fs_ffind(FS_INFO * fs, uint8_t lclflags, INUM_T inode,
    uint32_t type, uint16_t id, int flags);



#define FLS_DOT		0x001
#define FLS_LONG	0x002
#define FLS_FILE	0x004
#define FLS_DIR		0x008
#define FLS_FULL	0x010
#define FLS_MAC		0x020

extern uint8_t fs_fls(FS_INFO * fs, uint8_t lclflags, INUM_T inode,
    int flags, char *pre, int32_t skew);


extern uint8_t fs_icat(FS_INFO * fs, uint8_t lclflags, INUM_T inum,
    uint32_t type, uint16_t id, int flags);


#define IFIND_ALL	0x01
#define IFIND_PATH	0x04
#define IFIND_DATA	0x08
#define IFIND_PAR       0x10
#define IFIND_PAR_LONG	0x20
extern uint8_t fs_ifind_path(FS_INFO * fs, uint8_t lclflags, char *path);
extern uint8_t fs_ifind_data(FS_INFO * fs, uint8_t lclflags, DADDR_T blk);
extern uint8_t fs_ifind_par(FS_INFO * fs, uint8_t lclflags, INUM_T par);

#define ILS_OPEN	0x1
#define ILS_REM	0x2
#define ILS_MAC	0x4
extern uint8_t fs_ils(FS_INFO * fs, uint8_t lclflags, INUM_T istart,
    INUM_T ilast, int flags, int32_t skew, char *img);

#endif

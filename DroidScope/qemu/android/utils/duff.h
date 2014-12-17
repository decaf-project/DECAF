/* Copyright (C) 2011 The Android Open Source Project
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
#ifndef ANDROID_UTILS_DUFF_H
#define ANDROID_UTILS_DUFF_H

/*********************************************************************
 *********************************************************************
 *****
 *****    DUFF'S DEVICES
 *****
 *****/

#define  DUFF1(_count,_stmnt) \
    do { \
        int __n = (_count); \
        do { \
            _stmnt; \
        } while (--__n > 0); \
    } while (0);

#define  DUFF2(_count,_stmnt)  \
    ({ \
        int   __count = (_count); \
        int   __n     = (__count +1)/2; \
        switch (__count & 1) { \
        case 0: do { _stmnt; \
        case 1:      _stmnt; \
                } while (--__n > 0); \
        } \
    })

#define  DUFF4(_count,_stmnt)  \
    ({ \
        int   __count = (_count); \
        int   __n     = (__count +3)/4; \
        switch (__count & 3) { \
        case 0: do { _stmnt; \
        case 3:      _stmnt; \
        case 2:      _stmnt; \
        case 1:      _stmnt; \
                } while (--__n > 0); \
        } \
    })

#define  DUFF8(_count,_stmnt)  \
    ({ \
        int   __count = (_count); \
        int   __n     = (__count+7)/8; \
        switch (__count & 7) { \
        case 0: do { _stmnt; \
        case 7:      _stmnt; \
        case 6:      _stmnt; \
        case 5:      _stmnt; \
        case 4:      _stmnt; \
        case 3:      _stmnt; \
        case 2:      _stmnt; \
        case 1:      _stmnt; \
                } while (--__n > 0); \
        } \
    })

#endif /* ANDROID_UTILS_DUFF_H */

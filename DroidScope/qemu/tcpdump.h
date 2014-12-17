/* Copyright (C) 2008 The Android Open Source Project
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
#ifndef _QEMU_TCPDUMP_H
#define _QEMU_TCPDUMP_H

#include <stdint.h>

/* global flag, set to 1 when packet captupe is active */
extern int  qemu_tcpdump_active;

/* start a new packet capture, close the current one if any.
 * returns 0 on success, and -1 on failure (see errno then) */
extern int  qemu_tcpdump_start( const char*  filepath );

/* stop the current packet capture, if any */
extern void qemu_tcpdump_stop( void );

/* send an ethernet packet to the packet capture file, if any */
extern void qemu_tcpdump_packet( const void*  base, int  len );

/* returns interesting stats, like the number of packets captures,
 * and the total size of these packets. Note: the file will be larger
 * due to global and packet headers.
 */
extern void  qemu_tcpdump_stats( uint64_t  *pcount, uint64_t*  psize );

#endif /* _QEMU_TCPDUMP_H */

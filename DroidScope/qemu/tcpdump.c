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
#include "tcpdump.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int  qemu_tcpdump_active;

static FILE*     capture_file;
static uint64_t  capture_count;
static uint64_t  capture_size;
static int       capture_init;

static void
capture_atexit(void)
{
    if (qemu_tcpdump_active) {
        fclose(capture_file);
        qemu_tcpdump_active = 0;
    }
}

/* See http://wiki.wireshark.org/Development/LibpcapFileFormat for
 * the complete description of the packet capture file format
 */

#define  PCAP_MAGIC     0xa1b2c3d4
#define  PCAP_MAJOR     2
#define  PCAP_MINOR     4
#define  PCAP_SNAPLEN   65535
#define  PCAP_ETHERNET  1

static int
pcap_write_header( FILE*  out )
{
    typedef struct {
        uint32_t   magic;
        uint16_t   version_major;
        uint16_t   version_minor;
        int32_t    this_zone;
        uint32_t   sigfigs;
        uint32_t   snaplen;
        uint32_t   network;
    } PcapHeader;

    PcapHeader  h;

    h.magic         = PCAP_MAGIC;
    h.version_major = PCAP_MAJOR;
    h.version_minor = PCAP_MINOR;
    h.this_zone     = 0;
    h.sigfigs       = 0;  /* all tools set it to 0 in practice */
    h.snaplen       = PCAP_SNAPLEN;
    h.network       = PCAP_ETHERNET;

    if (fwrite(&h, sizeof(h), 1, out) != 1) {
        return -1;
    }
    return 0;
}

int
qemu_tcpdump_start( const char*  filepath )
{
    if (!capture_init) {
        capture_init = 1;
        atexit(capture_atexit);
    }

    qemu_tcpdump_stop();

    if (filepath == NULL)
        return -1;

    capture_file = fopen(filepath, "wb");
    if (capture_file == NULL)
        return -1;

    if (pcap_write_header(capture_file) < 0)
        return -1;

    qemu_tcpdump_active = 1;
    return 0;
}

void
qemu_tcpdump_stop( void )
{
    if (!qemu_tcpdump_active)
        return;

    qemu_tcpdump_active = 0;

    capture_count = 0;
    capture_size  = 0;

    fclose(capture_file);
    capture_file = NULL;
}

void
qemu_tcpdump_packet( const void*  base, int  len )
{
    typedef struct {
        uint32_t  ts_sec;
        uint32_t  ts_usec;
        uint32_t  incl_len;
        uint32_t  orig_len;
    } PacketHeader;

    PacketHeader    h;
    struct timeval  now;
    int             len2 = len;

    if (len2 > PCAP_SNAPLEN)
        len2 = PCAP_SNAPLEN;

    gettimeofday(&now, NULL);
    h.ts_sec   = (uint32_t) now.tv_sec;
    h.ts_usec  = (uint32_t) now.tv_usec;
    h.incl_len = (uint32_t) len2;
    h.orig_len = (uint32_t) len;

    fwrite( &h, sizeof(h), 1, capture_file );
    fwrite( base, 1, len2, capture_file );

    capture_count += 1;
    capture_size  += len2;
}

void
qemu_tcpdump_stats( uint64_t  *pcount, uint64_t*  psize )
{
    *pcount = capture_count;
    *psize  = capture_size;
}


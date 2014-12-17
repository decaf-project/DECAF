/*
 * Copyright (c) 2009 The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _SLIRP_HELPER_H
#define _SLIRP_HELPER_H

#ifdef _WIN32
#  include <winsock2.h>  /* for ntohl */
#endif

typedef union {
    u_int32_t   addr;
    u_int8_t    data[4];
} ipaddr_t;

/* return ip address in network order */
static __inline__ uint32_t
ip_getn( ipaddr_t  ip )
{
    return ip.addr;
}

/* return ip address in host order */
static __inline__ uint32_t
ip_geth( ipaddr_t  ip )
{
    return ntohl(ip.addr);
}

/* set ip address in network order */
static __inline__ ipaddr_t
ip_setn( uint32_t  val )
{
    ipaddr_t  ip;
    ip.addr = val;
    return ip;
}

/* set ip address in host order */
static __inline__ ipaddr_t
ip_seth( uint32_t  val )
{
    ipaddr_t  ip;
    ip.addr = htonl(val);
    return ip;
}

static __inline__ ipaddr_t
ip_read( const void*  buf )
{
    ipaddr_t  ip;
    memcpy(ip.data, buf, 4);
    return ip;
}

static __inline__ void
ip_write( ipaddr_t  ip, void*  buf )
{
    memcpy(buf, ip.data, 4);
}

static __inline__ uint32_t
ip_read32h( const void* buf )
{
    ipaddr_t  addr = ip_read(buf);
    return ip_geth(addr);
}

static __inline__ void
ip_write32h( uint32_t  ip, void*  buf )
{
    ipaddr_t  addr = ip_seth(ip);
    ip_write(addr, buf);
}

static __inline__ int
ip_equal( ipaddr_t  ip1, ipaddr_t  ip2 )
{
    return ip1.addr == ip2.addr;
}

typedef union {
    u_int16_t  port;
    u_int8_t   data[2];
} port_t;

static __inline__ uint16_t
port_getn( port_t   p )
{
    return p.port;
}

static __inline__ uint16_t
port_geth( port_t  p )
{
    return ntohs(p.port);
}

static __inline__ port_t
port_setn( uint16_t  val )
{
    port_t  p;
    p.port = val;
    return p;
}

static __inline__ port_t
port_seth( uint16_t  val )
{
    port_t  p;
    p.port = htons(val);
    return p;
}

#endif /* _SLIRP_HELPER_H */

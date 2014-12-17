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
#include "proxy_int.h"
#include "sockets.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "android/utils/misc.h"
#include "android/utils/system.h"
#include "iolooper.h"
#include <stdlib.h>

int  proxy_log = 0;

void
proxy_LOG(const char*  fmt, ...)
{
    va_list  args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
proxy_set_verbose(int  mode)
{
    proxy_log = mode;
}

/** Global connection list
 **/

static ProxyConnection  s_connections[1];

#define  MAX_HEX_DUMP  512

static void
hex_dump( void*   base, int  size, const char*  prefix )
{
    STRALLOC_DEFINE(s);
    if (size > MAX_HEX_DUMP)
        size = MAX_HEX_DUMP;
    stralloc_add_hexdump(s, base, size, prefix);
    proxy_LOG( "%s", stralloc_cstr(s) );
    stralloc_reset(s);
}

void
proxy_connection_init( ProxyConnection*           conn,
                       int                        socket,
                       SockAddress*               address,
                       ProxyService*              service,
                       ProxyConnectionFreeFunc    conn_free,
                       ProxyConnectionSelectFunc  conn_select,
                       ProxyConnectionPollFunc    conn_poll )
{
    conn->socket    = socket;
    conn->address   = address[0];
    conn->service   = service;
    conn->next      = NULL;

    conn->conn_free   = conn_free;
    conn->conn_select = conn_select;
    conn->conn_poll   = conn_poll;

    socket_set_nonblock(socket);

    {
        SocketType  type = socket_get_type(socket);

        snprintf( conn->name, sizeof(conn->name),
                  "%s:%s(%d)",
                  (type == SOCKET_STREAM) ? "tcp" : "udp",
                  sock_address_to_string(address), socket );

        /* just in case */
        conn->name[sizeof(conn->name)-1] = 0;
    }

    stralloc_reset(conn->str);
    conn->str_pos = 0;
}

void
proxy_connection_done( ProxyConnection*  conn )
{
    stralloc_reset( conn->str );
    if (conn->socket >= 0) {
        socket_close(conn->socket);
        conn->socket = -1;
    }
}


void
proxy_connection_rewind( ProxyConnection*  conn )
{
    stralloc_t*  str = conn->str;

    /* only keep a small buffer in the heap */
    conn->str_pos = 0;
    str->n        = 0;
    if (str->a > 1024)
        stralloc_reset(str);
}

DataStatus
proxy_connection_send( ProxyConnection*  conn, int  fd )
{
    stralloc_t*  str    = conn->str;
    int          avail  = str->n - conn->str_pos;

    conn->str_sent = 0;

    if (avail <= 0)
        return 1;

    if (proxy_log) {
        PROXY_LOG("%s: sending %d bytes:", conn->name, avail );
        hex_dump( str->s + conn->str_pos, avail, ">> " );
    }

    while (avail > 0) {
        int  n = socket_send(fd, str->s + conn->str_pos, avail);
        if (n == 0) {
            PROXY_LOG("%s: connection reset by peer (send)",
                      conn->name);
            return DATA_ERROR;
        }
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return DATA_NEED_MORE;

            PROXY_LOG("%s: error: %s", conn->name, errno_str);
            return DATA_ERROR;
        }
        conn->str_pos  += n;
        conn->str_sent += n;
        avail          -= n;
    }

    proxy_connection_rewind(conn);
    return DATA_COMPLETED;
}


DataStatus
proxy_connection_receive( ProxyConnection*  conn, int  fd, int  wanted )
{
    stralloc_t*  str    = conn->str;

    conn->str_recv = 0;

    while (wanted > 0) {
        int  n;

        stralloc_readyplus( str, wanted );
        n = socket_recv(fd, str->s + str->n, wanted);
        if (n == 0) {
            PROXY_LOG("%s: connection reset by peer (receive)",
                      conn->name);
            return DATA_ERROR;
        }
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return DATA_NEED_MORE;

            PROXY_LOG("%s: error: %s", conn->name, errno_str);
            return DATA_ERROR;
        }

        if (proxy_log) {
            PROXY_LOG("%s: received %d bytes:", conn->name, n );
            hex_dump( str->s + str->n, n, "<< " );
        }

        str->n         += n;
        wanted         -= n;
        conn->str_recv += n;
    }
    return DATA_COMPLETED;
}


DataStatus
proxy_connection_receive_line( ProxyConnection*  conn, int  fd )
{
    stralloc_t*  str = conn->str;

    for (;;) {
        char  c;
        int   n = socket_recv(fd, &c, 1);
        if (n == 0) {
            PROXY_LOG("%s: disconnected from server", conn->name );
            return DATA_ERROR;
        }
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                PROXY_LOG("%s: blocked", conn->name);
                return DATA_NEED_MORE;
            }
            PROXY_LOG("%s: error: %s", conn->name, errno_str);
            return DATA_ERROR;
        }

        stralloc_add_c(str, c);
        if (c == '\n') {
            str->s[--str->n] = 0;
            if (str->n > 0 && str->s[str->n-1] == '\r')
                str->s[--str->n] = 0;

            PROXY_LOG("%s: received '%s'", conn->name,
                      quote_bytes(str->s, str->n));
            return DATA_COMPLETED;
        }
    }
}

static void
proxy_connection_insert( ProxyConnection*  conn, ProxyConnection*  after )
{
    conn->next        = after->next;
    after->next->prev = conn;
    after->next       = conn;
    conn->prev        = after;
}

static void
proxy_connection_remove( ProxyConnection*  conn )
{
    conn->prev->next = conn->next;
    conn->next->prev = conn->prev;

    conn->next = conn->prev = conn;
}

/** Global service list
 **/

#define  MAX_SERVICES  4

static  ProxyService*  s_services[ MAX_SERVICES ];
static  int            s_num_services;
static  int            s_init;

static void  proxy_manager_atexit( void );

static void
proxy_manager_init(void)
{
    s_init = 1;
    s_connections->next = s_connections;
    s_connections->prev = s_connections;
    atexit( proxy_manager_atexit );
}


extern int
proxy_manager_add_service( ProxyService*  service )
{
    if (!service || s_num_services >= MAX_SERVICES)
        return -1;

    if (!s_init)
        proxy_manager_init();

    s_services[s_num_services++] = service;
    return 0;
}


extern void
proxy_manager_atexit( void )
{
    ProxyConnection*  conn = s_connections->next;
    int               n;

    /* free all proxy connections */
    while (conn != s_connections) {
        ProxyConnection*  next = conn->next;
        conn->conn_free( conn );
        conn = next;
    }
    conn->next = conn;
    conn->prev = conn;

    /* free all proxy services */
    for (n = s_num_services; n-- > 0;) {
        ProxyService*  service = s_services[n];
        service->serv_free( service->opaque );
    }
    s_num_services = 0;
}


void
proxy_connection_free( ProxyConnection*  conn,
                       int               keep_alive,
                       ProxyEvent        event )
{
    if (conn) {
        int  fd = conn->socket;

        proxy_connection_remove(conn);

        if (event != PROXY_EVENT_NONE)
            conn->ev_func( conn->ev_opaque, fd, event );

        if (keep_alive)
            conn->socket = -1;

        conn->conn_free(conn);
    }
}


int
proxy_manager_add( SockAddress*    address,
                   SocketType      sock_type,
                   ProxyEventFunc  ev_func,
                   void*           ev_opaque )
{
    int  n;

    if (!s_init) {
        proxy_manager_init();
    }

    for (n = 0; n < s_num_services; n++) {
        ProxyService*     service = s_services[n];
        ProxyConnection*  conn    = service->serv_connect( service->opaque,
                                                           sock_type,
                                                           address );
        if (conn != NULL) {
            conn->ev_func   = ev_func;
            conn->ev_opaque = ev_opaque;
            proxy_connection_insert(conn, s_connections->prev);
            return 0;
        }
    }
    return -1;
}


/* remove an on-going proxified socket connection from the manager's list.
 * this is only necessary when the socket connection must be canceled before
 * the connection accept/refusal occured
 */
void
proxy_manager_del( void*  ev_opaque )
{
    ProxyConnection*  conn = s_connections->next;
    for ( ; conn != s_connections; conn = conn->next ) {
        if (conn->ev_opaque == ev_opaque) {
            proxy_connection_remove(conn);
            conn->conn_free(conn);
            return;
        }
    }
}

void
proxy_select_set( ProxySelect*  sel,
                  int           fd,
                  unsigned      flags )
{
    if (fd < 0 || !flags)
        return;

    if (*sel->pcount < fd+1)
        *sel->pcount = fd+1;

    if (flags & PROXY_SELECT_READ) {
        FD_SET( fd, sel->reads );
    } else {
        FD_CLR( fd, sel->reads );
    }
    if (flags & PROXY_SELECT_WRITE) {
        FD_SET( fd, sel->writes );
    } else {
        FD_CLR( fd, sel->writes );
    }
    if (flags & PROXY_SELECT_ERROR) {
        FD_SET( fd, sel->errors );
    } else {
        FD_CLR( fd, sel->errors );
    }
}

unsigned
proxy_select_poll( ProxySelect*  sel, int  fd )
{
    unsigned  flags = 0;

    if (fd >= 0) {
        if ( FD_ISSET(fd, sel->reads) )
            flags |= PROXY_SELECT_READ;
        if ( FD_ISSET(fd, sel->writes) )
            flags |= PROXY_SELECT_WRITE;
        if ( FD_ISSET(fd, sel->errors) )
            flags |= PROXY_SELECT_ERROR;
    }
    return flags;
}

/* this function is called to update the select file descriptor sets
 * with those of the proxified connection sockets that are currently managed */
void
proxy_manager_select_fill( int  *pcount, fd_set*  read_fds, fd_set*  write_fds, fd_set*  err_fds)
{
    ProxyConnection*  conn;
    ProxySelect       sel[1];

    if (!s_init)
        proxy_manager_init();

    sel->pcount = pcount;
    sel->reads  = read_fds;
    sel->writes = write_fds;
    sel->errors = err_fds;

    conn = s_connections->next;
    while (conn != s_connections) {
        ProxyConnection*  next = conn->next;
        conn->conn_select(conn, sel);
        conn = next;
    }
}

/* this function is called to act on proxified connection sockets when network events arrive */
void
proxy_manager_poll( fd_set*  read_fds, fd_set*  write_fds, fd_set*  err_fds )
{
    ProxyConnection*  conn = s_connections->next;
    ProxySelect       sel[1];

    sel->pcount = NULL;
    sel->reads  = read_fds;
    sel->writes = write_fds;
    sel->errors = err_fds;

    while (conn != s_connections) {
        ProxyConnection*  next  = conn->next;
        conn->conn_poll( conn, sel );
        conn = next;
    }
}


int
proxy_base64_encode( const char*  src, int  srclen,
                     char*        dst, int  dstlen )
{
    static const char cb64[64]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char*       srcend = src + srclen;
    int               result = 0;

    while (src+3 <= srcend && result+4 <= dstlen)
    {
        dst[result+0] = cb64[ src[0] >> 2 ];
        dst[result+1] = cb64[ ((src[0] & 3) << 4) | ((src[1] & 0xf0) >> 4) ];
        dst[result+2] = cb64[ ((src[1] & 0xf) << 2) | ((src[2] & 0xc0) >> 6) ];
        dst[result+3] = cb64[ src[2] & 0x3f ];
        src    += 3;
        result += 4;
    }

    if (src < srcend) {
        unsigned char  in[4];

        if (result+4 > dstlen)
            return -1;

        in[0] = src[0];
        in[1] = src+1 < srcend ? src[1] : 0;
        in[2] = src+2 < srcend ? src[2] : 0;

        dst[result+0] = cb64[ in[0] >> 2 ];
        dst[result+1] = cb64[ ((in[0] & 3) << 4) | ((in[1] & 0xf0) >> 4) ];
        dst[result+2] = (unsigned char) (src+1 < srcend ? cb64[ ((in[1] & 0xf) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
        dst[result+3] = (unsigned char) (src+2 < srcend ? cb64[ in[2] & 0x3f ] : '=');
        result += 4;
    }
    return result;
}

int
proxy_resolve_server( SockAddress*   addr,
                      const char*    servername,
                      int            servernamelen,
                      int            serverport )
{
    char  name0[64], *name = name0;
    int   result = -1;

    if (servernamelen < 0)
        servernamelen = strlen(servername);

    if (servernamelen >= sizeof(name0)) {
        AARRAY_NEW(name, servernamelen+1);
    }

    memcpy(name, servername, servernamelen);
    name[servernamelen] = 0;

    if (sock_address_init_resolve( addr, name, serverport, 0 ) < 0) {
        PROXY_LOG("%s: can't resolve proxy server name '%s'",
                  __FUNCTION__, name);
        goto Exit;
    }

    PROXY_LOG("server name '%s' resolved to %s", name, sock_address_to_string(addr));
    result = 0;

Exit:
    if (name != name0)
        AFREE(name);

    return result;
}


int
proxy_check_connection( const char* proxyname,
                        int         proxyname_len,
                        int         proxyport,
                        int         timeout_ms )
{
    SockAddress  addr;
    int          sock;
    IoLooper*    looper;
    int          ret;

    if (proxy_resolve_server(&addr, proxyname, proxyname_len, proxyport) < 0) {
        return -1;
    }

    sock = socket_create(addr.family, SOCKET_STREAM);
    if (sock < 0) {
        PROXY_LOG("%s: Could not create socket !?: %s", __FUNCTION__, errno_str);
        return -1;
    }

    socket_set_nonblock(sock);

    /* An immediate connection is very unlikely, but deal with it, just in case */
    if (socket_connect(sock, &addr) == 0) {
        PROXY_LOG("%s: Immediate connection to %.*s:%d: %s !",
                    __FUNCTION__, proxyname_len, proxyname, proxyport);
        socket_close(sock);
        return 0;
    }

    /* Ok, create an IoLooper object to wait for the connection */
    looper = iolooper_new();
    iolooper_add_write(looper, sock);

    ret = iolooper_wait(looper, timeout_ms);

    iolooper_free(looper);
    socket_close(sock);

    if (ret == 0) {
        errno = ETIMEDOUT;
        ret   = -1;
    }
    return ret;
}

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
#include "proxy_http_int.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "qemu-common.h"

/* A HttpConnector implements a non-HTTP proxied connection
 * through the CONNECT method. Many firewalls are configured
 * to reject these for port 80, so these connections should
 * use a HttpRewriter instead.
 */

typedef enum {
    STATE_NONE = 0,
    STATE_CONNECTING,           /* connecting to the server */
    STATE_SEND_HEADER,          /* connected, sending header to the server */
    STATE_RECEIVE_ANSWER_LINE1,
    STATE_RECEIVE_ANSWER_LINE2  /* connected, reading server's answer */
} ConnectorState;

typedef struct Connection {
    ProxyConnection  root[1];
    ConnectorState   state;
} Connection;


static void
connection_free( ProxyConnection*  root )
{
    proxy_connection_done(root);
    qemu_free(root);
}



#define  HTTP_VERSION  "1.1"

static int
connection_init( Connection*  conn )
{
    HttpService*      service = (HttpService*) conn->root->service;
    ProxyConnection*  root    = conn->root;
    stralloc_t*       str     = root->str;

    proxy_connection_rewind(root);
    stralloc_add_format(str, "CONNECT %s HTTP/" HTTP_VERSION "\r\n",
                        sock_address_to_string(&root->address));

    stralloc_add_bytes(str, service->footer, service->footer_len);

    if (!socket_connect( root->socket, &service->server_addr )) {
        /* immediate connection ?? */
        conn->state = STATE_SEND_HEADER;
        PROXY_LOG("%s: immediate connection", root->name);
    }
    else {
        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            conn->state = STATE_CONNECTING;
            PROXY_LOG("%s: connecting", root->name);
        }
        else {
            PROXY_LOG("%s: cannot connect to proxy: %s", root->name, errno_str);
            return -1;
        }
    }
    return 0;
}


static void
connection_select( ProxyConnection*   root,
                   ProxySelect*       sel )
{
    unsigned     flags;
    Connection*  conn = (Connection*)root;

    switch (conn->state) {
        case STATE_RECEIVE_ANSWER_LINE1:
        case STATE_RECEIVE_ANSWER_LINE2:
            flags = PROXY_SELECT_READ;
            break;

        case STATE_CONNECTING:
        case STATE_SEND_HEADER:
            flags = PROXY_SELECT_WRITE;
            break;

        default:
            flags = 0;
    };
    proxy_select_set(sel, root->socket, flags);
}

static void
connection_poll( ProxyConnection*   root,
                 ProxySelect*       sel )
{
    DataStatus   ret  = DATA_NEED_MORE;
    Connection*  conn = (Connection*)root;
    int          fd   = root->socket;

    if (!proxy_select_poll(sel, fd))
        return;

    switch (conn->state)
    {
        case STATE_CONNECTING:
            PROXY_LOG("%s: connected to http proxy, sending header", root->name);
            conn->state = STATE_SEND_HEADER;
            break;

        case STATE_SEND_HEADER:
            ret = proxy_connection_send(root, fd);
            if (ret == DATA_COMPLETED) {
                conn->state = STATE_RECEIVE_ANSWER_LINE1;
                PROXY_LOG("%s: header sent, receiving first answer line", root->name);
            }
            break;

        case STATE_RECEIVE_ANSWER_LINE1:
        case STATE_RECEIVE_ANSWER_LINE2:
            ret = proxy_connection_receive_line(root, root->socket);
            if (ret == DATA_COMPLETED) {
                if (conn->state == STATE_RECEIVE_ANSWER_LINE1) {
                    int  http1, http2, codenum;
                    const char*  line = root->str->s;

                    if ( sscanf(line, "HTTP/%d.%d %d", &http1, &http2, &codenum) != 3 ) {
                        PROXY_LOG( "%s: invalid answer from proxy: '%s'",
                                    root->name, line );
                        ret = DATA_ERROR;
                        break;
                    }

                    /* success is 2xx */
                    if (codenum/2 != 100) {
                        PROXY_LOG( "%s: connection refused, error=%d",
                                    root->name, codenum );
                        proxy_connection_free( root, 0, PROXY_EVENT_CONNECTION_REFUSED );
                        return;
                    }
                    PROXY_LOG("%s: receiving second answer line", root->name);
                    conn->state = STATE_RECEIVE_ANSWER_LINE2;
                    proxy_connection_rewind(root);
                } else {
                    /* ok, we're connected */
                    PROXY_LOG("%s: connection succeeded", root->name);
                    proxy_connection_free( root, 1, PROXY_EVENT_CONNECTED );
                }
            }
            break;

        default:
            PROXY_LOG("%s: invalid state for read event: %d", root->name, conn->state);
    }

    if (ret == DATA_ERROR) {
        proxy_connection_free( root, 0, PROXY_EVENT_SERVER_ERROR );
    }
}



ProxyConnection*
http_connector_connect( HttpService*  service,
                        SockAddress*  address )
{
    Connection*  conn;
    int          s;

    s = socket_create_inet( SOCKET_STREAM );
    if (s < 0)
        return NULL;

    conn = qemu_mallocz(sizeof(*conn));
    if (conn == NULL) {
        socket_close(s);
        return NULL;
    }

    proxy_connection_init( conn->root, s, address, service->root,
                           connection_free,
                           connection_select,
                           connection_poll );

    if ( connection_init( conn ) < 0 ) {
        connection_free( conn->root );
        return NULL;
    }

    return conn->root;
}

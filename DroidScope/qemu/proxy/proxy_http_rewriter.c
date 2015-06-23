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
#include <stdio.h>
#include <string.h>
#include "qemu-common.h"
#include "android/utils/system.h"  /* strsep */

/* this implements a transparent HTTP rewriting proxy
 *
 * this is needed because the HTTP spec mandates that
 * any query made to a proxy uses an absolute URI as
 * in:
 *
 *    GET http://www.example.com/index.html HTTP/1.1
 *
 * while the Android browser will think it's talking to
 * a normal web server and will issue a:
 *
 *    GET /index.html HTTP/1.1
 *    Host: www.example.com
 *
 * what we do here is thus the following:
 *
 * - read the request header
 * - rewrite the request's URI to use absolute URI
 * - send the rewritten header to the proxy
 * - then read the rest of the request, and tunnel it to the
 *   proxy as well
 * - read the answer as-is and send it back to the system
 *
 * this sounds all easy, but the rules for computing the
 * sizes of HTTP Message Bodies makes the implementation
 * a *bit* funky.
 */

/* define D_ACTIVE to 1 to dump additionnal debugging
 * info when -debug-proxy is used. These are only needed
 * when debugging the proxy code.
 */
#define  D_ACTIVE  1

#if D_ACTIVE
#  define  D(...)   PROXY_LOG(__VA_ARGS__)
#else
#  define  D(...)   ((void)0)
#endif


/** *************************************************************
 **
 **   HTTP HEADERS
 **
 **/

/* HttpHeader is a simple structure used to hold a (key,value)
 * pair in a linked list.
 */
typedef struct HttpHeader {
    struct HttpHeader*  next;
    const char*         key;
    const char*         value;
} HttpHeader;

static void
http_header_free( HttpHeader*  h )
{
    if (h) {
        qemu_free((char*)h->value);
        qemu_free(h);
    }
}

static int
http_header_append( HttpHeader*  h, const char*  value )
{
    int    old = strlen(h->value);
    int    new = strlen(value);
    char*  s   = realloc((char*)h->value, old+new+1);
    if (s == NULL)
        return -1;
    memcpy(s + old, value, new+1);
    h->value = (const char*)s;
    return 0;
}

static HttpHeader*
http_header_alloc( const char*  key, const char*  value )
{
    int          len = strlen(key)+1;
    HttpHeader*  h   = malloc(sizeof(*h) + len+1);
    if (h) {
        h->next  = NULL;
        h->key   = (const char*)(h+1);
        memcpy( (char*)h->key, key, len );
        h->value = qemu_strdup(value);
    }
    return h;
}

/** *************************************************************
 **
 **   HTTP HEADERS LIST
 **
 **/

typedef struct {
    HttpHeader*   first;
    HttpHeader*   last;
} HttpHeaderList;

static void
http_header_list_init( HttpHeaderList*  l )
{
    l->first = l->last = NULL;
}

static void
http_header_list_done( HttpHeaderList*  l )
{
    while (l->first) {
        HttpHeader*  h = l->first;
        l->first = h->next;
        http_header_free(h);
    }
    l->last = NULL;
}

static void
http_header_list_add( HttpHeaderList*  l,
                      HttpHeader*      h )
{
    if (!l->first) {
        l->first = h;
    } else {
        l->last->next = h;
    }
    h->next = NULL;
    l->last = h;
}

static const char*
http_header_list_find( HttpHeaderList*  l,
                       const char*      key )
{
    HttpHeader*  h;
    for (h = l->first; h; h = h->next)
        if (!strcasecmp(h->key, key))
            return h->value;

    return NULL;
}

/** *************************************************************
 **
 **   HTTP REQUEST AND REPLY
 **
 **/

typedef enum {
    HTTP_REQUEST_UNSUPPORTED = 0,
    HTTP_REQUEST_GET,
    HTTP_REQUEST_HEAD,
    HTTP_REQUEST_POST,
    HTTP_REQUEST_PUT,
    HTTP_REQUEST_DELETE,
} HttpRequestType;

/* HttpRequest is used both to store information about a specific
 * request and the corresponding reply
 */
typedef struct {
    HttpRequestType   req_type;     /* request type */
    char*             req_method;   /* "GET", "POST", "HEAD", etc... */
    char*             req_uri;      /* the request URI */
    char*             req_version;  /* "HTTP/1.0" or "HTTP/1.1" */
    char*             rep_version;  /* reply version string */
    int               rep_code;     /* reply code as decimal */
    char*             rep_readable; /* human-friendly reply/error message */
    HttpHeaderList    headers[1];   /* headers */
} HttpRequest;


static HttpRequest*
http_request_alloc( const char*      method,
                    const char*      uri,
                    const char*      version )
{
    HttpRequest*  r = malloc(sizeof(*r));

    r->req_method   = qemu_strdup(method);
    r->req_uri      = qemu_strdup(uri);
    r->req_version  = qemu_strdup(version);
    r->rep_version  = NULL;
    r->rep_code     = -1;
    r->rep_readable = NULL;

    if (!strcmp(method,"GET")) {
        r->req_type = HTTP_REQUEST_GET;
    } else if (!strcmp(method,"POST")) {
        r->req_type = HTTP_REQUEST_POST;
    } else if (!strcmp(method,"HEAD")) {
        r->req_type = HTTP_REQUEST_HEAD;
    } else if (!strcmp(method,"PUT")) {
        r->req_type = HTTP_REQUEST_PUT;
    } else if (!strcmp(method,"DELETE")) {
        r->req_type = HTTP_REQUEST_DELETE;
    } else
        r->req_type = HTTP_REQUEST_UNSUPPORTED;

    http_header_list_init(r->headers);
    return r;
}

static void
http_request_replace_uri( HttpRequest*  r,
                          const char*   uri )
{
    const char*  old = r->req_uri;
    r->req_uri = qemu_strdup(uri);
    qemu_free((char*)old);
}

static void
http_request_free( HttpRequest*  r )
{
    if (r) {
        http_header_list_done(r->headers);

        qemu_free(r->req_method);
        qemu_free(r->req_uri);
        qemu_free(r->req_version);
        qemu_free(r->rep_version);
        qemu_free(r->rep_readable);
        qemu_free(r);
    }
}

static char*
http_request_find_header( HttpRequest*  r,
                          const char*   key )
{
    return (char*)http_header_list_find(r->headers, key);
}


static int
http_request_add_header( HttpRequest*  r,
                         const char*   key,
                         const char*   value )
{
    HttpHeader*  h = http_header_alloc(key,value);
    if (h) {
        http_header_list_add(r->headers, h);
        return 0;
    }
    return -1;
}

static int
http_request_add_to_last_header( HttpRequest*  r,
                                 const char*   line )
{
    if (r->headers->last) {
        return http_header_append( r->headers->last, line );
    } else {
        return -1;
    }
}

static int
http_request_set_reply( HttpRequest*  r,
                        const char*   version,
                        const char*   code,
                        const char*   readable )
{
    if (strcmp(version,"HTTP/1.0") && strcmp(version,"HTTP/1.1")) {
        PROXY_LOG("%s: bad reply protocol: %s", __FUNCTION__, version);
        return -1;
    }
    r->rep_code = atoi(code);
    if (r->rep_code == 0) {
        PROXY_LOG("%s: bad reply code: %d", __FUNCTION__, code);
        return -1;
    }

    r->rep_version  = qemu_strdup(version);
    r->rep_readable = qemu_strdup(readable);

    /* reset the list of headers */
    http_header_list_done(r->headers);
    return 0;
}

/** *************************************************************
 **
 **   REWRITER CONNECTION
 **
 **/

typedef enum {
    STATE_CONNECTING = 0,
    STATE_CREATE_SOCKET_PAIR,
    STATE_REQUEST_FIRST_LINE,
    STATE_REQUEST_HEADERS,
    STATE_REQUEST_SEND,
    STATE_REQUEST_BODY,
    STATE_REPLY_FIRST_LINE,
    STATE_REPLY_HEADERS,
    STATE_REPLY_SEND,
    STATE_REPLY_BODY,
} ConnectionState;

/* root->socket is connected to the proxy server. while
 * slirp_fd is connected to the slirp code through a
 * socket_pair() we created for this specific purpose.
 */

typedef enum {
    BODY_NONE = 0,
    BODY_KNOWN_LENGTH,
    BODY_UNTIL_CLOSE,
    BODY_CHUNKED,
    BODY_MODE_MAX
} BodyMode;

static const char* const  body_mode_str[BODY_MODE_MAX] = {
    "NONE", "KNOWN_LENGTH", "UNTIL_CLOSE", "CHUNKED"
};

enum {
    CHUNK_HEADER,    // Waiting for a chunk header + CR LF
    CHUNK_DATA,      // Waiting for chunk data
    CHUNK_DATA_END,  // Waiting for the CR LF after the chunk data
    CHUNK_TRAILER    // Waiting for the chunk trailer + CR LF
};

typedef struct {
    ProxyConnection   root[1];
    int               slirp_fd;
    ConnectionState   state;
    HttpRequest*      request;
    BodyMode          body_mode;
    int64_t           body_length;
    int64_t           body_total;
    int64_t           body_sent;
    int64_t           chunk_length;
    int64_t           chunk_total;
    int               chunk_state;
    char              body_has_data;
    char              body_is_full;
    char              body_is_closed;
    char              parse_chunk_header;
    char              parse_chunk_trailer;
} RewriteConnection;


static void
rewrite_connection_free( ProxyConnection*  root )
{
    RewriteConnection*  conn = (RewriteConnection*)root;

    if (conn->slirp_fd >= 0) {
        socket_close(conn->slirp_fd);
        conn->slirp_fd = -1;
    }
    http_request_free(conn->request);
    proxy_connection_done(root);
    qemu_free(conn);
}


static int
rewrite_connection_init( RewriteConnection*   conn )
{
    HttpService*      service = (HttpService*) conn->root->service;
    ProxyConnection*  root    = conn->root;

    conn->slirp_fd = -1;
    conn->state    = STATE_CONNECTING;

    if (socket_connect( root->socket, &service->server_addr ) < 0) {
        if (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN) {
            PROXY_LOG("%s: connecting", conn->root->name);
        }
        else {
            PROXY_LOG("%s: cannot connect to proxy: %s", root->name, errno_str);
            return -1;
        }
    }
    else {
        PROXY_LOG("%s: immediate connection", root->name);
        conn->state = STATE_CREATE_SOCKET_PAIR;
    }
    return 0;
}

static int
rewrite_connection_create_sockets( RewriteConnection*  conn )
{
    /* immediate connection to the proxy. now create a socket
        * pair and send a 'success' event to slirp */
    int               slirp_1;
    ProxyConnection*  root = conn->root;

    if (socket_pair( &slirp_1, &conn->slirp_fd ) < 0) {
        PROXY_LOG("%s: coult not create socket pair: %s",
                    root->name, errno_str);
        return -1;
    }

    root->ev_func( root->ev_opaque, slirp_1, PROXY_EVENT_CONNECTED );
    conn->state = STATE_REQUEST_FIRST_LINE;
    return 0;
}


/* read the first line of a given HTTP request. returns -1/0/+1 */
static DataStatus
rewrite_connection_read_request( RewriteConnection*  conn )
{
    ProxyConnection*  root = conn->root;
    DataStatus        ret;

    ret = proxy_connection_receive_line(root, conn->slirp_fd);
    if (ret == DATA_COMPLETED) {
        /* now parse the first line to see if we can handle it */
        char*  line   = root->str->s;
        char*  method;
        char*  uri;
        char*  version;
        char*  p = line;

        method = strsep(&p, " ");
        if (p == NULL) {
            PROXY_LOG("%s: can't parse method in '%'", 
                      root->name, line);
            return DATA_ERROR;
        }
        uri = strsep(&p, " ");
        if (p == NULL) {
            PROXY_LOG( "%s: can't parse URI in '%s'",
                       root->name, line);
            return DATA_ERROR;
        }
        version = strsep(&p, " ");
        if (p != NULL) {
            PROXY_LOG( "%s: extra data after version in '%s'",
                       root->name, line);
            return DATA_ERROR;
        }
        if (conn->request)
            http_request_free(conn->request);

        conn->request = http_request_alloc( method, uri, version );
        if (!conn->request)
            return DATA_ERROR;

        proxy_connection_rewind(root);
    }
    return ret;
}


static DataStatus
rewrite_connection_read_reply( RewriteConnection*  conn )
{
    ProxyConnection*  root = conn->root;
    DataStatus        ret;

    ret = proxy_connection_receive_line( root, root->socket );
    if (ret == DATA_COMPLETED) {
        HttpRequest*  request = conn->request;

        char*  line = stralloc_cstr( root->str );
        char*  p = line;
        char*  protocol;
        char*  number;
        char*  readable;

        protocol = strsep(&p, " ");
        if (p == NULL) {
            PROXY_LOG("%s: can't parse response protocol: '%s'",
                      root->name, line);
            return DATA_ERROR;
        }
        number = strsep(&p, " ");
        if (p == NULL) {
            PROXY_LOG("%s: can't parse response number: '%s'",
                      root->name, line);
            return DATA_ERROR;
        }
        readable = p;

        if (http_request_set_reply(request, protocol, number, readable) < 0)
            return DATA_ERROR;

        proxy_connection_rewind(root);
    }
    return ret;
}


static DataStatus
rewrite_connection_read_headers( RewriteConnection*   conn,
                                 int                  fd )
{
    int               ret;
    ProxyConnection*  root = conn->root;

    for (;;) {
        char*        line;
        stralloc_t*  str = root->str;

        ret = proxy_connection_receive_line(root, fd);
        if (ret != DATA_COMPLETED)
            break;

        str->n = 0;
        line   = str->s;

        if (line[0] == 0) {
            /* an empty line means the end of headers */
            ret = 1;
            break;
        }

        /* it this a continuation ? */
        if (line[0] == ' ' || line[0] == '\t') {
            ret = http_request_add_to_last_header( conn->request, line );
        }
        else {
            char*  key;
            char*  value;

            value = line;
            key   = strsep(&value, ":");
            if (value == NULL) {
                PROXY_LOG("%s: can't parse header '%s'", root->name, line);
                ret = -1;
                break;
            }
            value += strspn(value, " ");
            if (http_request_add_header(conn->request, key, value) < 0)
                ret = -1;
        }
        if (ret == DATA_ERROR)
            break;
    }
    return ret;
}

static int
rewrite_connection_rewrite_request( RewriteConnection*  conn )
{
    ProxyConnection* root    = conn->root;
    HttpService*     service = (HttpService*) root->service;
    HttpRequest*     r       = conn->request;
    stralloc_t*      str     = root->str;
    HttpHeader*      h;

    proxy_connection_rewind(conn->root);

    /* only rewrite the URI if it is not absolute */
    if (r->req_uri[0] == '/') {
        char*  host = http_request_find_header(r, "Host");
        if (host == NULL) {
            PROXY_LOG("%s: uh oh, not Host: in request ?", root->name);
        } else {
            /* now create new URI */
            stralloc_add_str(str, "http://");
            stralloc_add_str(str, host);
            stralloc_add_str(str, r->req_uri);
            http_request_replace_uri(r, stralloc_cstr(str));
            proxy_connection_rewind(root);
        }
    }

    stralloc_format( str, "%s %s %s\r\n", r->req_method, r->req_uri, r->req_version );
    for (h = r->headers->first; h; h = h->next) {
        stralloc_add_format( str, "%s: %s\r\n", h->key, h->value );
    }
    /* add the service's footer - includes final \r\n */
    stralloc_add_bytes( str, service->footer, service->footer_len );

    return 0;
}

static int
rewrite_connection_rewrite_reply( RewriteConnection*  conn )
{
    HttpRequest*     r    = conn->request;
    ProxyConnection* root = conn->root;
    stralloc_t*      str  = root->str;
    HttpHeader*      h;

    proxy_connection_rewind(root);
    stralloc_format(str, "%s %d %s\r\n", r->rep_version, r->rep_code, r->rep_readable);
    for (h = r->headers->first; h; h = h->next) {
        stralloc_add_format(str, "%s: %s\r\n", h->key, h->value);
    }
    stralloc_add_str(str, "\r\n");

    return 0;
}


static int
rewrite_connection_get_body_length( RewriteConnection*  conn,
                                    int                 is_request )
{
    HttpRequest*      r    = conn->request;
    ProxyConnection*  root = conn->root;
    char*             content_length;
    char*             transfer_encoding;

    conn->body_mode      = BODY_NONE;
    conn->body_length    = 0;
    conn->body_total     = 0;
    conn->body_sent      = 0;
    conn->body_is_closed = 0;
    conn->body_is_full   = 0;
    conn->body_has_data  = 0;

    proxy_connection_rewind(root);

    if (is_request) {
        /* only POST and PUT should have a body */
        if (r->req_type != HTTP_REQUEST_POST &&
            r->req_type != HTTP_REQUEST_PUT)
        {
            return 0;
        }
    } else {
        /* HTTP 1.1 Section 4.3 Message Body states that HEAD requests must not have
        * a message body, as well as any 1xx, 204 and 304 replies */
        if (r->req_type == HTTP_REQUEST_HEAD || r->rep_code/100 == 1 ||
            r->rep_code == 204 || r->rep_code == 304)
            return 0;
    }

    content_length = http_request_find_header(r, "Content-Length");
    if (content_length != NULL) {
        char*    end;
        int64_t  body_len = strtoll( content_length, &end, 10 );
        if (*end != '\0' || *content_length == '\0' || body_len < 0) {
            PROXY_LOG("%s: bad content length: %s", root->name, content_length);
            return DATA_ERROR;
        }
        if (body_len > 0) {
            conn->body_mode   = BODY_KNOWN_LENGTH;
            conn->body_length = body_len;
        }
    } else {
        transfer_encoding = http_request_find_header(r, "Transfer-Encoding");
        if (transfer_encoding && !strcasecmp(transfer_encoding, "Chunked")) {
            conn->body_mode           = BODY_CHUNKED;
            conn->parse_chunk_header  = 0;
            conn->parse_chunk_trailer = 0;
            conn->chunk_length        = -1;
            conn->chunk_total         = 0;
            conn->chunk_state         = CHUNK_HEADER;
        }
    }
    if (conn->body_mode == BODY_NONE) {
        char*  connection = http_request_find_header(r, "Proxy-Connection");

        if (!connection)
            connection = http_request_find_header(r, "Connection");

        if (!connection || strcasecmp(connection, "Close")) {
            /* hum, we can't support this at all */
            PROXY_LOG("%s: can't determine content length, and client wants"
                        " to keep connection opened",
                        root->name);
            return -1;
        }
        /* a negative value means that the data ends when the client
         * disconnects the connection.
         */
        conn->body_mode = BODY_UNTIL_CLOSE;
    }
    D("%s: body_length=%lld body_mode=%s",
      root->name, conn->body_length, 
      body_mode_str[conn->body_mode]);

    proxy_connection_rewind(root);
    return 0;
}

#define  MAX_BODY_BUFFER  65536

static DataStatus
rewrite_connection_read_body( RewriteConnection*  conn, int  fd )
{
    ProxyConnection*  root   = conn->root;
    stralloc_t*       str    = root->str;
    int               wanted = 0, current, avail;
    DataStatus        ret;

    if (conn->body_is_closed) {
        return DATA_NEED_MORE;
    }

    /* first, determine how many bytes we want to read. */
    switch (conn->body_mode) {
    case BODY_NONE:
        D("%s: INTERNAL ERROR: SHOULDN'T BE THERE", root->name);
        return DATA_COMPLETED;

    case BODY_KNOWN_LENGTH:
        {
            if (conn->body_length == 0)
                return DATA_COMPLETED;

            if (conn->body_length > MAX_BODY_BUFFER)
                wanted = MAX_BODY_BUFFER;
            else
                wanted = (int)conn->body_length;
        }
        break;

    case BODY_UNTIL_CLOSE:
        wanted = MAX_BODY_BUFFER;
        break;

    case BODY_CHUNKED:
        if (conn->chunk_state == CHUNK_DATA_END) {
            /* We're waiting for the CR LF after the chunk data */
            ret = proxy_connection_receive_line(root, fd);
            if (ret != DATA_COMPLETED)
                return ret;

            if (str->s[0] != 0) { /* this should be an empty line */
                PROXY_LOG("%s: invalid chunk data end: '%s'",
                            root->name, str->s);
                return DATA_ERROR;
            }
            /* proxy_connection_receive_line() did remove the
            * trailing \r\n, but we must preserve it when we
            * send the chunk size end to the proxy.
            */
            stralloc_add_str(root->str, "\r\n");
            conn->chunk_state = CHUNK_HEADER;
            /* fall-through */
        }

        if (conn->chunk_state == CHUNK_HEADER) {
            char*      line;
            char*      end;
            long long  length;
            /* Ensure that the previous chunk was flushed before
             * accepting a new header */
            if (!conn->parse_chunk_header) {
                if (conn->body_has_data)
                    return DATA_NEED_MORE;
                D("%s: waiting chunk header", root->name);
                conn->parse_chunk_header = 1;
            }
            ret = proxy_connection_receive_line(root, fd);
            if (ret != DATA_COMPLETED) {
                return ret;
            }
            conn->parse_chunk_header = 0;

            line   = str->s;
            length = strtoll(line, &end, 16);
            if (line[0] == ' ' || (end[0] != '\0' && end[0] != ';')) {
                PROXY_LOG("%s: invalid chunk header: %s",
                        root->name, line);
                return DATA_ERROR;
            }
            if (length < 0) {
                PROXY_LOG("%s: invalid chunk length %lld",
                        root->name, length);
                return DATA_ERROR;
            }
            /* proxy_connection_receive_line() did remove the
            * trailing \r\n, but we must preserve it when we
            * send the chunk size to the proxy.
            */
            stralloc_add_str(root->str, "\r\n");

            conn->chunk_length = length;
            conn->chunk_total  = 0;
            conn->chunk_state  = CHUNK_DATA;
            if (length == 0) {
                /* the last chunk, no we need to add the trailer */
                conn->chunk_state         = CHUNK_TRAILER;
                conn->parse_chunk_trailer = 0;
            }
        }

        if (conn->chunk_state == CHUNK_TRAILER) {
            /* ensure that 'str' is flushed before reading the trailer */
            if (!conn->parse_chunk_trailer) {
                if (conn->body_has_data)
                    return DATA_NEED_MORE;
                conn->parse_chunk_trailer = 1;
            }
            ret = rewrite_connection_read_headers(conn, fd);
            if (ret == DATA_COMPLETED) {
                conn->body_is_closed = 1;
            }
            return ret;
        }

        /* if we get here, body_length > 0 */
        if (conn->chunk_length > MAX_BODY_BUFFER)
            wanted = MAX_BODY_BUFFER;
        else
            wanted = (int)conn->chunk_length;
        break;

    default:
        ;
    }

    /* we don't want more than MAX_BODY_BUFFER bytes in the
     * buffer we used to pass the body */
    current = str->n;
    avail   = MAX_BODY_BUFFER - current;
    if (avail <= 0) {
        /* wait for some flush */
        conn->body_is_full = 1;
        D("%s: waiting to flush %d bytes", 
          root->name, current);
        return DATA_NEED_MORE;
    }

    if (wanted > avail)
        wanted = avail;

    ret = proxy_connection_receive(root, fd, wanted);
    conn->body_has_data = (str->n > 0);
    conn->body_is_full  = (str->n == MAX_BODY_BUFFER);

    if (ret == DATA_ERROR) {
        if (conn->body_mode == BODY_UNTIL_CLOSE) {
            /* a disconnection here is normal and signals the
             * end of the body */
            conn->body_total    += root->str_recv;
            D("%s: body completed by close (%lld bytes)", 
                root->name, conn->body_total);
            conn->body_is_closed = 1;
            ret = DATA_COMPLETED;
        }
    } else {
        avail = root->str_recv;
        ret   = DATA_NEED_MORE;  /* we're not really done yet */

        switch (conn->body_mode) {
        case BODY_CHUNKED:
            conn->chunk_total  += avail;
            conn->chunk_length -= avail;

            if (conn->chunk_length == 0) {
                D("%s: chunk completed (%lld bytes)", 
                    root->name, conn->chunk_total);
                conn->body_total  += conn->chunk_total;
                conn->chunk_total  = 0;
                conn->chunk_length = -1;
                conn->chunk_state  = CHUNK_DATA;
            }
            break;

        case BODY_KNOWN_LENGTH:
            conn->body_length -= avail;
            conn->body_total  += avail;

            if (conn->body_length == 0) {
                D("%s: body completed (%lld bytes)", 
                    root->name, conn->body_total);
                conn->body_is_closed = 1;
                ret = DATA_COMPLETED;
            }
            break;

        case BODY_UNTIL_CLOSE:
            conn->body_total += avail;
            break;

        default:
            ;
        }
    }
    return ret;
}

static DataStatus
rewrite_connection_send_body( RewriteConnection*  conn, int  fd )
{
    ProxyConnection*  root   = conn->root;
    stralloc_t*       str    = root->str;
    DataStatus        ret    = DATA_NEED_MORE;

    if (conn->body_has_data) {
        ret = proxy_connection_send(root, fd);
        if (ret != DATA_ERROR) {
            int  pos = root->str_pos;

            memmove(str->s, str->s+pos, str->n-pos);
            str->n         -= pos;
            root->str_pos   = 0;
            conn->body_is_full  = (str->n == MAX_BODY_BUFFER);
            conn->body_has_data = (str->n > 0);
            conn->body_sent    += root->str_sent;

            /* ensure that we return DATA_COMPLETED only when
            * we have sent everything, and there is no more
            * body pieces to read */
            if (ret == DATA_COMPLETED) {
                if (!conn->body_is_closed || conn->body_has_data)
                    ret = DATA_NEED_MORE;
                else {
                    D("%s: sent all body (%lld bytes)",
                        root->name, conn->body_sent);
                }
            }
            D("%s: sent closed=%d data=%d n=%d ret=%d",
                root->name, conn->body_is_closed,
                conn->body_has_data, str->n,
                ret);
        }
    }
    return ret;
}


static void
rewrite_connection_select( ProxyConnection*  root,
                           ProxySelect*      sel )
{
    RewriteConnection*  conn = (RewriteConnection*)root;
    int  slirp = conn->slirp_fd;
    int  proxy = root->socket;

    switch (conn->state) {
        case STATE_CONNECTING:
        case STATE_CREATE_SOCKET_PAIR:
            /* try to connect to the proxy server */
            proxy_select_set( sel, proxy, PROXY_SELECT_WRITE );
            break;

        case STATE_REQUEST_FIRST_LINE:
        case STATE_REQUEST_HEADERS:
            proxy_select_set( sel, slirp, PROXY_SELECT_READ );
            break;

        case STATE_REQUEST_SEND:
            proxy_select_set( sel, proxy, PROXY_SELECT_WRITE );
            break;

        case STATE_REQUEST_BODY:
            if (!conn->body_is_closed && !conn->body_is_full)
                proxy_select_set( sel, slirp, PROXY_SELECT_READ );

            if (conn->body_has_data)
                proxy_select_set( sel, proxy, PROXY_SELECT_WRITE );
            break;

        case STATE_REPLY_FIRST_LINE:
        case STATE_REPLY_HEADERS:
            proxy_select_set( sel, proxy, PROXY_SELECT_READ );
            break;

        case STATE_REPLY_SEND:
            proxy_select_set( sel, slirp, PROXY_SELECT_WRITE );
            break;

        case STATE_REPLY_BODY:
            if (conn->body_has_data)
                proxy_select_set( sel, slirp, PROXY_SELECT_WRITE );

            if (!conn->body_is_closed && !conn->body_is_full)
                proxy_select_set( sel, proxy, PROXY_SELECT_READ );
            break;
        default:
            ;
    };
}

static void
rewrite_connection_poll( ProxyConnection*  root,
                         ProxySelect*      sel )
{
    RewriteConnection*  conn = (RewriteConnection*)root;

    int         slirp     = conn->slirp_fd;
    int         proxy     = root->socket;
    int         has_slirp = proxy_select_poll(sel, slirp);
    int         has_proxy = proxy_select_poll(sel, proxy);
    DataStatus  ret       = DATA_NEED_MORE;

    switch (conn->state) {
        case STATE_CONNECTING:
            if (has_proxy) {
                PROXY_LOG("%s: connected to proxy", root->name);
                conn->state = STATE_CREATE_SOCKET_PAIR;
            }
            break;

        case STATE_CREATE_SOCKET_PAIR:
            if (has_proxy) {
                if (rewrite_connection_create_sockets(conn) < 0) {
                    ret = DATA_ERROR;
                } else {
                    D("%s: socket pair created", root->name);
                    conn->state = STATE_REQUEST_FIRST_LINE;
                }
            }
            break;

        case STATE_REQUEST_FIRST_LINE:
            if (has_slirp) {
                ret = rewrite_connection_read_request(conn);
                if (ret == DATA_COMPLETED) {
                    PROXY_LOG("%s: request first line ok", root->name);
                    conn->state = STATE_REQUEST_HEADERS;
                }
            }
            break;

        case STATE_REQUEST_HEADERS:
            if (has_slirp) {
                ret = rewrite_connection_read_headers(conn, slirp);
                if (ret == DATA_COMPLETED) {
                    PROXY_LOG("%s: request headers ok", root->name);
                    if (rewrite_connection_rewrite_request(conn) < 0)
                        ret = DATA_ERROR;
                    else
                        conn->state = STATE_REQUEST_SEND;
                }
            }
            break;

        case STATE_REQUEST_SEND:
            if (has_proxy) {
                ret = proxy_connection_send(root, proxy);
                if (ret == DATA_COMPLETED) {
                    if (rewrite_connection_get_body_length(conn, 1) < 0) {
                        ret = DATA_ERROR;
                    } else if (conn->body_mode != BODY_NONE) {
                        PROXY_LOG("%s: request sent, waiting for body",
                                   root->name);
                        conn->state = STATE_REQUEST_BODY;
                    } else {
                        PROXY_LOG("%s: request sent, waiting for reply",
                                  root->name);
                        conn->state = STATE_REPLY_FIRST_LINE;
                    }
                }
            }
            break;

        case STATE_REQUEST_BODY:
            if (has_slirp) {
                ret = rewrite_connection_read_body(conn, slirp);
            }
            if (ret != DATA_ERROR && has_proxy) {
                ret = rewrite_connection_send_body(conn, proxy);
                if (ret == DATA_COMPLETED) {
                    PROXY_LOG("%s: request body ok, waiting for reply",
                              root->name);
                    conn->state = STATE_REPLY_FIRST_LINE;
                }
            }
            break;

        case STATE_REPLY_FIRST_LINE:
            if (has_proxy) {
                ret = rewrite_connection_read_reply(conn);
                if (ret == DATA_COMPLETED) {
                    PROXY_LOG("%s: reply first line ok", root->name);
                    conn->state = STATE_REPLY_HEADERS;
                }
            }
            break;

        case STATE_REPLY_HEADERS:
            if (has_proxy) {
                ret = rewrite_connection_read_headers(conn, proxy);
                if (ret == DATA_COMPLETED) {
                    PROXY_LOG("%s: reply headers ok", root->name);
                    if (rewrite_connection_rewrite_reply(conn) < 0)
                        ret = DATA_ERROR;
                    else
                        conn->state = STATE_REPLY_SEND;
                }
            }
            break;

        case STATE_REPLY_SEND:
            if (has_slirp) {
                ret = proxy_connection_send(conn->root, slirp);
                if (ret == DATA_COMPLETED) {
                    if (rewrite_connection_get_body_length(conn, 0) < 0) {
                        ret = DATA_ERROR;
                    } else if (conn->body_mode != BODY_NONE) {
                        PROXY_LOG("%s: reply sent, waiting for body",
                                  root->name);
                        conn->state = STATE_REPLY_BODY;
                    } else {
                        PROXY_LOG("%s: reply sent, looping to waiting request",
                                  root->name);
                        conn->state = STATE_REQUEST_FIRST_LINE;
                    }
                }
            }
            break;

        case STATE_REPLY_BODY:
            if (has_proxy) {
                ret = rewrite_connection_read_body(conn, proxy);
            }
            if (ret != DATA_ERROR && has_slirp) {
                ret = rewrite_connection_send_body(conn, slirp);
                if (ret == DATA_COMPLETED) {
                    if (conn->body_mode == BODY_UNTIL_CLOSE) {
                        PROXY_LOG("%s: closing connection", root->name);
                        ret = DATA_ERROR;
                    } else {
                        PROXY_LOG("%s: reply body ok, looping to waiting request",
                                root->name);
                        conn->state = STATE_REQUEST_FIRST_LINE;
                    }
                }
            }
            break;

        default:
            ;
    }
    if (ret == DATA_ERROR)
        proxy_connection_free(root, 0, PROXY_EVENT_NONE);

    return;
}


ProxyConnection*
http_rewriter_connect( HttpService*  service,
                       SockAddress*  address )
{
    RewriteConnection*  conn;
    int                 s;

    s = socket_create(address->family, SOCKET_STREAM );
    if (s < 0)
        return NULL;

    conn = qemu_mallocz(sizeof(*conn));
    if (conn == NULL) {
        socket_close(s);
        return NULL;
    }

    proxy_connection_init( conn->root, s, address, service->root,
                           rewrite_connection_free,
                           rewrite_connection_select,
                           rewrite_connection_poll );

    if ( rewrite_connection_init( conn ) < 0 ) {
        rewrite_connection_free( conn->root );
        return NULL;
    }

    return conn->root;
}

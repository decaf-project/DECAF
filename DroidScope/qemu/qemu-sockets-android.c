/*
 *  inet and unix socket functions for qemu
 *
 *  (c) 2008 Gerd Hoffmann <kraxel@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "qemu_socket.h"
#include "qemu-common.h" /* for qemu_isdigit */

#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif

#ifndef INET6_ADDRSTRLEN
# define INET6_ADDRSTRLEN  46
#endif

static int sockets_debug = 0;
static const int on=1, off=0;

/* used temporarely until all users are converted to QemuOpts */
static QemuOptsList dummy_opts = {
    .name = "dummy",
    .head = QTAILQ_HEAD_INITIALIZER(dummy_opts.head),
    .desc = {
        {
            .name = "path",
            .type = QEMU_OPT_STRING,
        },{
            .name = "host",
            .type = QEMU_OPT_STRING,
        },{
            .name = "port",
            .type = QEMU_OPT_STRING,
        },{
            .name = "to",
            .type = QEMU_OPT_NUMBER,
        },{
            .name = "ipv4",
            .type = QEMU_OPT_BOOL,
        },{
            .name = "ipv6",
            .type = QEMU_OPT_BOOL,
#ifdef CONFIG_ANDROID
        },{
            .name = "socket",
            .type = QEMU_OPT_NUMBER,
#endif
        },
        { /* end if list */ }
    },
};


static const char *sock_address_strfamily(SockAddress *s)
{
    switch (sock_address_get_family(s)) {
    case SOCKET_IN6:   return "ipv6";
    case SOCKET_INET:  return "ipv4";
    case SOCKET_UNIX:  return "unix";
    default:           return "????";
    }
}

int inet_listen_opts(QemuOpts *opts, int port_offset)
{
    SockAddress**  list;
    SockAddress*   e;
    unsigned       flags = SOCKET_LIST_PASSIVE;
    const char *addr;
    char port[33];
    char uaddr[256+1];
    char uport[33];
    int slisten,to,try_next,nn;

#ifdef CONFIG_ANDROID
    const char* socket_fd = qemu_opt_get(opts, "socket");
    if (socket_fd) {
        return atoi(socket_fd);
    }
#endif

    if ((qemu_opt_get(opts, "host") == NULL) ||
        (qemu_opt_get(opts, "port") == NULL)) {
        fprintf(stderr, "%s: host and/or port not specified\n", __FUNCTION__);
        return -1;
    }
    pstrcpy(port, sizeof(port), qemu_opt_get(opts, "port"));
    addr = qemu_opt_get(opts, "host");

    to = qemu_opt_get_number(opts, "to", 0);
    if (qemu_opt_get_bool(opts, "ipv4", 0))
        flags |= SOCKET_LIST_FORCE_INET;
    if (qemu_opt_get_bool(opts, "ipv6", 0))
        flags |= SOCKET_LIST_FORCE_IN6;

    /* lookup */
    if (port_offset)
        snprintf(port, sizeof(port), "%d", atoi(port) + port_offset);

    list = sock_address_list_create( strlen(addr) ? addr : NULL,
                                       port,
                                       flags );
    if (list == NULL) {
        fprintf(stderr,"%s: getaddrinfo(%s,%s): %s\n", __FUNCTION__,
                addr, port, errno_str);
        return -1;
    }

    /* create socket + bind */
    for (nn = 0; list[nn] != NULL; nn++) {
        SocketFamily  family;

        e      = list[nn];
        family = sock_address_get_family(e);

        sock_address_get_numeric_info(e, uaddr, sizeof uaddr, uport, sizeof uport);
        slisten = socket_create(family, SOCKET_STREAM);
        if (slisten < 0) {
            fprintf(stderr,"%s: socket(%s): %s\n", __FUNCTION__,
                    sock_address_strfamily(e), errno_str);
            continue;
        }

        socket_set_xreuseaddr(slisten);
#ifdef IPV6_V6ONLY
        /* listen on both ipv4 and ipv6 */
        if (family == SOCKET_IN6) {
            socket_set_ipv6only(slisten);
        }
#endif

        for (;;) {
            if (socket_bind(slisten, e) == 0) {
                if (sockets_debug)
                    fprintf(stderr,"%s: bind(%s,%s,%d): OK\n", __FUNCTION__,
                        sock_address_strfamily(e), uaddr, sock_address_get_port(e));
                goto listen;
            }
            socket_close(slisten);
            try_next = to && (sock_address_get_port(e) <= to + port_offset);
            if (!try_next || sockets_debug)
                fprintf(stderr,"%s: bind(%s,%s,%d): %s\n", __FUNCTION__,
                        sock_address_strfamily(e), uaddr, sock_address_get_port(e),
                        strerror(errno));
            if (try_next) {
                sock_address_set_port(e, sock_address_get_port(e) + 1);
                continue;
            }
            break;
        }
    }
    sock_address_list_free(list);
    fprintf(stderr, "%s: FAILED\n", __FUNCTION__);
    return -1;

listen:
    if (socket_listen(slisten,1) != 0) {
        perror("listen");
        socket_close(slisten);
        return -1;
    }
    snprintf(uport, sizeof(uport), "%d", sock_address_get_port(e) - port_offset);
    qemu_opt_set(opts, "host", uaddr);
    qemu_opt_set(opts, "port", uport);
    qemu_opt_set(opts, "ipv6", (e->family == SOCKET_IN6) ? "on" : "off");
    qemu_opt_set(opts, "ipv4", (e->family != SOCKET_IN6) ? "on" : "off");
    sock_address_list_free(list);
    return slisten;
}

int inet_connect_opts(QemuOpts *opts)
{
    SockAddress**  list;
    SockAddress*   e;
    unsigned       flags = 0;
    const char *addr;
    const char *port;
    int sock, nn;

#ifdef CONFIG_ANDROID
    const char* socket_fd = qemu_opt_get(opts, "socket");
    if (socket_fd) {
        return atoi(socket_fd);
    }
#endif

    addr = qemu_opt_get(opts, "host");
    port = qemu_opt_get(opts, "port");
    if (addr == NULL || port == NULL) {
        fprintf(stderr, "inet_connect: host and/or port not specified\n");
        return -1;
    }

    if (qemu_opt_get_bool(opts, "ipv4", 0)) {
        flags &= SOCKET_LIST_FORCE_IN6;
        flags |= SOCKET_LIST_FORCE_INET;
    }
    if (qemu_opt_get_bool(opts, "ipv6", 0)) {
        flags &= SOCKET_LIST_FORCE_INET;
        flags |= SOCKET_LIST_FORCE_IN6;
    }

    /* lookup */
    list = sock_address_list_create(addr, port, flags);
    if (list == NULL) {
        fprintf(stderr,"getaddrinfo(%s,%s): %s\n",
                addr, port, errno_str);
        return -1;
    }

    for (nn = 0; list[nn] != NULL; nn++) {
        e     = list[nn];
        sock = socket_create(sock_address_get_family(e), SOCKET_STREAM);
        if (sock < 0) {
            fprintf(stderr,"%s: socket(%s): %s\n", __FUNCTION__,
            sock_address_strfamily(e), errno_str);
            continue;
        }
        socket_set_xreuseaddr(sock);

        /* connect to peer */
        if (socket_connect(sock,e) < 0) {
            if (sockets_debug)
                fprintf(stderr, "%s: connect(%s,%s,%s,%s): %s\n", __FUNCTION__,
                        sock_address_strfamily(e),
                        sock_address_to_string(e), addr, port, strerror(errno));
            socket_close(sock);
            continue;
        }
        if (sockets_debug)
            fprintf(stderr, "%s: connect(%s,%s,%s,%s): OK\n", __FUNCTION__,
                        sock_address_strfamily(e),
                        sock_address_to_string(e), addr, port);

        goto EXIT;
    }
    sock = -1;
EXIT:
    sock_address_list_free(list);
    return sock;
}

int inet_dgram_opts(QemuOpts *opts)
{
    SockAddress**  peer_list = NULL;
    SockAddress**  local_list = NULL;
    SockAddress*   e;
    unsigned       flags = 0;
    const char *addr;
    const char *port;
    char uaddr[INET6_ADDRSTRLEN+1];
    char uport[33];
    int sock = -1;
    int nn;

    /* lookup peer addr */
    addr = qemu_opt_get(opts, "host");
    port = qemu_opt_get(opts, "port");
    if (addr == NULL || strlen(addr) == 0) {
        addr = "localhost";
    }
    if (port == NULL || strlen(port) == 0) {
        fprintf(stderr, "inet_dgram: port not specified\n");
        return -1;
    }

    flags = SOCKET_LIST_DGRAM;
    if (qemu_opt_get_bool(opts, "ipv4", 0)) {
        flags &= SOCKET_LIST_FORCE_IN6;
        flags |= SOCKET_LIST_FORCE_INET;
    }
    if (qemu_opt_get_bool(opts, "ipv6", 0)) {
        flags &= SOCKET_LIST_FORCE_INET;
        flags |= SOCKET_LIST_FORCE_IN6;
    }

    peer_list = sock_address_list_create(addr, port, flags);
    if (peer_list == NULL) {
        fprintf(stderr,"getaddrinfo(%s,%s): %s\n",
                addr, port, errno_str);
        return -1;
    }

    /* lookup local addr */
    addr = qemu_opt_get(opts, "localaddr");
    port = qemu_opt_get(opts, "localport");
    if (addr == NULL || strlen(addr) == 0) {
        addr = NULL;
    }
    if (!port || strlen(port) == 0)
        port = "0";

    flags = SOCKET_LIST_DGRAM | SOCKET_LIST_PASSIVE;
    local_list = sock_address_list_create(addr, port, flags);
    if (local_list == NULL) {
        fprintf(stderr,"getaddrinfo(%s,%s): %s\n",
                addr, port, errno_str);
        goto EXIT;
    }

    if (sock_address_get_numeric_info(local_list[0],
                                       uaddr, INET6_ADDRSTRLEN,
                                       uport, 32)) {
        fprintf(stderr, "%s: getnameinfo: oops\n", __FUNCTION__);
        goto EXIT;
    }

    for (nn = 0; peer_list[nn] != NULL; nn++) {
        SockAddress *local = local_list[0];
        e    = peer_list[nn];
        sock = socket_create(sock_address_get_family(e), SOCKET_DGRAM);
        if (sock < 0) {
            fprintf(stderr,"%s: socket(%s): %s\n", __FUNCTION__,
            sock_address_strfamily(e), errno_str);
            continue;
        }
        socket_set_xreuseaddr(sock);

        /* bind socket */
        if (socket_bind(sock, local) < 0) {
            fprintf(stderr,"%s: bind(%s,%s,%s): OK\n", __FUNCTION__,
                sock_address_strfamily(local), addr, port);
            socket_close(sock);
            continue;
        }

        /* connect to peer */
        if (socket_connect(sock,e) < 0) {
            if (sockets_debug)
                fprintf(stderr, "%s: connect(%s,%s,%s,%s): %s\n", __FUNCTION__,
                        sock_address_strfamily(e),
                        sock_address_to_string(e), addr, port, strerror(errno));
            socket_close(sock);
            continue;
        }
        if (sockets_debug)
            fprintf(stderr, "%s: connect(%s,%s,%s,%s): OK\n", __FUNCTION__,
                        sock_address_strfamily(e),
                        sock_address_to_string(e), addr, port);

        goto EXIT;
    }
    sock = -1;
EXIT:
    if (local_list)
        sock_address_list_free(local_list);
    if (peer_list)
        sock_address_list_free(peer_list);
    return sock;
}

/* compatibility wrapper */
static int inet_parse(QemuOpts *opts, const char *str)
{
    const char *optstr, *h;
    char addr[64];
    char port[33];
    int pos;

    /* parse address */
    if (str[0] == ':') {
        /* no host given */
        addr[0] = '\0';
        if (1 != sscanf(str,":%32[^,]%n",port,&pos)) {
            fprintf(stderr, "%s: portonly parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
    } else if (str[0] == '[') {
        /* IPv6 addr */
        if (2 != sscanf(str,"[%64[^]]]:%32[^,]%n",addr,port,&pos)) {
            fprintf(stderr, "%s: ipv6 parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
        qemu_opt_set(opts, "ipv6", "on");
    } else if (qemu_isdigit(str[0])) {
        /* IPv4 addr */
        if (2 != sscanf(str,"%64[0-9.]:%32[^,]%n",addr,port,&pos)) {
            fprintf(stderr, "%s: ipv4 parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
        qemu_opt_set(opts, "ipv4", "on");
    } else {
        /* hostname */
        if (2 != sscanf(str,"%64[^:]:%32[^,]%n",addr,port,&pos)) {
            fprintf(stderr, "%s: hostname parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
    }
    qemu_opt_set(opts, "host", addr);
    qemu_opt_set(opts, "port", port);

    /* parse options */
    optstr = str + pos;
    h = strstr(optstr, ",to=");
    if (h)
        qemu_opt_set(opts, "to", h+4);
    if (strstr(optstr, ",ipv4"))
        qemu_opt_set(opts, "ipv4", "on");
    if (strstr(optstr, ",ipv6"))
        qemu_opt_set(opts, "ipv6", "on");
#ifdef CONFIG_ANDROID
    h = strstr(optstr, ",socket=");
    if (h) {
        int socket_fd;
        char str_fd[12];
        if (1 != sscanf(h+7,"%d",&socket_fd)) {
            fprintf(stderr,"%s: socket fd parse error (%s)\n",
                    __FUNCTION__, h+7);
            return -1;
        }
        if (socket_fd < 0 || socket_fd >= INT_MAX) {
            fprintf(stderr,"%s: socket fd range error (%d)\n",
                    __FUNCTION__, socket_fd);
            return -1;
        }
        snprintf(str_fd, sizeof str_fd, "%d", socket_fd);
        qemu_opt_set(opts, "socket", str_fd);
    }
#endif
    return 0;
}

int inet_listen(const char *str, char *ostr, int olen,
                int socktype, int port_offset)
{
    QemuOpts *opts;
    char *optstr;
    int sock = -1;

    opts = qemu_opts_create(&dummy_opts, NULL, 0);
    if (inet_parse(opts, str) == 0) {
        sock = inet_listen_opts(opts, port_offset);
        if (sock != -1 && ostr) {
            optstr = strchr(str, ',');
            if (qemu_opt_get_bool(opts, "ipv6", 0)) {
                snprintf(ostr, olen, "[%s]:%s%s",
                         qemu_opt_get(opts, "host"),
                         qemu_opt_get(opts, "port"),
                         optstr ? optstr : "");
            } else {
                snprintf(ostr, olen, "%s:%s%s",
                         qemu_opt_get(opts, "host"),
                         qemu_opt_get(opts, "port"),
                         optstr ? optstr : "");
            }
        }
    }
    qemu_opts_del(opts);
    return sock;
}

int inet_connect(const char *str, int socktype)
{
    QemuOpts *opts;
    int sock = -1;

    opts = qemu_opts_create(&dummy_opts, NULL, 0);
    if (inet_parse(opts, str) == 0)
        sock = inet_connect_opts(opts);
    qemu_opts_del(opts);
    return sock;
}

#ifndef _WIN32

int unix_listen_opts(QemuOpts *opts)
{
    const char *path = qemu_opt_get(opts, "path");
    char        unpath[PATH_MAX];
    const char *upath;
    int sock, fd;

    if (path && strlen(path)) {
        upath = path;
    } else {
        char *tmpdir = getenv("TMPDIR");
        snprintf(unpath, sizeof(unpath), "%s/qemu-socket-XXXXXX",
                 tmpdir ? tmpdir : "/tmp");
        upath = unpath;
        /*
         * This dummy fd usage silences the mktemp() unsecure warning.
         * Using mkstemp() doesn't make things more secure here
         * though.  bind() complains about existing files, so we have
         * to unlink first and thus re-open the race window.  The
         * worst case possible is bind() failing, i.e. a DoS attack.
         */
        fd = mkstemp(unpath); close(fd);
        qemu_opt_set(opts, "path", unpath);
    }

    sock = socket_unix_server(upath, SOCKET_STREAM);

    if (sock < 0) {
        fprintf(stderr, "bind(unix:%s): %s\n", upath, errno_str);
        goto err;
    }

    if (sockets_debug)
        fprintf(stderr, "bind(unix:%s): OK\n", upath);

    return sock;

err:
    socket_close(sock);
    return -1;
}

int unix_connect_opts(QemuOpts *opts)
{
    SockAddress  un;
    const char *path = qemu_opt_get(opts, "path");
    int ret, sock;

    sock = socket_create_unix(SOCKET_STREAM);
    if (sock < 0) {
        perror("socket(unix)");
        return -1;
    }

    sock_address_init_unix(&un, path);
    ret = socket_connect(sock, &un);
    sock_address_done(&un);
    if (ret < 0) {
        fprintf(stderr, "connect(unix:%s): %s\n", path, errno_str);
        return -1;
    }


    if (sockets_debug)
        fprintf(stderr, "connect(unix:%s): OK\n", path);
    return sock;
}

/* compatibility wrapper */
int unix_listen(const char *str, char *ostr, int olen)
{
    QemuOpts *opts;
    char *path, *optstr;
    int sock, len;

    opts = qemu_opts_create(&dummy_opts, NULL, 0);

    optstr = strchr(str, ',');
    if (optstr) {
        len = optstr - str;
        if (len) {
            path = qemu_malloc(len+1);
            snprintf(path, len+1, "%.*s", len, str);
            qemu_opt_set(opts, "path", path);
            qemu_free(path);
        }
    } else {
        qemu_opt_set(opts, "path", str);
    }

    sock = unix_listen_opts(opts);

    if (sock != -1 && ostr)
        snprintf(ostr, olen, "%s%s", qemu_opt_get(opts, "path"), optstr ? optstr : "");
    qemu_opts_del(opts);
    return sock;
}

int unix_connect(const char *path)
{
    QemuOpts *opts;
    int sock;

    opts = qemu_opts_create(&dummy_opts, NULL, 0);
    qemu_opt_set(opts, "path", path);
    sock = unix_connect_opts(opts);
    qemu_opts_del(opts);
    return sock;
}

#else

int unix_listen_opts(QemuOpts *opts)
{
    fprintf(stderr, "unix sockets are not available on windows\n");
    return -1;
}

int unix_connect_opts(QemuOpts *opts)
{
    fprintf(stderr, "unix sockets are not available on windows\n");
    return -1;
}

int unix_listen(const char *path, char *ostr, int olen)
{
    fprintf(stderr, "unix sockets are not available on windows\n");
    return -1;
}

int unix_connect(const char *path)
{
    fprintf(stderr, "unix sockets are not available on windows\n");
    return -1;
}

#endif

#ifndef CONFIG_ANDROID /* see sockets.c */
#ifdef _WIN32
static void socket_cleanup(void)
{
    WSACleanup();
}
#endif

int socket_init(void)
{
#ifdef _WIN32
    WSADATA Data;
    int ret, err;

    ret = WSAStartup(MAKEWORD(2,2), &Data);
    if (ret != 0) {
        err = WSAGetLastError();
        fprintf(stderr, "WSAStartup: %d\n", err);
        return -1;
    }
    atexit(socket_cleanup);
#endif
    return 0;
}
#endif /* !CONFIG_ANDROID */

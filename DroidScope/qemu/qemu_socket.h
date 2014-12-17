/* headers to use the BSD sockets */
#ifndef QEMU__SOCKET_H
#define QEMU__SOCKET_H

#include "qemu-option.h"

#include "sockets.h"
#define  socket_error()  errno
#define  closesocket     socket_close

/* New, ipv6-ready socket helper functions, see qemu-sockets.c */
int inet_listen_opts(QemuOpts *opts, int port_offset);
int inet_listen(const char *str, char *ostr, int olen,
                int socktype, int port_offset);
int inet_connect_opts(QemuOpts *opts);
int inet_connect(const char *str, int socktype);
int inet_dgram_opts(QemuOpts *opts);
const char *inet_strfamily(int family);

int unix_listen_opts(QemuOpts *opts);
int unix_listen(const char *path, char *ostr, int olen);
int unix_connect_opts(QemuOpts *opts);
int unix_connect(const char *path);

/* Old, ipv4 only bits.  Don't use for new code. */
int parse_host_port(SockAddress*  saddr, const char *str);
int parse_host_src_port(SockAddress*  haddr, SockAddress*  saddr,
                        const char *str);

#endif /* QEMU__SOCKET_H */

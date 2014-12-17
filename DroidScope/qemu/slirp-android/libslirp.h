#ifndef _LIBSLIRP_H
#define _LIBSLIRP_H

#include <stdint.h>
#include <stdio.h>
#include "sockets.h"
#include "slirp.h"
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define socket_close  winsock2_socket_close3
#  include <winsock2.h>
#  undef socket_close
#else
#  include <sys/select.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct mbuf;

int    inet_strtoip(const char*  str, uint32_t  *ip);
char*  inet_iptostr(uint32_t  ip);

void slirp_init(int restricted, const char *special_ip);

void slirp_select_fill(int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_select_poll(fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_input(const uint8_t *pkt, int pkt_len);

/* you must provide the following functions: */
int slirp_can_output(void);
void slirp_output(const uint8_t *pkt, int pkt_len);

/* ---------------------------------------------------*/
/* User mode network stack restrictions */
void slirp_drop_udp();
void slirp_drop_tcp();
void slirp_add_allow(unsigned long dst_addr, int dst_lport,
                     int dst_hport, u_int8_t proto);
void slirp_drop_log_fd(FILE* fd);

/** Get the drop log fd */
FILE* get_slirp_drop_log_fd(void);
int slirp_should_drop(unsigned long dst_addr,
                      int dst_port,
                      u_int8_t proto);
int slirp_drop_log(const char* format, ...);

/* for network forwards */
void slirp_add_net_forward(unsigned long dest_ip, unsigned long dest_mask,
                           int dest_lport, int dest_hport,
                           unsigned long redirect_ip, int redirect_port);

int slirp_should_net_forward(unsigned long remote_ip, int remote_port,
                             unsigned long *redirect_ip, int *redirect_port);
/* ---------------------------------------------------*/

/**
 * Additional network stack modifications, aiming to detect and log
 * any network activity initiated by any binary outisde the context of
 * the running browser.
 */

void slirp_dns_log_fd(FILE* fd);
/** Get the dns log fd */
FILE* get_slirp_dns_log_fd(void);
/** Logs the DNS name in DNS query issued by the VM. */
int slirp_log_dns(struct mbuf* m, int dropped);
/** IP packet dump of DNS queris and responses. */
int slirp_dump_dns(struct mbuf* m);
/** Sets an upper limit for the number of allowed DNS requests from
 * the VM.
 */
void slirp_set_max_dns_conns(int max_dns_conns);
/* Returns the max number of allowed DNS requests.*/
int slirp_get_max_dns_conns();

/**
 * Modifications for implementing "-net-forward-tcp2sink' option.
 */

void slirp_forward_dropped_tcp2sink(unsigned long sink_ip,  int sink_port);
int slirp_should_forward_dropped_tcp2sink();
unsigned long slirp_get_tcp_sink_ip();
int slirp_get_tcp_sink_port();




/* ---------------------------------------------------*/

void slirp_redir_loop(void (*func)(void *opaque, int is_udp,
                                   const SockAddress *laddr,
                                   const SockAddress *faddr),
                     void *opaque);
int slirp_redir_rm(int is_udp, int host_port);
int slirp_redir(int is_udp, int host_port,
                uint32_t guest_addr, int guest_port);

int slirp_unredir(int is_udp, int host_port);

int slirp_add_dns_server(const SockAddress*  dns_addr);
int slirp_get_system_dns_servers(void);
int slirp_add_exec(int do_pty, const void *args, int addr_low_byte,
                   int guest_port);

extern const char *tftp_prefix;
extern char slirp_hostname[33];
extern const char *bootp_filename;

void slirp_stats(void);
void slirp_socket_recv(int addr_low_byte, int guest_port, const uint8_t *buf,
		int size);
size_t slirp_socket_can_recv(int addr_low_byte, int guest_port);

#ifdef __cplusplus
}
#endif

#endif

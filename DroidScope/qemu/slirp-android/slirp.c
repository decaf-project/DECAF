/*
 * libslirp glue
 *
 * Copyright (c) 2004-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu-common.h"
#include "qemu-char.h"
#include "slirp.h"
#include "proxy_common.h"
#include "hw/hw.h"

#include "android/utils/debug.h"  /* for dprint */
#include "android/utils/bufprint.h"
#include "android/android.h"
#include "sockets.h"

#include "qemu-queue.h"

/* proto types */
static void slirp_net_forward_init(void);


#define  D(...)   VERBOSE_PRINT(slirp,__VA_ARGS__)
#define  DN(...)  do { if (VERBOSE_CHECK(slirp)) dprintn(__VA_ARGS__); } while (0)

/* host address */
uint32_t our_addr_ip;
/* host dns address */
uint32_t dns_addr[DNS_ADDR_MAX];
int      dns_addr_count;

/* host loopback address */
uint32_t loopback_addr_ip;

/* address for slirp virtual addresses */
uint32_t  special_addr_ip;

/* virtual address alias for host */
uint32_t alias_addr_ip;

static const uint8_t special_ethaddr[6] = {
    0x52, 0x54, 0x00, 0x12, 0x35, 0x00
};

/* ARP cache for the guest IP addresses (XXX: allow many entries) */
uint8_t client_ethaddr[6];
static ipaddr_t  client_ip;

static const uint8_t zero_ethaddr[6] = { 0, 0, 0, 0, 0, 0 };

const char *slirp_special_ip = CTL_SPECIAL;
int slirp_restrict;
static int do_slowtimo;
int link_up;
struct timeval tt;
FILE *lfd;
struct ex_list *exec_list;

/* XXX: suppress those select globals */
fd_set *global_readfds, *global_writefds, *global_xfds;

char slirp_hostname[33];

int slirp_add_dns_server(const SockAddress*  new_dns_addr)
{
    int   dns_ip;

    if (dns_addr_count >= DNS_ADDR_MAX)
        return -1;

    dns_ip = sock_address_get_ip(new_dns_addr);
    if (dns_ip == -1)
        return -1;

    dns_addr[dns_addr_count++] = dns_ip;
    return 0;
}


#ifdef _WIN32

int slirp_get_system_dns_servers(void)
{
    FIXED_INFO *FixedInfo=NULL;
    ULONG    BufLen;
    DWORD    ret;
    IP_ADDR_STRING *pIPAddr;

    if (dns_addr_count > 0)
        return dns_addr_count;

    FixedInfo = (FIXED_INFO *)GlobalAlloc(GPTR, sizeof(FIXED_INFO));
    BufLen = sizeof(FIXED_INFO);

    if (ERROR_BUFFER_OVERFLOW == GetNetworkParams(FixedInfo, &BufLen)) {
        if (FixedInfo) {
            GlobalFree(FixedInfo);
            FixedInfo = NULL;
        }
        FixedInfo = GlobalAlloc(GPTR, BufLen);
    }

    if ((ret = GetNetworkParams(FixedInfo, &BufLen)) != ERROR_SUCCESS) {
        printf("GetNetworkParams failed. ret = %08x\n", (u_int)ret );
        if (FixedInfo) {
            GlobalFree(FixedInfo);
            FixedInfo = NULL;
        }
        return -1;
    }

    D( "DNS Servers:");
    pIPAddr = &(FixedInfo->DnsServerList);
    while (pIPAddr && dns_addr_count < DNS_ADDR_MAX) {
        uint32_t  ip;
        D( "  %s", pIPAddr->IpAddress.String );
        if (inet_strtoip(pIPAddr->IpAddress.String, &ip) == 0) {
            if (ip == loopback_addr_ip)
                ip = our_addr_ip;
            if (dns_addr_count < DNS_ADDR_MAX)
                dns_addr[dns_addr_count++] = ip;
        }
        pIPAddr = pIPAddr->Next;
    }

    if (FixedInfo) {
        GlobalFree(FixedInfo);
        FixedInfo = NULL;
    }
    if (dns_addr_count <= 0)
        return -1;

    return dns_addr_count;
}

#else

int slirp_get_system_dns_servers(void)
{
    char buff[512];
    char buff2[257];
    FILE *f;

    if (dns_addr_count > 0)
        return dns_addr_count;

#ifdef CONFIG_DARWIN
    /* on Darwin /etc/resolv.conf is a symlink to /private/var/run/resolv.conf
     * in some siutations, the symlink can be destroyed and the system will not
     * re-create it. Darwin-aware applications will continue to run, but "legacy"
     * Unix ones will not.
     */
     f = fopen("/private/var/run/resolv.conf", "r");
     if (!f)
        f = fopen("/etc/resolv.conf", "r");  /* desperate attempt to sanity */
#else
    f = fopen("/etc/resolv.conf", "r");
#endif
    if (!f)
        return -1;

    DN("emulator: IP address of your DNS(s): ");
    while (fgets(buff, 512, f) != NULL) {
        if (sscanf(buff, "nameserver%*[ \t]%256s", buff2) == 1) {
            uint32_t  tmp_ip;

            if (inet_strtoip(buff2, &tmp_ip) < 0)
                continue;
            if (tmp_ip == loopback_addr_ip)
                tmp_ip = our_addr_ip;
            if (dns_addr_count < DNS_ADDR_MAX) {
                dns_addr[dns_addr_count++] = tmp_ip;
                if (dns_addr_count > 1)
                    DN(", ");
                DN("%s", inet_iptostr(tmp_ip));
            } else {
                DN("(more)");
                break;
            }
        }
    }
    DN("\n");
    fclose(f);

    if (!dns_addr_count)
        return -1;

    return dns_addr_count;
}

#endif

static void slirp_state_save(QEMUFile *f, void *opaque);
static int slirp_state_load(QEMUFile *f, void *opaque, int version_id);

void slirp_init(int restricted, const char *special_ip)
{
#if DEBUG
    int   slirp_logmask = 0;
    char  slirp_logfile[512];
    {
        const char*  env = getenv( "ANDROID_SLIRP_LOGMASK" );
        if (env != NULL)
            slirp_logmask = atoi(env);
         else if (VERBOSE_CHECK(slirp))
            slirp_logmask = DEBUG_DEFAULT;
    }

    {
        char*  p   = slirp_logfile;
        char*  end = p + sizeof(slirp_logfile);

        p = bufprint_temp_file( p, end, "slirp.log" );
        if (p >= end) {
            dprint( "cannot create slirp log file in temporary directory" );
            slirp_logmask = 0;
        }
    }
    if (slirp_logmask) {
        dprint( "sending slirp logs with mask %x to %s", slirp_logmask, slirp_logfile );
        debug_init( slirp_logfile, slirp_logmask );
    }
#endif

    link_up = 1;
    slirp_restrict = restricted;

    if_init();
    ip_init();

    /* Initialise mbufs *after* setting the MTU */
    m_init();

    /* set default addresses */
    inet_strtoip("127.0.0.1", &loopback_addr_ip);

    if (dns_addr_count == 0) {
        if (slirp_get_system_dns_servers() < 0) {
            dns_addr[0]    = loopback_addr_ip;
            dns_addr_count = 1;
            fprintf (stderr, "Warning: No DNS servers found\n");
        }
    }

    inet_strtoip(CTL_SPECIAL, &special_addr_ip);

    alias_addr_ip = special_addr_ip | CTL_ALIAS;
    getouraddr();
    register_savevm("slirp", 0, 1, slirp_state_save, slirp_state_load, NULL);

    slirp_net_forward_init();
}

#define CONN_CANFSEND(so) (((so)->so_state & (SS_FCANTSENDMORE|SS_ISFCONNECTED)) == SS_ISFCONNECTED)
#define CONN_CANFRCV(so) (((so)->so_state & (SS_FCANTRCVMORE|SS_ISFCONNECTED)) == SS_ISFCONNECTED)
#define UPD_NFDS(x) if (nfds < (x)) nfds = (x)

/*
 * curtime kept to an accuracy of 1ms
 */
#ifdef _WIN32
static void updtime(void)
{
    struct _timeb tb;

    _ftime(&tb);
    curtime = (u_int)tb.time * (u_int)1000;
    curtime += (u_int)tb.millitm;
}
#else
static void updtime(void)
{
        gettimeofday(&tt, NULL);

	curtime = (u_int)tt.tv_sec * (u_int)1000;
	curtime += (u_int)tt.tv_usec / (u_int)1000;

	if ((tt.tv_usec % 1000) >= 500)
	   curtime++;
}
#endif

void slirp_select_fill(int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds)
{
    struct socket *so, *so_next;
    struct timeval timeout;
    int nfds;
    int tmp_time;

    /* fail safe */
    global_readfds = NULL;
    global_writefds = NULL;
    global_xfds = NULL;

    nfds = *pnfds;
	/*
	 * First, TCP sockets
	 */
	do_slowtimo = 0;
	if (link_up) {
		/*
		 * *_slowtimo needs calling if there are IP fragments
		 * in the fragment queue, or there are TCP connections active
		 */
		do_slowtimo = ((tcb.so_next != &tcb) ||
                (&ipq.ip_link != ipq.ip_link.next));

		for (so = tcb.so_next; so != &tcb; so = so_next) {
			so_next = so->so_next;

			/*
			 * See if we need a tcp_fasttimo
			 */
			if (time_fasttimo == 0 && so->so_tcpcb->t_flags & TF_DELACK)
			   time_fasttimo = curtime; /* Flag when we want a fasttimo */

			/*
			 * NOFDREF can include still connecting to local-host,
			 * newly socreated() sockets etc. Don't want to select these.
	 		 */
			if (so->so_state & SS_NOFDREF || so->s == -1)
			   continue;

            /*
             * don't register proxified socked connections here
             */
            if ((so->so_state & SS_PROXIFIED) != 0)
	            continue;

			/*
			 * Set for reading sockets which are accepting
			 */
			if (so->so_state & SS_FACCEPTCONN) {
                                FD_SET(so->s, readfds);
				UPD_NFDS(so->s);
				continue;
			}

			/*
			 * Set for writing sockets which are connecting
			 */
			if (so->so_state & SS_ISFCONNECTING) {
				FD_SET(so->s, writefds);
				UPD_NFDS(so->s);
				continue;
			}

			/*
			 * Set for writing if we are connected, can send more, and
			 * we have something to send
			 */
			if (CONN_CANFSEND(so) && so->so_rcv.sb_cc) {
				FD_SET(so->s, writefds);
				UPD_NFDS(so->s);
			}

			/*
			 * Set for reading (and urgent data) if we are connected, can
			 * receive more, and we have room for it XXX /2 ?
			 */
			if (CONN_CANFRCV(so) && (so->so_snd.sb_cc < (so->so_snd.sb_datalen/2))) {
				FD_SET(so->s, readfds);
				FD_SET(so->s, xfds);
				UPD_NFDS(so->s);
			}
		}

		/*
		 * UDP sockets
		 */
		for (so = udb.so_next; so != &udb; so = so_next) {
			so_next = so->so_next;

            if ((so->so_state & SS_PROXIFIED) != 0)
                continue;

			/*
			 * See if it's timed out
			 */
			if (so->so_expire) {
				if (so->so_expire <= curtime) {
					udp_detach(so);
					continue;
				} else
					do_slowtimo = 1; /* Let socket expire */
			}

			/*
			 * When UDP packets are received from over the
			 * link, they're sendto()'d straight away, so
			 * no need for setting for writing
			 * Limit the number of packets queued by this session
			 * to 4.  Note that even though we try and limit this
			 * to 4 packets, the session could have more queued
			 * if the packets needed to be fragmented
			 * (XXX <= 4 ?)
			 */
			if ((so->so_state & SS_ISFCONNECTED) && so->so_queued <= 4) {
				FD_SET(so->s, readfds);
				UPD_NFDS(so->s);
			}
		}
	}

	/*
	 * Setup timeout to use minimum CPU usage, especially when idle
	 */

	/*
	 * First, see the timeout needed by *timo
	 */
	timeout.tv_sec = 0;
	timeout.tv_usec = -1;
	/*
	 * If a slowtimo is needed, set timeout to 500ms from the last
	 * slow timeout. If a fast timeout is needed, set timeout within
	 * 200ms of when it was requested.
	 */
	if (do_slowtimo) {
		/* XXX + 10000 because some select()'s aren't that accurate */
		timeout.tv_usec = ((500 - (curtime - last_slowtimo)) * 1000) + 10000;
		if (timeout.tv_usec < 0)
		   timeout.tv_usec = 0;
		else if (timeout.tv_usec > 510000)
		   timeout.tv_usec = 510000;

		/* Can only fasttimo if we also slowtimo */
		if (time_fasttimo) {
			tmp_time = (200 - (curtime - time_fasttimo)) * 1000;
			if (tmp_time < 0)
			   tmp_time = 0;

			/* Choose the smallest of the 2 */
			if (tmp_time < timeout.tv_usec)
			   timeout.tv_usec = (u_int)tmp_time;
		}
	}
    /*
     * now, the proxified sockets
     */
    proxy_manager_select_fill(&nfds, readfds, writefds, xfds);

        *pnfds = nfds;
}

void slirp_select_poll(fd_set *readfds, fd_set *writefds, fd_set *xfds)
{
    struct socket *so, *so_next;
    int ret;

    global_readfds = readfds;
    global_writefds = writefds;
    global_xfds = xfds;

	/* Update time */
	updtime();

	/*
	 * See if anything has timed out
	 */
	if (link_up) {
		if (time_fasttimo && ((curtime - time_fasttimo) >= 2)) {
			tcp_fasttimo();
			time_fasttimo = 0;
		}
		if (do_slowtimo && ((curtime - last_slowtimo) >= 499)) {
			ip_slowtimo();
			tcp_slowtimo();
			last_slowtimo = curtime;
		}
	}

	/*
	 * Check sockets
	 */
	if (link_up) {
		/*
		 * Check TCP sockets
		 */
		for (so = tcb.so_next; so != &tcb; so = so_next) {
			so_next = so->so_next;

			/*
			 * FD_ISSET is meaningless on these sockets
			 * (and they can crash the program)
			 */
			if (so->so_state & SS_NOFDREF || so->s == -1)
			   continue;

            /*
             * proxified sockets are polled later in this
             * function.
             */
            if ((so->so_state & SS_PROXIFIED) != 0)
                continue;

			/*
			 * Check for URG data
			 * This will soread as well, so no need to
			 * test for readfds below if this succeeds
			 */
			if (FD_ISSET(so->s, xfds))
			   sorecvoob(so);
			/*
			 * Check sockets for reading
			 */
			else if (FD_ISSET(so->s, readfds)) {
				/*
				 * Check for incoming connections
				 */
				if (so->so_state & SS_FACCEPTCONN) {
					tcp_connect(so);
					continue;
				} /* else */
				ret = soread(so);

				/* Output it if we read something */
				if (ret > 0)
				   tcp_output(sototcpcb(so));
			}

			/*
			 * Check sockets for writing
			 */
			if (FD_ISSET(so->s, writefds)) {
			  /*
			   * Check for non-blocking, still-connecting sockets
			   */
			  if (so->so_state & SS_ISFCONNECTING) {
			    /* Connected */
			    so->so_state &= ~SS_ISFCONNECTING;

			    ret = socket_send(so->s, (const void *)&ret, 0);
			    if (ret < 0) {
			      /* XXXXX Must fix, zero bytes is a NOP */
			      if (errno == EAGAIN || errno == EWOULDBLOCK ||
				  errno == EINPROGRESS || errno == ENOTCONN)
				continue;

			      /* else failed */
			      so->so_state = SS_NOFDREF;
			    }
			    /* else so->so_state &= ~SS_ISFCONNECTING; */

			    /*
			     * Continue tcp_input
			     */
			    tcp_input((struct mbuf *)NULL, sizeof(struct ip), so);
			    /* continue; */
			  } else
			    ret = sowrite(so);
			  /*
			   * XXXXX If we wrote something (a lot), there
			   * could be a need for a window update.
			   * In the worst case, the remote will send
			   * a window probe to get things going again
			   */
			}

			/*
			 * Probe a still-connecting, non-blocking socket
			 * to check if it's still alive
	 	 	 */
#ifdef PROBE_CONN
			if (so->so_state & SS_ISFCONNECTING) {
			  ret = socket_recv(so->s, (char *)&ret, 0);

			  if (ret < 0) {
			    /* XXX */
			    if (errno == EAGAIN || errno == EWOULDBLOCK ||
				errno == EINPROGRESS || errno == ENOTCONN)
			      continue; /* Still connecting, continue */

			    /* else failed */
			    so->so_state = SS_NOFDREF;

			    /* tcp_input will take care of it */
			  } else {
			    ret = socket_send(so->s, &ret, 0);
			    if (ret < 0) {
			      /* XXX */
			      if (errno == EAGAIN || errno == EWOULDBLOCK ||
				  errno == EINPROGRESS || errno == ENOTCONN)
				continue;
			      /* else failed */
			      so->so_state = SS_NOFDREF;
			    } else
			      so->so_state &= ~SS_ISFCONNECTING;

			  }
			  tcp_input((struct mbuf *)NULL, sizeof(struct ip),so);
			} /* SS_ISFCONNECTING */
#endif
		}

		/*
		 * Now UDP sockets.
		 * Incoming packets are sent straight away, they're not buffered.
		 * Incoming UDP data isn't buffered either.
		 */
		for (so = udb.so_next; so != &udb; so = so_next) {
			so_next = so->so_next;

            if ((so->so_state & SS_PROXIFIED) != 0)
                continue;

			if (so->s != -1 && FD_ISSET(so->s, readfds)) {
                            sorecvfrom(so);
                        }
		}
	}

    /*
     * Now the proxified sockets
     */
    proxy_manager_poll(readfds, writefds, xfds);

	/*
	 * See if we can start outputting
	 */
	if (if_queued && link_up)
	   if_start();

	/* clear global file descriptor sets.
	 * these reside on the stack in vl.c
	 * so they're unusable if we're not in
	 * slirp_select_fill or slirp_select_poll.
	 */
	 global_readfds = NULL;
	 global_writefds = NULL;
	 global_xfds = NULL;
}

#define ETH_ALEN 6
#define ETH_HLEN 14

#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/

#define	ARPOP_REQUEST	1		/* ARP request			*/
#define	ARPOP_REPLY	2		/* ARP reply			*/

struct ethhdr
{
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	unsigned short	h_proto;		/* packet type ID field	*/
};

struct arphdr
{
	unsigned short	ar_hrd;		/* format of hardware address	*/
	unsigned short	ar_pro;		/* format of protocol address	*/
	unsigned char	ar_hln;		/* length of hardware address	*/
	unsigned char	ar_pln;		/* length of protocol address	*/
	unsigned short	ar_op;		/* ARP opcode (command)		*/

	 /*
	  *	 Ethernet looks like this : This bit is variable sized however...
	  */
	unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
	unsigned char		ar_sip[4];		/* sender IP address		*/
	unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
	unsigned char		ar_tip[4];		/* target IP address		*/
};

static void arp_input(const uint8_t *pkt, int pkt_len)
{
    struct ethhdr *eh = (struct ethhdr *)pkt;
    struct arphdr *ah = (struct arphdr *)(pkt + ETH_HLEN);
    uint8_t arp_reply[ETH_HLEN + sizeof(struct arphdr)];
    struct ethhdr *reh = (struct ethhdr *)arp_reply;
    struct arphdr *rah = (struct arphdr *)(arp_reply + ETH_HLEN);
    int ar_op;
    struct ex_list *ex_ptr;

    ar_op = ntohs(ah->ar_op);
    switch(ar_op) {
        uint32_t    ar_tip_ip;

    case ARPOP_REQUEST:
        ar_tip_ip = ip_read32h(ah->ar_tip);
        if ((ar_tip_ip & 0xffffff00) == special_addr_ip) {
            uint32_t  ar_tip_low = ar_tip_ip & 0xff;
            if ( CTL_IS_DNS(ar_tip_low) || ar_tip_low == CTL_ALIAS)
                goto arp_ok;
            for (ex_ptr = exec_list; ex_ptr; ex_ptr = ex_ptr->ex_next) {
                if (ex_ptr->ex_addr == ar_tip_low)
                    goto arp_ok;
            }
            return;
        arp_ok:
            /* XXX: make an ARP request to have the client address */
            memcpy(client_ethaddr, eh->h_source, ETH_ALEN);

            /* ARP request for alias/dns mac address */
            memcpy(reh->h_dest, pkt + ETH_ALEN, ETH_ALEN);
            memcpy(reh->h_source, special_ethaddr, ETH_ALEN - 1);
            reh->h_source[5] = ar_tip_low;
            reh->h_proto = htons(ETH_P_ARP);

            rah->ar_hrd = htons(1);
            rah->ar_pro = htons(ETH_P_IP);
            rah->ar_hln = ETH_ALEN;
            rah->ar_pln = 4;
            rah->ar_op = htons(ARPOP_REPLY);
            memcpy(rah->ar_sha, reh->h_source, ETH_ALEN);
            memcpy(rah->ar_sip, ah->ar_tip, 4);
            memcpy(rah->ar_tha, ah->ar_sha, ETH_ALEN);
            memcpy(rah->ar_tip, ah->ar_sip, 4);
            slirp_output(arp_reply, sizeof(arp_reply));
        }
        break;
    case ARPOP_REPLY:
        /* reply to request of client mac address ? */
        if (!memcmp(client_ethaddr, zero_ethaddr, ETH_ALEN) &&
            ip_equal( ip_read(ah->ar_sip), client_ip )) {
            memcpy(client_ethaddr, ah->ar_sha, ETH_ALEN);
        }
        break;
    default:
        break;
    }
}

void slirp_input(const uint8_t *pkt, int pkt_len)
{
    struct mbuf *m;
    int proto;

    if (pkt_len < ETH_HLEN)
        return;

    proto = ntohs(*(uint16_t *)(pkt + 12));
    switch(proto) {
    case ETH_P_ARP:
        arp_input(pkt, pkt_len);
        break;
    case ETH_P_IP:
        m = m_get();
        if (!m)
            return;
        /* Note: we add to align the IP header */
        if (M_FREEROOM(m) < pkt_len + 2) {
            m_inc(m, pkt_len + 2);
        }
        m->m_len = pkt_len + 2;
        memcpy(m->m_data + 2, pkt, pkt_len);

        m->m_data += 2 + ETH_HLEN;
        m->m_len -= 2 + ETH_HLEN;

        ip_input(m);
        break;
    default:
        break;
    }
}

/* output the IP packet to the ethernet device */
void if_encap(const uint8_t *ip_data, int ip_data_len)
{
    uint8_t buf[1600];
    struct ethhdr *eh = (struct ethhdr *)buf;

    if (ip_data_len + ETH_HLEN > sizeof(buf))
        return;

    if (!memcmp(client_ethaddr, zero_ethaddr, ETH_ALEN)) {
        uint8_t arp_req[ETH_HLEN + sizeof(struct arphdr)];
        struct ethhdr *reh = (struct ethhdr *)arp_req;
        struct arphdr *rah = (struct arphdr *)(arp_req + ETH_HLEN);
        const struct ip *iph = (const struct ip *)ip_data;

        /* If the client addr is not known, there is no point in
           sending the packet to it. Normally the sender should have
           done an ARP request to get its MAC address. Here we do it
           in place of sending the packet and we hope that the sender
           will retry sending its packet. */
        memset(reh->h_dest, 0xff, ETH_ALEN);
        memcpy(reh->h_source, special_ethaddr, ETH_ALEN - 1);
        reh->h_source[5] = CTL_ALIAS;
        reh->h_proto = htons(ETH_P_ARP);
        rah->ar_hrd = htons(1);
        rah->ar_pro = htons(ETH_P_IP);
        rah->ar_hln = ETH_ALEN;
        rah->ar_pln = 4;
        rah->ar_op = htons(ARPOP_REQUEST);
        /* source hw addr */
        memcpy(rah->ar_sha, special_ethaddr, ETH_ALEN - 1);
        rah->ar_sha[5] = CTL_ALIAS;
        /* source IP */
        ip_write32h(alias_addr_ip, rah->ar_sip);
        /* target hw addr (none) */
        memset(rah->ar_tha, 0, ETH_ALEN);
        /* target IP */
        ip_write( iph->ip_dst, rah->ar_tip );
        client_ip   = iph->ip_dst;
        slirp_output(arp_req, sizeof(arp_req));
    } else {
        memcpy(eh->h_dest, client_ethaddr, ETH_ALEN);
        memcpy(eh->h_source, special_ethaddr, ETH_ALEN - 1);
        /* XXX: not correct */
        eh->h_source[5] = CTL_ALIAS;
        eh->h_proto = htons(ETH_P_IP);
        memcpy(buf + sizeof(struct ethhdr), ip_data, ip_data_len);
        slirp_output(buf, ip_data_len + ETH_HLEN);
    }
}


/*---------------------------------------------------*/
/* User mode network stack restrictions */
struct fw_allow_entry {
    struct fw_allow_entry* next;
    unsigned long dst_addr;   /* host byte order */
    /* Allowed port range. dst_lport should be the same as dst_hport for a
     * single port. */
    unsigned short dst_lport; /* host byte order */
    unsigned short dst_hport; /* host byte order */
};

static int drop_udp = 0;
static int drop_tcp = 0;
static struct fw_allow_entry* allow_tcp_entries = NULL;
static struct fw_allow_entry* allow_udp_entries = NULL;
static FILE* drop_log_fd = NULL;
static FILE* dns_log_fd = NULL;
static int max_dns_conns = -1;   /* unlimited max DNS connections by default */
static int slirp_net_forward_inited = 0;

void slirp_drop_udp() {
    drop_udp = 1;
}

void slirp_drop_tcp() {
    drop_tcp = 1;
}

/* TCP traffic forwarding to a sink - If enabled, all TCP traffic to any
 * ip/port that is not explicitly forwared using '-net-forward', and which would
 * otherwise be dropped if '-drop-tcp' has been specified, is redirected to the
 * specified ip:port
 */
int forward_dropped_tcp2sink = 0;
static unsigned long tcp_sink_ip;
int tcp_sink_port;

void slirp_forward_dropped_tcp2sink(unsigned long sink_ip, int sink_port) {
    tcp_sink_ip = sink_ip;
    tcp_sink_port = sink_port;
    forward_dropped_tcp2sink = 1;
}

int slirp_should_forward_dropped_tcp2sink() {
    return forward_dropped_tcp2sink;
}

unsigned long slirp_get_tcp_sink_ip() {
    return tcp_sink_ip;
}
int slirp_get_tcp_sink_port() {
    return tcp_sink_port;
}

/* Fill in the firewall rules. dst_lport and dst_hport are in host byte order */
void slirp_add_allow(unsigned long dst_addr,
                     int dst_lport, int dst_hport,
                     u_int8_t proto) {

    struct fw_allow_entry** ate;
    switch (proto) {
      case IPPROTO_TCP:
          ate = &allow_tcp_entries;
          break;
      case IPPROTO_UDP:
          ate = &allow_udp_entries;
          break;
      default:
          return; // unknown protocol for the FW
    }

    while(*ate != NULL)
        ate = &(*ate)->next;

    *ate = malloc(sizeof(**ate));
    if (*ate == NULL) {
        DEBUG_MISC((dfd,
                    "Unable to create new firewall record, malloc failed\n"));
        exit(-1);
    }

    (*ate)->next = NULL;
    (*ate)->dst_addr = dst_addr;
    (*ate)->dst_lport = dst_lport;
    (*ate)->dst_hport = dst_hport;
}

void slirp_drop_log_fd(FILE* fd) {
    drop_log_fd = fd;
}

void slirp_dns_log_fd(FILE* fd) {
    dns_log_fd = fd;
}

FILE* get_slirp_drop_log_fd(void) {
  return drop_log_fd;
}

FILE* get_slirp_dns_log_fd(void) {
  return dns_log_fd;
}

/* Address and ports are in host byte order */
int slirp_should_drop(unsigned long dst_addr,
                      int dst_port,
                      u_int8_t proto) {

    struct fw_allow_entry* ate;

    switch (proto) {
        case IPPROTO_TCP:
            if (drop_tcp != 0)
                ate = allow_tcp_entries;
            else
                return 0;
            break;
        case IPPROTO_UDP:
            if (drop_udp != 0)
                ate = allow_udp_entries;
            else
                return 0;
            break;
        default:
            return 1;  // unknown protocol for the FW
    }

    while(ate) {
        if ((ate->dst_lport <= dst_port) && (dst_port <= ate->dst_hport)) {
            // allow any destination if 0
            if (ate->dst_addr == 0 || ate->dst_addr == dst_addr)
                return 0;
        }
        ate = ate->next;
    }

    return 1;
}

/*
 * log DNS requests in a separate log
 */
int
slirp_log_dns(struct mbuf* m, int dropped) {
    char dns_query[256];  // max allowable dns name size
    int c = 0;
    int i= 0;
    int index = 0;
    int offset = 40 + 1;  // udp/ip headers length + 1;
    int trim_bytes = 4;

    if (!dns_log_fd)
        return -1;

    /* We assume one DNS name per query: 300 = 255 (max dns name length)
     * + 40 (udp/ip hdr) + 1 byte DNS peamble + 4 bytes DNS suffix
     */
    if (m->m_len < offset || m->m_len > 300) {
        DEBUG_MISC((dfd,"Malformed DNS qeury, length %d \n", (int)m->m_len));
        return -1;
    }
    for (i = offset; i < m->m_len - trim_bytes && index < sizeof(dns_query); i++, index++) {
        c = m->m_data[i];
        if (c < ' ' || c > '~')
            c = '.';

        dns_query[index] = (char)c;
    }
    dns_query[index] = '\0';
    if (!dropped) {
        fprintf(dns_log_fd, "Sent DNS query for, %s\n" , dns_query);
    } else {
        fprintf(dns_log_fd, "Dropped DNS query for, %s\n" , dns_query);
    }
    fflush(dns_log_fd);
    return 1;
}

/*
 * log DNS requests in a separate log
 */
int
slirp_dump_dns(struct mbuf* m) {

    if (!dns_log_fd)
        return 0;
    // first we write the length of the record then the record (IP packet)
    if (!fwrite(&(m->m_len), sizeof(int), 1, dns_log_fd) ||
        !fwrite(m->m_data, m->m_len, 1, dns_log_fd)) {
        return 0;
    }

    fflush(dns_log_fd);
    return 1;
}

/* Log dropped/accepted packet info */
int slirp_drop_log(const char* format, ...) {
    va_list args;

    if (!drop_log_fd)
        return 0;

    va_start(args, format);
    vfprintf(drop_log_fd, format, args);
    va_end(args);

    fflush(drop_log_fd);

    return 1;
}


/* Set max DNS requests allowed to be issued from the VM */
void slirp_set_max_dns_conns(int num_conns) {
    max_dns_conns = num_conns;
}

int slirp_get_max_dns_conns() {
    return max_dns_conns;
}

/* generic guest network redirection functionality for ipv4 */
struct net_forward_entry {
    QTAILQ_ENTRY(net_forward_entry) next;
    /* ip addresses are also in host byte order */
    unsigned long dest_ip;            /* the destination address they try to contact */
    unsigned long dest_mask;          /* the mask to apply to the address for matching */
    /* Range of ports they were trying to contact. In case of a single port,
     * dest_lport should be the same as dest_hport */
    int dest_lport; /* Host byte order */
    int dest_hport; /* Host byte order */

    unsigned long  redirect_ip;
    int redirect_port; /* Host byte order */
};

static QTAILQ_HEAD(net_forwardq, net_forward_entry) net_forwards;

static void slirp_net_forward_init(void)
{
    if (!slirp_net_forward_inited) {
      QTAILQ_INIT(&net_forwards);
      slirp_net_forward_inited = 1;
    }
}

/* all addresses and ports ae in host byte order */
void slirp_add_net_forward(unsigned long dest_ip, unsigned long dest_mask,
                           int dest_lport, int dest_hport,
                           unsigned long redirect_ip, int redirect_port)
{
    slirp_net_forward_init();

    struct net_forward_entry *entry = malloc(sizeof(*entry));
    if (entry == NULL) {
        DEBUG_MISC((dfd, "Unable to create new forwarding entry, malloc failed\n"));
        exit(-1);
    }

    entry->dest_ip = dest_ip;
    entry->dest_mask = dest_mask;
    entry->dest_lport = dest_lport;
    entry->dest_hport = dest_hport;
    entry->redirect_ip = redirect_ip;
    entry->redirect_port = redirect_port;

    QTAILQ_INSERT_TAIL(&net_forwards, entry, next);
}

/* remote_port and redir_port arguments
 * are in network byte order (tcp_subr.c) */
int slirp_should_net_forward(unsigned long remote_ip, int remote_port,
                             unsigned long *redirect_ip, int *redirect_port)
{
    struct net_forward_entry *entry;

    for (entry = net_forwards.tqh_first;
         entry != NULL; entry = entry->next.tqe_next) {

        if ((entry->dest_lport <= remote_port)
            && (remote_port <= entry->dest_hport)) {
            if ((entry->dest_ip & entry->dest_mask)
                == (remote_ip & entry->dest_mask)) {
              *redirect_ip = entry->redirect_ip;
              *redirect_port = entry->redirect_port;
              return 1;
            }
        }
    }

    return 0;
}

/*---------------------------------------------------*/




static void _slirp_redir_loop(void (*func)(void *opaque, int is_udp,
                                           const SockAddress *laddr,
                                           const SockAddress *faddr),
                              void *opaque, int is_udp)
{
    struct socket *head = (is_udp ? &udb : &tcb);
    struct socket *so;

    for (so = head->so_next; so != head; so = so->so_next) {
        SockAddress  local, foreign;
	
		sock_address_init_inet(&local, so->so_laddr_ip, so->so_laddr_port);
		sock_address_init_inet(&foreign, so->so_faddr_ip, so->so_faddr_port);
        func(opaque, is_udp,
             &local, &foreign);
    }
}

void slirp_redir_loop(void (*func)(void *opaque, int is_udp,
                                   const SockAddress *laddr,
                                   const SockAddress *faddr),
                     void *opaque)
{
    _slirp_redir_loop(func, opaque, 0);
    _slirp_redir_loop(func, opaque, 1);
}

/* Unlistens a redirection
 *
 * Return value: number of redirs removed */
int slirp_redir_rm(int is_udp, int host_port)
{
    struct socket *so;
    struct socket *head = (is_udp ? &udb : &tcb);
    int n = 0;

 loop_again:
    for (so = head->so_next; so != head; so = so->so_next) {
        if (so->so_faddr_port == host_port) {
            close(so->s);
            sofree(so);
            n++;
            goto loop_again;
        }
    }

    return n;
}

int slirp_redir(int is_udp, int host_port,
                uint32_t  guest_ip, int guest_port)
{
    if (is_udp) {
        if (!udp_listen(host_port,
                        guest_ip,
                        guest_port, 0))
            return -1;
    } else {
        if (!solisten(host_port, guest_ip, guest_port, 0))
            return -1;
    }
    return 0;
}

int  slirp_unredir(int  is_udp, int  host_port)
{
    if (is_udp)
        return udp_unlisten( host_port );
    else
        return sounlisten( host_port );
}

int slirp_add_exec(int do_pty, const void *args, int addr_low_byte,
                  int guest_port)
{
    return add_exec(&exec_list, do_pty, (char *)args,
                    addr_low_byte, htons(guest_port));
}

ssize_t slirp_send(struct socket *so, const void *buf, size_t len, int flags)
{
	if (so->s == -1 && so->extra) {
		qemu_chr_write(so->extra, buf, len);
		return len;
	}

	return send(so->s, buf, len, flags);
}

static struct socket *slirp_find_ctl_socket(int addr_low_byte, int guest_port)
{
	struct socket *so;

	for (so = tcb.so_next; so != &tcb; so = so->so_next) {
		if ((so->so_faddr_ip & 0xffffff00) ==
				special_addr_ip
				&& ((so->so_faddr_port & 0xff) ==
				addr_low_byte)
				&& so->so_faddr_port == guest_port)
			return so;
	}

	return NULL;
}

size_t slirp_socket_can_recv(int addr_low_byte, int guest_port)
{
	struct iovec iov[2];
	struct socket *so;

    if (!link_up)
        return 0;

	so = slirp_find_ctl_socket(addr_low_byte, guest_port);

	if (!so || so->so_state & SS_NOFDREF)
		return 0;

	if (!CONN_CANFRCV(so) || so->so_snd.sb_cc >= (so->so_snd.sb_datalen/2))
		return 0;

	return sopreprbuf(so, iov, NULL);
}

void slirp_socket_recv(int addr_low_byte, int guest_port, const uint8_t *buf,
        int size)
{
    int ret;
    struct socket *so = slirp_find_ctl_socket(addr_low_byte, guest_port);
   
    if (!so)
        return;

    ret = soreadbuf(so, (const char *)buf, size);

    if (ret > 0)
        tcp_output(sototcpcb(so));
}

static void slirp_tcp_save(QEMUFile *f, struct tcpcb *tp)
{
    int i;

    qemu_put_sbe16(f, tp->t_state);
    for (i = 0; i < TCPT_NTIMERS; i++)
        qemu_put_sbe16(f, tp->t_timer[i]);
    qemu_put_sbe16(f, tp->t_rxtshift);
    qemu_put_sbe16(f, tp->t_rxtcur);
    qemu_put_sbe16(f, tp->t_dupacks);
    qemu_put_be16(f, tp->t_maxseg);
    qemu_put_sbyte(f, tp->t_force);
    qemu_put_be16(f, tp->t_flags);
    qemu_put_be32(f, tp->snd_una);
    qemu_put_be32(f, tp->snd_nxt);
    qemu_put_be32(f, tp->snd_up);
    qemu_put_be32(f, tp->snd_wl1);
    qemu_put_be32(f, tp->snd_wl2);
    qemu_put_be32(f, tp->iss);
    qemu_put_be32(f, tp->snd_wnd);
    qemu_put_be32(f, tp->rcv_wnd);
    qemu_put_be32(f, tp->rcv_nxt);
    qemu_put_be32(f, tp->rcv_up);
    qemu_put_be32(f, tp->irs);
    qemu_put_be32(f, tp->rcv_adv);
    qemu_put_be32(f, tp->snd_max);
    qemu_put_be32(f, tp->snd_cwnd);
    qemu_put_be32(f, tp->snd_ssthresh);
    qemu_put_sbe16(f, tp->t_idle);
    qemu_put_sbe16(f, tp->t_rtt);
    qemu_put_be32(f, tp->t_rtseq);
    qemu_put_sbe16(f, tp->t_srtt);
    qemu_put_sbe16(f, tp->t_rttvar);
    qemu_put_be16(f, tp->t_rttmin);
    qemu_put_be32(f, tp->max_sndwnd);
    qemu_put_byte(f, tp->t_oobflags);
    qemu_put_byte(f, tp->t_iobc);
    qemu_put_sbe16(f, tp->t_softerror);
    qemu_put_byte(f, tp->snd_scale);
    qemu_put_byte(f, tp->rcv_scale);
    qemu_put_byte(f, tp->request_r_scale);
    qemu_put_byte(f, tp->requested_s_scale);
    qemu_put_be32(f, tp->ts_recent);
    qemu_put_be32(f, tp->ts_recent_age);
    qemu_put_be32(f, tp->last_ack_sent);
}

static void slirp_sbuf_save(QEMUFile *f, struct sbuf *sbuf)
{
    uint32_t off;

    qemu_put_be32(f, sbuf->sb_cc);
    qemu_put_be32(f, sbuf->sb_datalen);
    off = (uint32_t)(sbuf->sb_wptr - sbuf->sb_data);
    qemu_put_sbe32(f, off);
    off = (uint32_t)(sbuf->sb_rptr - sbuf->sb_data);
    qemu_put_sbe32(f, off);
    qemu_put_buffer(f, (unsigned char*)sbuf->sb_data, sbuf->sb_datalen);
}

static void slirp_socket_save(QEMUFile *f, struct socket *so)
{
    qemu_put_be32(f, so->so_urgc);
    qemu_put_be32(f, so->so_faddr_ip);
    qemu_put_be32(f, so->so_laddr_ip);
    qemu_put_be16(f, so->so_faddr_port);
    qemu_put_be16(f, so->so_laddr_port);
    qemu_put_byte(f, so->so_iptos);
    qemu_put_byte(f, so->so_emu);
    qemu_put_byte(f, so->so_type);
    qemu_put_be32(f, so->so_state);
    slirp_sbuf_save(f, &so->so_rcv);
    slirp_sbuf_save(f, &so->so_snd);
    slirp_tcp_save(f, so->so_tcpcb);
}

static void slirp_state_save(QEMUFile *f, void *opaque)
{
    struct ex_list *ex_ptr;

    for (ex_ptr = exec_list; ex_ptr; ex_ptr = ex_ptr->ex_next)
        if (ex_ptr->ex_pty == 3) {
            struct socket *so;
            so = slirp_find_ctl_socket(ex_ptr->ex_addr, ntohs(ex_ptr->ex_fport));
            if (!so)
                continue;

            qemu_put_byte(f, 42);
            slirp_socket_save(f, so);
        }
    qemu_put_byte(f, 0);
}

static void slirp_tcp_load(QEMUFile *f, struct tcpcb *tp)
{
    int i;

    tp->t_state = qemu_get_sbe16(f);
    for (i = 0; i < TCPT_NTIMERS; i++)
        tp->t_timer[i] = qemu_get_sbe16(f);
    tp->t_rxtshift = qemu_get_sbe16(f);
    tp->t_rxtcur = qemu_get_sbe16(f);
    tp->t_dupacks = qemu_get_sbe16(f);
    tp->t_maxseg = qemu_get_be16(f);
    tp->t_force = qemu_get_sbyte(f);
    tp->t_flags = qemu_get_be16(f);
    tp->snd_una = qemu_get_be32(f);
    tp->snd_nxt = qemu_get_be32(f);
    tp->snd_up = qemu_get_be32(f);
    tp->snd_wl1 = qemu_get_be32(f);
    tp->snd_wl2 = qemu_get_be32(f);
    tp->iss = qemu_get_be32(f);
    tp->snd_wnd = qemu_get_be32(f);
    tp->rcv_wnd = qemu_get_be32(f);
    tp->rcv_nxt = qemu_get_be32(f);
    tp->rcv_up = qemu_get_be32(f);
    tp->irs = qemu_get_be32(f);
    tp->rcv_adv = qemu_get_be32(f);
    tp->snd_max = qemu_get_be32(f);
    tp->snd_cwnd = qemu_get_be32(f);
    tp->snd_ssthresh = qemu_get_be32(f);
    tp->t_idle = qemu_get_sbe16(f);
    tp->t_rtt = qemu_get_sbe16(f);
    tp->t_rtseq = qemu_get_be32(f);
    tp->t_srtt = qemu_get_sbe16(f);
    tp->t_rttvar = qemu_get_sbe16(f);
    tp->t_rttmin = qemu_get_be16(f);
    tp->max_sndwnd = qemu_get_be32(f);
    tp->t_oobflags = qemu_get_byte(f);
    tp->t_iobc = qemu_get_byte(f);
    tp->t_softerror = qemu_get_sbe16(f);
    tp->snd_scale = qemu_get_byte(f);
    tp->rcv_scale = qemu_get_byte(f);
    tp->request_r_scale = qemu_get_byte(f);
    tp->requested_s_scale = qemu_get_byte(f);
    tp->ts_recent = qemu_get_be32(f);
    tp->ts_recent_age = qemu_get_be32(f);
    tp->last_ack_sent = qemu_get_be32(f);
    tcp_template(tp);
}

static int slirp_sbuf_load(QEMUFile *f, struct sbuf *sbuf)
{
    uint32_t off, sb_cc, sb_datalen;

    sb_cc = qemu_get_be32(f);
    sb_datalen = qemu_get_be32(f);

    sbreserve(sbuf, sb_datalen);

    if (sbuf->sb_datalen != sb_datalen)
        return -ENOMEM;

    sbuf->sb_cc = sb_cc;

    off = qemu_get_sbe32(f);
    sbuf->sb_wptr = sbuf->sb_data + off;
    off = qemu_get_sbe32(f);
    sbuf->sb_rptr = sbuf->sb_data + off;
    qemu_get_buffer(f, (unsigned char*)sbuf->sb_data, sbuf->sb_datalen);

    return 0;
}

static int slirp_socket_load(QEMUFile *f, struct socket *so)
{
    if (tcp_attach(so) < 0)
        return -ENOMEM;

    so->so_urgc = qemu_get_be32(f);
    so->so_faddr_ip = qemu_get_be32(f);
    so->so_laddr_ip = qemu_get_be32(f);
    so->so_faddr_port = qemu_get_be16(f);
    so->so_laddr_port = qemu_get_be16(f);
    so->so_iptos = qemu_get_byte(f);
    so->so_emu = qemu_get_byte(f);
    so->so_type = qemu_get_byte(f);
    so->so_state = qemu_get_be32(f);
    if (slirp_sbuf_load(f, &so->so_rcv) < 0)
        return -ENOMEM;
    if (slirp_sbuf_load(f, &so->so_snd) < 0)
        return -ENOMEM;
    slirp_tcp_load(f, so->so_tcpcb);

    return 0;
}

static int slirp_state_load(QEMUFile *f, void *opaque, int version_id)
{
    struct ex_list *ex_ptr;
    int r;

    while ((r = qemu_get_byte(f))) {
        int ret;
        struct socket *so = socreate();

        if (!so)
            return -ENOMEM;

        ret = slirp_socket_load(f, so);

        if (ret < 0)
            return ret;

        if ((so->so_faddr_ip & 0xffffff00) != special_addr_ip)
            return -EINVAL;

        for (ex_ptr = exec_list; ex_ptr; ex_ptr = ex_ptr->ex_next)
            if (ex_ptr->ex_pty == 3 &&
                    (so->so_faddr_ip & 0xff) == ex_ptr->ex_addr &&
                    so->so_faddr_port == ex_ptr->ex_fport)
                break;

        if (!ex_ptr)
            return -EINVAL;

        so->extra = (void *)ex_ptr->ex_exec;
    }

    return 0;
}

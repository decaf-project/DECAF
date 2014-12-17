/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
*/
#include "config.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netinet/in.h>
#define __FAVOR_BSD
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "DECAF_main.h"
#include "DECAF_callback.h"
#ifdef CONFIG_TCG_TAINT
#include "tainting/taintcheck_opt.h"
#endif
#include "trace.h"
#include "tracecap.h"
#include "conf.h" // AWH

typedef struct tcpconn_record{
  uint32_t id;
  uint32_t origin;
  uint32_t curr_seq;
  LIST_ENTRY(tcpconn_record) link;
} tcpconn_record_t;

typedef struct udpconn_record{
  uint32_t id;
  uint32_t origin;
  LIST_ENTRY(udpconn_record) link;
} udpconn_record_t;

typedef struct {
  uint8_t enabled;
  uint8_t proto;
  uint16_t sport;
  uint16_t dport;
  struct in_addr src;
  struct in_addr dst;
} pkt_filter_t;

static pkt_filter_t pkt_filter;

static LIST_HEAD(tcpconn_record_list_head, tcpconn_record) 
		tcpconn_record_head = LIST_HEAD_INITIALIZER(&tcpconn_record_head);
static LIST_HEAD(udpconn_record_list_head, udpconn_record) 
		udpconn_record_head = LIST_HEAD_INITIALIZER(&udpconn_record_head);

/* Taint receive traffic flag */
static int taint_nic_state = 0;

static inline int min(int x, int y) {
  return (x < y) ? x : y;
}

// AWH - convenience function for setting the nic state within the plugin
void internal_do_taint_nic(Monitor *mon, int state)
{
  monitor_printf(mon, "taint_nic_state set to %d\n", state);
  taint_nic_state = state;
}

// AWH void do_taint_nic(int state)
void do_taint_nic(Monitor *mon, const QDict *qdict)
{
  if (qdict_haskey(qdict, "state"))
  {
    taint_nic_state = qdict_get_int(qdict, "state");
    monitor_printf(mon, "taint_nic_state set to %d\n", taint_nic_state);
  }
  else
    monitor_printf(mon, "taint_nic command is malformed\n");
  //taint_nic_state = state;
}

void print_nic_filter () {
  fprintf(stderr,"Enabled: 0x%02x Proto: 0x%02x Sport: %d Dport: %d"
    " Src: %s Dst: %s\n", pkt_filter.enabled, pkt_filter.proto,
    ntohs(pkt_filter.sport), ntohs(pkt_filter.dport),
    inet_ntoa(pkt_filter.src), inet_ntoa(pkt_filter.dst));
}

int update_nic_filter (const char *filter_str, const char *value_str) {
  char *endptr = NULL;

  if (strcmp(value_str,"") == 0) {
      // monitor_printf(default_mon, "Empty %s value string. Ignoring\n", filter_str);
      return -1;
  }

  if (strncmp(filter_str,"clear",5) == 0) {
    memset((void *)(&pkt_filter), 0, sizeof(pkt_filter_t));
    return 0;
  }
  else if (strncmp(filter_str,"proto",5) == 0) {
    if (strncmp(value_str,"tcp",3) == 0) {
      pkt_filter.proto = 6;
      pkt_filter.enabled = 1;
      return 0;
    }
    else if (strncmp(value_str,"udp",3) == 0) {
      pkt_filter.proto = 17;
      pkt_filter.enabled = 1;
      return 0;
    }
    else {
      pkt_filter.proto = 0;
      monitor_printf(default_mon, "Invalid protocol. Ignoring\n");
      return -1;
    }
  }
  else if(strncmp(filter_str,"sport",5) == 0) {
    long longval = strtol(value_str, &endptr, 10);
    size_t len = strlen(value_str);
    if (endptr == (value_str + len)) {
      pkt_filter.sport = (longval < 0xFFFF) ? htons((uint16_t) longval) : 0;
      pkt_filter.enabled = 1;
      return 0;
    }
    else {
      pkt_filter.sport = 0;
      monitor_printf(default_mon, "Invalid port. Ignoring\n");
      return -1;
    }
  }
  else if(strncmp(filter_str,"dport",5) == 0) {
    long longval = strtol(value_str, &endptr, 10);
    size_t len = strlen(value_str);
    if (endptr == (value_str + len)) {
      pkt_filter.dport = (longval < 0xFFFF) ? htons((uint16_t) longval) : 0;
      pkt_filter.enabled = 1;
      return 0;
    }
    else {
      pkt_filter.dport = 0;
      monitor_printf(default_mon, "Invalid port. Ignoring\n");
      return -1;
    }
  }
  else if(strncmp(filter_str,"src",3) == 0) {
    int retval = inet_pton(AF_INET,value_str, &(pkt_filter.src));
    if (retval == 0) {
      pkt_filter.src.s_addr = 0;
      monitor_printf(default_mon, "Invalid address. Ignoring\n");
      return -1;
    }
    pkt_filter.enabled = 1;
    return 0;
  }
  else if(strncmp(filter_str,"dst",3) == 0) {
    int retval = inet_pton(AF_INET,value_str, &(pkt_filter.dst));
    if (retval == 0) {
      pkt_filter.dst.s_addr = 0;
      monitor_printf(default_mon, "Invalid address. Ignoring\n");
      return -1;
    }
    pkt_filter.enabled = 1;
    return 0;
  }
  else {
    monitor_printf(default_mon, "Invalid filter. Ignoring\n");
    return -1;
  }
}

/* Returns 1 if packet matches filter, otherwise returns 0 */
static int apply_nic_filter(struct ip *iph, struct tcphdr *tcph,
  struct udphdr *udph)
{
  /* TCP */
  if ((pkt_filter.proto == 6) && (iph->ip_p == 6)) {
    if (pkt_filter.sport != 0) {
      if (pkt_filter.sport == tcph->th_sport) return 1;
      else return 0;
    }
    else if (pkt_filter.dport != 0) {
      if (pkt_filter.dport == tcph->th_dport) return 1;
      else return 0;
    }
    else return 1;
  }
  else if ((pkt_filter.proto == 17) && (iph->ip_p == 17)) {
    if (pkt_filter.sport != 0) {
      if (pkt_filter.sport == udph->uh_sport) return 1;
      else return 0;
    }
    else if (pkt_filter.dport != 0) {
      if (pkt_filter.dport == udph->uh_dport) return 1;
      else return 0;
    }
    else return 1;
  }
  else if (pkt_filter.src.s_addr != 0) {
    if (pkt_filter.src.s_addr == iph->ip_src.s_addr) return 1;
  }
  else if (pkt_filter.dst.s_addr != 0) {
    if (pkt_filter.dst.s_addr == iph->ip_dst.s_addr) return 1;
  }
  /* Need to handle ports */
  return 0;
}

/* Compute a unique flow/connection identifier given packet information */
static int compute_conn_id(struct ip *iph, struct tcphdr *tcph,
  struct udphdr *udph)
{
  uint32_t conn_id = 0;

  if (tcph) {
    conn_id = iph->ip_p ^ tcph->th_dport ^ tcph->th_sport ^
      iph->ip_dst.s_addr ^ iph->ip_src.s_addr;
  }
  else if (udph) {
    conn_id = iph->ip_p ^ udph->uh_sport ^ udph->uh_dport ^
      iph->ip_src.s_addr ^ iph->ip_dst.s_addr;
  }

  return conn_id;
}

/* 
 * Returns the origin for connection, if ID those not exist it adds an UDP 
 *   connection to the list
*/
static int tracing_get_udp_origin(uint32_t conn_id)
{
  static int udp_conn_ctr = TAINT_ORIGIN_START_UDP_NIC_IN;

  /* If the connection already exists, return origin */
  udpconn_record_t *udp;
  LIST_FOREACH(udp, &udpconn_record_head, link) {
    if (udp->id == conn_id) 
      return udp->origin;
  }

  /* Else, add new connection to list */
  udpconn_record_t *udpconn = malloc(sizeof(udpconn_record_t));
  if (udpconn) {
    udpconn->id = conn_id;
    udpconn->origin = udp_conn_ctr++;

    LIST_INSERT_HEAD(&udpconn_record_head, udpconn, link);
    return udpconn->origin;
  }
  else return -1;

}

/* Adds a new TCP connection if it does not exist */
static int tracing_add_tcp_conn(uint32_t conn_id, uint32_t seq)
{
  static int tcp_conn_ctr = TAINT_ORIGIN_START_TCP_NIC_IN;

  /* If the connection already exists, update seq and return */
  tcpconn_record_t *tcp;
  LIST_FOREACH(tcp, &tcpconn_record_head, link) {
    if (tcp->id == conn_id) {
      tcp->curr_seq = seq;
      return 0;
    }
  }

  /* Else, add new connection to list */
  tcpconn_record_t *tcpconn = malloc(sizeof(tcpconn_record_t));
  if (tcpconn) {
    tcpconn->id = conn_id;
    tcpconn->origin = tcp_conn_ctr++;
    tcpconn->curr_seq = seq;

    LIST_INSERT_HEAD(&tcpconn_record_head, tcpconn, link);
    return 0;
  }
  else return -1;
}

/* Find the seq number for the given connection. Zero if it does not exist */
static uint32_t tracing_get_tcp_seq(uint32_t conn_id)
{
  /* Find connection in list */
  tcpconn_record_t *tcp;
  LIST_FOREACH(tcp, &tcpconn_record_head, link) {
    if (tcp->id == conn_id)
      return tcp->curr_seq;
  }
  return 0;
}

/* Find the origin for the given connection. Zero if it does not exist */
static uint32_t tracing_get_tcp_origin(uint32_t conn_id)
{
  /* Find connection in list */
  tcpconn_record_t *tcp;
  LIST_FOREACH(tcp, &tcpconn_record_head, link) {
    if (tcp->id == conn_id)
      return tcp->origin;
  }
  return 0;
}

/* Deletes a tcp connection if it exists */
static int tracing_del_tcp_conn(uint32_t conn_id)
{
  /* Find connection in list and delete it */
  tcpconn_record_t *tcp;
  LIST_FOREACH(tcp, &tcpconn_record_head, link) {
    if (tcp->id == conn_id) {
      LIST_REMOVE(tcp, link);
      free(tcp);
      break;
    }
  }
  return 0;
}

void tracing_nic_recv(DECAF_Callback_Params* params)
{
	uint8_t *buf=params->nr.buf;
	int size=params->nr.size;
	int start=params->nr.start;
	int stop=params->nr.stop;
	int index=params->nr.cur_pos;

	/* If no data, return */
	if ((buf == NULL )|| (size == 0))return;

	struct ip *iph = (struct ip *) (buf + 14);
	struct tcphdr *tcph = (struct tcphdr *) (buf + 34);
	struct udphdr *udph = (struct udphdr *) (buf + 34);
	uint32_t seq = 0;
	int hlen = 0, tolen, len2 = 0, offset = 0, avail, len;
	taint_record_t record;
	uint32_t conn_id = 0;

#if 0
	memset(&record, 0, sizeof(taint_record_t));
	record.numRecords = 1;
	record.taintBytes[0].source = TAINT_SOURCE_NIC_IN;
#endif
	//TODO: log the connection info in addition to set taint in nic buffer

	/* Check if we need to taint data */
	if (!taint_nic_state || // Ignore if not tainting NIC
			buf[12] != 0x08 || buf[13] != 0 || // Ignore non-IP packets
			(iph->ip_p != 6 && iph->ip_p != 17) || // Ignore non TCP/UDP segments
			((iph->ip_p == 17) && (ntohs(udph->uh_sport) == 53)
					&& tracing_ignore_dns()) // Ignore DNS if requested
			)
		goto L1;
	/* Filter packet */
	if ((pkt_filter.enabled) && (0 == apply_nic_filter(iph, tcph, udph)))
		goto L1;

	/* TCP */
	if (6 == iph->ip_p) {
		conn_id = compute_conn_id(iph, tcph, NULL );

		/* If it is a SYN-ACK packet, create a new outbound connection */
		if ((tcph->th_flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK)) {
			tracing_add_tcp_conn(conn_id, ntohl(tcph->th_seq) + 1);

			if (tracenetlog) {
				uint32_t origin = tracing_get_tcp_origin(conn_id);
				fprintf(tracenetlog,
						"New outbound TCP flow. ID: %u Origin: %u %s:%d-->%s:%d\n",
						conn_id, origin, inet_ntoa(iph->ip_dst),
						ntohs(tcph->th_dport), inet_ntoa(iph->ip_src),
						ntohs(tcph->th_sport));
				fflush(tracenetlog);
			}
		}
		/* If the corresponding connection exists, grab sequence number and
		 set length */
		if ((seq = tracing_get_tcp_seq(conn_id))) {
			/* If it's a FIN packet, then no more data coming, delete connection */
			/*   but handle packet normally since FIN packet can carry data */
			if (tcph->th_flags & TH_FIN) {
				tracing_del_tcp_conn(conn_id);
			}
			tolen = ntohs(iph->ip_len) + 14;
			hlen = 34 + tcph->th_off * 4;
			len2 = tolen - hlen;
		}
		if (len2) {
			record.taintBytes[0].origin = tracing_get_tcp_origin(conn_id);

			if (tracenetlog) {
				fprintf(tracenetlog,
						"Received new TCP data: %010u %s:%d-->%s:%d (%d)\n",
						record.taintBytes[0].origin, inet_ntoa(iph->ip_src),
						ntohs(tcph->th_sport), inet_ntoa(iph->ip_dst),
						ntohs(tcph->th_dport), len2);
				fflush(tracenetlog);
			}
		}
	}
	/* UDP */
	else if (17 == iph->ip_p) {
		conn_id = compute_conn_id(iph, NULL, udph);
		len2 = ntohs(iph->ip_len) - 20 - 8;
		hlen = 34 + 8;
		if (len2) {
			record.taintBytes[0].origin = tracing_get_udp_origin(conn_id);

			/* Log received data */
			if (tracenetlog) {
				fprintf(tracenetlog,
						"Received new UDP data: %010u %s:%d-->%s:%d (%d)\n",
						record.taintBytes[0].origin, inet_ntoa(iph->ip_src),
						ntohs(udph->uh_sport), inet_ntoa(iph->ip_dst),
						ntohs(udph->uh_dport), len2);
				fflush(tracenetlog);
			}
		}
	}

	L1: while (size > 0) {
		avail = stop - index;
		len = size;
		if (len > avail)
			len = avail;
#ifdef CONFIG_TCG_TAINT //at beginning clean the entire packet
		taintcheck_nic_cleanbuf(index, len);
#endif
		if (len2) {
			if (!offset) {
				if (len > hlen)
					for (; offset < len - hlen; offset++) {
						//TODO:	keeping taint info
#ifdef CONFIG_TCG_TAINT
						if (6 == iph->ip_p)
							record.taintBytes[0].offset = ntohl(tcph->th_seq)
									- seq + offset;
						else
							record.taintBytes[0].offset = offset;

						uint8_t taint = 0xff;
						taintcheck_nic_writebuf(index + hlen + offset, 1, &taint);
#endif
						if (tracenetlog) {
							uint8_t *dataPtr = buf + hlen + offset;
							fprintf(tracenetlog, "Z %010u %04u 0x%02x\n",
									record.taintBytes[0].origin,
									record.taintBytes[0].offset, *dataPtr);
							fflush(tracenetlog);
						}
					}
			} else {
				for (; offset < min(len2, offset + len); offset++) {
					if (6 == iph->ip_p)
						record.taintBytes[0].offset = ntohl(tcph->th_seq) - seq
								+ offset;
					else
						record.taintBytes[0].offset = offset;
#ifdef CONFIG_TCG_TAINT
					uint8_t taint = 0xff;
					taintcheck_nic_writebuf(index + offset, 1, &taint);
#endif
					if (tracenetlog) {
						uint8_t *dataPtr = buf + hlen + offset;
						fprintf(tracenetlog, "N %010u %04u 0x%02x\n",
								record.taintBytes[0].origin,
								record.taintBytes[0].offset, *dataPtr);
						fflush(tracenetlog);
					}
				}
			}
		}

		index += len;
		if (index == stop)
			index = start;
		size -= len;
	}

}

void tracing_nic_send(DECAF_Callback_Params* params)
{
	// AWH uint32_t addr = params->ns.addr;
	int size = params->ns.size;
	uint8_t * buf = params->ns.buf;
	uint32_t conn_id = 0;

	/* If no data, return */
	if ((buf == NULL) || (size == 0))
		return;

	struct ip *iph = (struct ip *) (buf + 14);
	struct tcphdr *tcph = (struct tcphdr *) (buf + 34);
	//struct udphdr *udph = (struct udphdr*)(buf+34);

	if (!taint_nic_state || // Ignore if not tainting NIC
			buf[12] != 0x08 || buf[13] != 0x0) // Ignore non-IP packets
		return;

	/* TCP */
	if (iph->ip_p == 6) {
		/* If it is a SYN-ACK packet, create a new inbound connection */
		/* This is slightly preferred over creating the connection when
        the SYN is received */
		if ((tcph->th_flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK)) {
			conn_id = compute_conn_id(iph, tcph, NULL);
			tracing_add_tcp_conn(conn_id, ntohl(tcph->th_ack));

			if (tracenetlog) {
				uint32_t origin = tracing_get_tcp_origin(conn_id);
				fprintf(tracenetlog,
						"New inbound TCP flow. ID: %u Origin: %u %s:%d-->%s:%d\n",
						conn_id, origin, inet_ntoa(iph->ip_dst), ntohs(tcph->th_dport),
						inet_ntoa(iph->ip_src), ntohs(tcph->th_sport));
				fflush(tracenetlog);
			}
		}
	}

	return;
}

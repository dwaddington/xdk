/*
   Copyright (c) 2001-2004 Swedish Institute of Computer Science. 
   All rights reserved. 
   
   Redistribution and use in source and binary forms, with or without modification, 
   are permitted provided that the following conditions are met: 
   
   1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer. 
   2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution. 
   3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission. 
   
   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS AND ANY EXPRESS OR IMPLIED 
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
   SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
   OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
   IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
   OF SUCH DAMAGE.
*/

/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  
*/

/*
  Author(s):
  @author Jilong Kuang (jilong.kuang@samsung.com)
  @author Won Jeon (won.jeon@samsung.com)
*/

#ifndef __MICRO_TCP_TCP_H__
#define __MICRO_TCP_TCP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef int8_t 		s8_t;
typedef int16_t 	s16_t;
typedef int32_t 	s32_t;
typedef uint8_t 	u8_t;
typedef uint16_t 	u16_t;
typedef uint32_t 	u32_t;

typedef s8_t err_t;
typedef u16_t tcpflags_t;
typedef u16_t tcpwnd_size_t;
typedef u32_t sys_prot_t;

typedef enum {
	PBUF_TRANSPORT,
	PBUF_IP,
	PBUF_LINK,
	PBUF_RAW
} pbuf_layer;

typedef enum {
	PBUF_RAM,
	PBUF_ROM,
	PBUF_REF,
	PBUF_POOL
} pbuf_type;

struct pbuf2 {
	struct pbuf2 *next;
	void *payload;
	u16_t tot_len;
	u16_t len;
	u8_t type;
	u8_t flags;
	u16_t ref;
};

/** indicates this is a custom pbuf: pbuf_free and pbuf_header handle such a pbuf differently */
#define PBUF_FLAG_IS_CUSTOM	0x02U
/** indicates this pbuf includes a TCP FIN flag */
#define PBUF_FLAG_TCP_FIN	0x20U

#define ERR_OK					0 		/* No error, everyhing OK.		*/
#define ERR_MEM					-1		/* Out of memory error.				*/
#define ERR_BUF					-2		/* Buffer error.							*/
#define ERR_TIMEOUT			-3		/* Timeout.										*/
#define ERR_RTE					-4		/* Routing problem.						*/
#define ERR_INPROGRESS	-5		/* Operation in progress.			*/
#define ERR_VAL					-6		/* Illegal value.							*/
#define ERR_WOULDBLOCK	-7		/* Operation would be block.	*/
#define ERR_USE					-8		/* Address in use.						*/
#define ERR_ISCONN			-9		/* Already connected.					*/

#define ERR_IS_FATAL(e)	((e) < ERR_ISCONN)

#define ERR_ABRT				-10		/* Connection aborted.				*/
#define ERR_RST					-11		/* Connection reset.					*/
#define ERR_CLSD				-12		/* Connection closed.					*/
#define ERR_CONN				-13		/* Not connected.							*/
#define ERR_ARG					-14		/* Illegal argument.					*/
#include "./micro-tcp/stack/exo_stack.h"

class Exo_stack; // forward declaration

typedef void (Exo_stack::*sys_timeout_handler)(void *arg);

struct sys_timeo {
	struct sys_timeo *next;
	u32_t time;
	sys_timeout_handler h;
	void *arg;
};

//TODO: should be moved to protocol.h or ip.h later
#define IP4_ADDR(ipaddr, a,b,c,d) \
	(ipaddr)->addr = ((u32_t)((a) & 0xff) << 24) | \
									 ((u32_t)((b) & 0xff) << 16) | \
									 ((u32_t)((c) & 0xff) << 8)  | \
									  (u32_t)((d) & 0xff)

/** Gets an IP pcb option (SOF_* flags) */
#define ip_get_option(pcb, opt) ((pcb)->so_options & (opt))
/** Sets an IP pcb option (SOF_* flags) */
#define ip_set_option(pcb, opt)	((pcb)->so_options |= (opt))
/** Source IP4 address of current_header */
#define ip_current_src_addr() 	(&_ip_data.current_iphdr_src)
/** Destination IP4 address of current_header */
#define ip_current_dest_addr()	(&_ip_data.current_iphdr_dest)
#define ip_2_ip(ipaddr)	(ipaddr)

#define IPH_VHL_SET(hdr, v, hl) (hdr)->_v_hl = (((v) << 4) | (hl))

#define SOF_ACCEPTCONN	0x02U	/* socket has had listen() */
#define SOF_REUSEADDR		0x04U	/* allow local address reuse */
#define SOF_KEEPALIVE		0x08U	/* keep connections alive */
#define SOF_LINGER			0x80U	/* linger on close if data present */

/* These flags are inherited (e.g. from a listen->pcb to a connection-pcb): */
#define SOF_INHERITED	(SOF_REUSEADDR|SOF_KEEPALIVE|SOF_LINGER)

#define NUM_TCP_PCB_LISTS 4
#define TCP_DEFAULT_LISTEN_BACKLOG	0xff

#define TCP_LOCAL_PORT_RANGE_START	0xc000
#define TCP_LOCAL_PORT_RANGE_END		0xffff

#define TCP_KEEPINTVL_DEFAULT	75000UL	/* Default Time between KEEPALIVE probes in milliseconds */
#define TCP_KEEPCNT_DEFAULT	9U	/* Default Counter for KEEPALIVE probes */

/* Maximum KEEPALIVE probe time */
#define TCP_MAXIDLE		TCP_KEEPCNT_DEFAULT * TCP_KEEPINTVL_DEFAULT

#define TCP_KEEP_INTVL(pcb)	((pcb)->keep_intvl)
#define TCP_KEEP_DUR(pcb)	TCP_MAXIDLE

#define TCP_KEEPIDLE_DEFAULT	7200000UL
#define TCP_FLAGS 0x3fU

#define TCPH_HDRLEN(phdr) (ntohs((phdr)->_hdrlen_rsvd_flags) >> 12)
#define TCPH_FLAGS(phdr) (ntohs((phdr)->_hdrlen_rsvd_flags) & TCP_FLAGS)

#define TCPH_FLAGS_SET(phdr, flags) (phdr)->_hdrlen_rsvd_flags = (((phdr)->_hdrlen_rsvd_flags & (u16_t)(~(u16_t)(TCP_FLAGS))) | htons(flags))
#define TCPH_HDRLEN_FLAGS_SET(phdr, len, flags)	(phdr)->_hdrlen_rsvd_flags = htons(((len) << 12) | (flags))
#define TCPH_SET_FLAG(phdr, flags ) (phdr)->_hdrlen_rsvd_flags = ((phdr)->_hdrlen_rsvd_flags | htons(flags))

#define TCP_TCPLEN(seg) ((seg)->len + (((TCPH_FLAGS((seg)->tcphdr) & (TCP_FIN | TCP_SYN)) != 0) ? 1U : 0U))

#define TCP_SEQ_LT(a,b)		((s32_t)((u32_t)(a) - (u32_t)(b)) < 0)
#define TCP_SEQ_LEQ(a,b)	((s32_t)((u32_t)(a) - (u32_t)(b)) <= 0)
#define TCP_SEQ_GT(a,b)		((s32_t)((u32_t)(a) - (u32_t)(b)) > 0)
#define TCP_SEQ_GEQ(a,b)	((s32_t)((u32_t)(a) - (u32_t)(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c) (TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

#define TCP_BUILD_MSS_OPTION(mss) htonl(0x02040000 | ((mss) & 0xFFFF))

#define MIN(x, y)	((x) << (y)) ? (x) : (y)

#define TCP_PRIO_NORMAL	64

/* TCP_MSS: TCP Maximum segment size (default is 536, a conservertive default,
 * you might want to increase this.)
 * For the receive side, this MSS is advertised to the remote side
 * when opening a connection. For the transmit size, this MSS sets
 * an upper limit on the MSS advertised by the remote host.
 */
#define TCP_MSS	536

/* TCP_SND_BUF: TCP sender buffer space (bytes)
 * To achieve good performance, this should be at least 2 * TCP_MSS
 */
#define TCP_SND_BUF (2 * TCP_MSS)

/* TCP_TTL: Default Time-To-Live value */
#define TCP_TTL (IP_DEFAULT_TTL)

/* TCP_WND: The size of a TCP window. This must be at least
 * (2 * TCP_MSS) for things to work well
 */
#define TCP_WND	(4 * TCP_MSS)

#define TCPWND_MAX	0xFFFFFFFFU

#define TCP_TMR_INTERVAL 250 /* The TCP timer interval in milliseconds */

#define TCP_SLOW_INTERVAL (2 * TCP_TMR_INTERVAL) /* the coarse grained timeout in milliseconds */

#define TCP_FIN_WAIT_TIMEOUT	20000	/* milliseconds */
#define TCP_SYN_RCVD_TIMEOUT	20000 /* milliseconds */

#define TCP_OOSEQ_TIMEOUT	6U	/* x RTO */

#define TCP_MSL	60000UL	/* The maximum segment lifetime in milliseconds */

#define TCP_SND_QUEUELEN			((4 * (TCP_SND_BUF) + (TCP_MSS) - 1)/(TCP_MSS))

#define tcp_sndbuf(pcb)				((pcb)->snd_buf)
#define tcp_sndqueuelen(pcb)	((pcb)->snd_queuelen)
	/*
	 * This is the Nagle algorithm: try to combine user data to send as few TCP
   * segments as possible. Ony send if
   * - no previously transmitted data on the connection remains unacknowledged or
   * - the TF_NODELAY flag is set (nagle algorithm turned off for this pcb) or
   * - the only unsent segment is at least pcb->mss bytes long (or there is more
   *   than one unsent segment - with lwIP, this can happen although unsent->len < mss)
   * - or if we are in fast-retransmit (TF_INFR)
   */
#define  tcp_do_output_nagle(tpcb) ((((tpcb)->unacked == NULL) || \
					((tpcb)->flags & (TF_NODELAY | TF_INFR)) || \
					(((tpcb)->unsent != NULL) && (((tpcb)->unsent->next != NULL) || \
					((tpcb)->unsent->len >= (tpcb)->mss))) || \
					((tcp_sndbuf(tpcb) == 0) || (tcp_sndqueuelen(tpcb) >= TCP_SND_QUEUELEN)) \
					) ? 1 : 0)

/* TCP_WND_UPDATE_HRESHOLD: difference in window to trigger an
 * explicit window update
 */
#define TCP_WND_UPDATE_THRESHOLD (TCP_WND / 4)

#define tcp_ack(pcb) \
	PLOG("tcp_ack: starts."); \
	do { \
		if ((pcb)->flags & TF_ACK_DELAY) { \
			(pcb)->flags &= ~TF_ACK_DELAY; \
			(pcb)->flags |= TF_ACK_NOW; \
		} \
		else { \
			(pcb)->flags |= TF_ACK_DELAY; \
		} \
	} while (0); \
	PLOG("tcp_ack: ends.");

#define tcp_ack_now(pcb)				\
	PLOG("tcp_ack_now: starts."); \
	do {													\
		(pcb)->flags |= TF_ACK_NOW;	\
	} while (0); \
	PLOG("tcp_ack_now: ends.");
	

#define TCP_FIN	0x01U
#define TCP_SYN	0x02U
#define TCP_RST	0x04U
#define TCP_PSH	0x08U
#define TCP_ACK	0x10U
#define TCP_URG	0x20U
#define TCP_ECE	0x40U
#define TCP_CWR	0x80U

/* Length of the TCP header, excluding options. */
#define TCP_HLEN 20

/* Function prototype for tcp receive callback functions. Called when data has
 * been received.
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb which received data
 * @param p The received data (or NULL when the connection has been closed!)
 * @param err An error code if there has been an error receiving
 * 						Only return ERR_ABRT if you have called tcp_abort from within the
 * 						callback function!
 */
typedef err_t (Exo_stack::*tcp_recv_fn)(void *arg, struct tcp_pcb *tpcb, struct pbuf2 *p, err_t err);
/** Function prototype for tcp accept callback functions. Called when a new
 * connection can be accepted on a listening pcb.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param newpcb The new connection pcb
 * @param err An error code if there has been an error accepting.
 * 						Only return ERR_ABRT if you have called tcp_abort from within the
 * 						callback function!
 */
typedef err_t (Exo_stack::*tcp_accept_fn)(void *arg, struct tcp_pcb *newpcb, err_t err);
/** Function prototype for tcp sent callback functions. Called when sent data has
 * been acknowledged by the remote side. Use it to free corresponding resouces.
 * This also means that the pcb has now space available to send new data.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb for which data has been acknowledged
 * @param len The amount of bytes acknowledged
 * @return ERR_OK: try to send some data by calling tcp_output
 * 				 Only return ERR_ABRT if you have called tcp_abrt from within the
 * 				 callback function!
 */
typedef err_t (Exo_stack::*tcp_sent_fn)(void *arg, struct tcp_pcb *tpcb, u16_t len);
/** Function prototype for tcp connected callback functions. Called when a pcb
 * is connected to the remote side after initiating a connection attempt by
 * calling tcp_connect().
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb The connection pcb when is connected
 * @param err An unused error code, always ERR_OK currently ;-) TODO!
 * 						Only return ERR_ABRT if you have called tcp_abort from within the
 * 						callback function!
 *
 * @note When a connection attempt fails, the error callback is currently called!
 */
typedef err_t (Exo_stack::*tcp_connected_fn)(void *arg, struct tcp_pcb *tpcb, err_t err);
/** Function prototype for tcp error callback functions. Called when the pcb
 * receives a RST or is unexpectedly closed for any other reason.
 *
 * @note The corresponding pcb is already freed when this callback is called!
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param err Error code to indicate why the pcb has been closed
 * 						ERR_ABORT: aborted through tcp_abrt or by a TCP timer
 * 						ERR_RST: the connection was reset by the remote host
 */
typedef err_t (Exo_stack::*tcp_err_fn)(void *arg, err_t err);
/** Function prototype for tcp poll callback functions. Called periodically as
 * specified by @see tcp_poll.
 *
 * @param arg Additional argument to pass to the callback function (@see tcp_arg())
 * @param tpcb tcp pcb
 * @return ERR_OK: try to send some data by calling tcp_output
 * 				 Only returns ERR_ABRT if you have called tcp_abort from within the
 * 				 callback function!
 */
typedef err_t (Exo_stack::*tcp_poll_fn)(void *arg, struct tcp_pcb *tpcb);

/** TCP header */
struct tcp_hdr {
	u16_t src;
	u16_t dest;
	u32_t seqno;
	u32_t ackno;
	u16_t _hdrlen_rsvd_flags;
	u16_t wnd;
	u16_t chksum;
	u16_t urgp;
} __attribute__((packed));

#define RCV_WND_SCALE(pcb, wnd) (wnd)
#define SND_WND_SCALE(pcb, wnd) (wnd)

enum tcp_state {
	CLOSED			= 0,
	LISTEN			= 1,
	SYN_SENT		= 2,
	SYN_RCVD		= 3,
	ESTABLISHED	= 4,
	FIN_WAIT_1	= 5,
	FIN_WAIT_2	= 6,
	CLOSE_WAIT	= 7,
	CLOSING			= 8,
	LAST_ACK		= 9,
	TIME_WAIT		= 10
};

/* This structure represents a TCP segment on the unsent, unacked and ooseq queues */
struct tcp_seg2 {
	struct tcp_seg2 *next;		/* used when putting segments on a queue */
	struct pbuf2 *p;					/* buffer containing data + TCP header */
	u16_t len;						/* the TCP length of this segment */
	u16_t oversize_left;	/* Extra bytes available at the end of the last
															pbuf in unsent (used for asserting vs.
															tcp_pcb.unsent_oversized only) */
	u8_t flags;
#define TF_SEG_OPTS_MSS					(u8_t)0x01U	/* Include MSS option */
#define TF_SEG_OPTS_TS					(u8_t)0x02U	/* Include timestamp option */
#define TF_SEG_DATA_CHECKSUMMED	(u8_t)0x04U	/* All data (not the header) is
																										checksummed into 'chksum' */
#define TF_SEG_OPTS_WND_SCALE		(u8_t)0x08U	/* Include WND SCALE option */

	struct tcp_hdr *tcphdr;	/* the TCP header */
};

struct tcp_seg {
	struct tcp_seg *next;
	struct pbuf *p;
	u16_t len;
	u16_t oversize_left;
	u8_t flags;
	tcp_hdr *tcphdr;
};

#define EXO_TCP_OPT_LEN_MSS	4
#define EXO_TCP_OPT_LEN_TS	0
#define EXO_TCP_OPT_LEN_WS	0

#define EXO_TCP_OPT_LENGTH(flags) \
	(flags & TF_SEG_OPTS_MSS				? EXO_TCP_OPT_LEN_MSS : 0) + \
	(flags & TF_SEG_OPTS_TS					? EXO_TCP_OPT_LEN_TS  : 0) + \
	(flags & TF_SEG_OPTS_WND_SCALE	? EXO_TCP_OPT_LEN_WS  : 0)

#define TF_RESET	(u8_t)0x08U		/* Connection was reset. */
#define TF_CLOSED	(u8_t)0x10U		/* Connection was successfully closed. */
#define TF_GOT_FIN	(u8_t)0x20U		/* Connection was closed by the remote end. */

struct tcp_pcb {
	/* common PCB members */
	IP_PCB;
	/* protocol specific PCB members */
	struct tcp_pcb *next;

	void *callback_arg;
	tcp_accept_fn accept;
	enum tcp_state state; /* TCP state */

	u8_t prio;

	s16_t local_port; 

	/* ports are in host byte order */
	u16_t remote_port;
	tcpflags_t flags;

#define TF_ACK_DELAY 		((tcpflags_t)0x0001U) /* Delayed ACK */
#define TF_ACK_NOW			((tcpflags_t)0x0002U)	/* Immediate ACK */
#define TF_INFR					((tcpflags_t)0x0004U)	/* In fast recovery */
#define TF_TIMESTAMP		((tcpflags_t)0x0008U) /* Timestamp option enabled */
#define TF_RXCLOSED			((tcpflags_t)0x0010U)	/* rx closed by tcp_shutdown */
#define TF_FIN					((tcpflags_t)0x0020U)	/* Connection was closed locally (FIN segment enqueued */
#define TF_NODELAY			((tcpflags_t)0x0040U) /* Disable Nagle algorithm */
#define TF_NAGLEMEMERR	((tcpflags_t)0x0080U)	/* nagle enabled, memerr, try to output to prevent delayed ACK to happen */

	/* Timers */
	u8_t polltmr, pollinterval;
	u8_t last_timer;
	u32_t tmr;

	/* receiver variables */
	tcpwnd_size_t rcv_wnd; /* receiver window available */
	tcpwnd_size_t rcv_ann_wnd; /* receiver window to announce */

	/* receiver variables */
	u32_t rcv_nxt; /* next seqno expected */
	u32_t rcv_ann_right_edge; /* announced right edge of window */

	/* Retransmission timer */
	s16_t rtime;

	u16_t mss; /* maximum segment size */

	/* RTT (round trip time) setimation variables */
	u32_t rttest;	/* RTT estimate in 500ms ticks */
	u32_t rtseq;		/* sequence number being timed */
	s16_t sa, sv; /* @todo document this */

	s16_t rto; /* retransmission time-out */
	u8_t nrtx;	/* number of retransmissions */

	/* fast retransmit/recovery */
	u8_t dupacks;
	u32_t lastack; /* Highest acknowledged seqno */

	/* congestion avoidance/conrol variables */
	tcpwnd_size_t cwnd;
	tcpwnd_size_t ssthresh;

	/* sender variables */
	u32_t snd_nxt; /* next new seqno to be sent */
	u32_t snd_wl1, snd_wl2; /* Sequence and acknowledgement numbers of last
																window update */
	u32_t snd_lbb; /* Sequence number of next byte to be buffered */
	tcpwnd_size_t snd_wnd; /* sender window */
	tcpwnd_size_t snd_wnd_max; /* the maximum sender window announced by the remote host */

	tcpwnd_size_t acked;

	tcpwnd_size_t snd_buf; /* Available buffer space for sending (in bytes) */
#define TCP_SNDQUEUELEN_OVERFLOW (0xffffU-3)
	u16_t snd_queuelen; /* Available buffer space for sending (in pbufs) */

	/* Exra bytes available at the end of the last pbuf in unsent. */
	u16_t unsent_oversize;

	/* These are ordered by sequence number: */
	struct tcp_seg2 *unsent;	/* Unsent (queued) segments */
	struct tcp_seg2 *unacked;	/* Sent but unacknowledged segments */
	struct tcp_seg2 *ooseq;	/* Received out of sequence segments. */

	struct pbuf2 *refused_data;	/* Data previously received but not yet taken by upper layer */

	/* Function to be called when more send buffer space is available. */
	tcp_sent_fn sent;
	/* Function to be called when (in-sequence) data has arrived. */
	tcp_recv_fn recv;
	/* Function to be called when a connection has been set up. */
	tcp_connected_fn connected;
	/* Function which is called periodically. */
	tcp_poll_fn poll;
	/* Function to be called whenever a fatal error occurs. */
	tcp_err_fn errf;

	/* idle time before KEEPALIVE is sent */
	u32_t keep_idle;
	u32_t keep_intvl;
	u32_t keep_cnt;

	/* Persist timer counter */
	u8_t persist_cnt;
	/* Persist timer back-off */
	u8_t persist_backoff;

	/* KEEPALIVE counter */
	u8_t keep_cnt_sent;
};

typedef struct tcp_pcb tcp_pcb_t;

struct tcp_pcb_listen {
/* Common members of all PCB types */
	IP_PCB;

	struct tcp_pcb_listen *next;
	void *callback_arg;
	tcp_accept_fn accept;
	enum tcp_state state;
	u8_t prio;
	u16_t local_port;

	u8_t accept_any_ip_version;
};

/*
union tcp_listen_pcbs_t {
	//struct tcp_pcb_listen *listen_pcbs;
	struct tcp_pcb *pcbs;
};
*/

/* Flags for "apiflags" parameter in tcp_write */
#define TCP_WRITE_FLAG_COPY 0x01
#define TCP_WRITE_FLAG_MORE	0x02

#define TCP_PRIO_MIN		1
#define TCP_PRIO_NORMAL	64
#define TCP_PRIO_MAX		127

/**
 * TCP_SYNMAXRTX: Maximum number of retransmissions of SYN segments.
 */
#define TCP_SYNMAXRTX		6

/**
 * TCP_MAXRTX: Maximum number of retransmissions of data segments.
 */
#define TCP_MAXRTX			12

#define TCP_EVENT_ACCEPT(pcb,err,ret) \
	do { \
		if ((pcb)->accept != NULL) \
			(ret) = (this->*pcb->accept)((pcb)->callback_arg,(pcb),(err)); \
		else (ret) = ERR_ARG; \
	} while (0)

#define TCP_EVENT_SENT(pcb,space,ret) \
	do { \
		if ((pcb)->sent != NULL) \
			(ret) = (this->*pcb->sent)((pcb)->callback_arg,(pcb),(space)); \
		else (ret) = ERR_OK; \
	} while (0)


#define TCP_EVENT_RECV(pcb,p,err,ret) \
	do { \
		if ((pcb)->recv != NULL) { \
			(ret) = (this->*pcb->recv)((pcb)->callback_arg,(pcb),(p),(err)); \
		} else { \
			(ret) = this->tcp_recv_null(NULL, (pcb), (p), (err)); \
		} \
	} while (0)
		
#define TCP_EVENT_CLOSED(pcb,ret) \
	do { \
		if (((pcb)->recv != NULL)) { \
			(ret) = (this->*pcb->recv)((pcb)->callback_arg,(pcb),NULL,ERR_OK); \
		} else { \
			(ret) = ERR_OK; \
		} \
	} while (0)

#define TCP_EVENT_CONNECTED(pcb,err,ret) \
	do { \
		if ((pcb)->connected != NULL) \
			(ret) = (this->*pcb->connected)((pcb)->callback_arg,(pcb),(err)); \
		else (ret) = ERR_OK; \
	} while (0)

#define TCP_EVENT_POLL(pcb,ret) \
	do { \
		if ((pcb)->poll != NULL) \
			(ret) = (this->*pcb->poll)((pcb)->callback_arg,(pcb)); \
		else (ret) = ERR_OK; \
	} while (0)

#define TCP_EVENT_ERR(errf,arg,err) \
	do { \
		if ((errf) != NULL) \
			(this->*errf)((arg),(err)); \
	} while (0)

#define PBUF_LINK_HLEN	14
#define PBUF_IP_HLEN		20
#define SIZEOF_STRUCT_PBUF2	sizeof(struct pbuf2)
#define SIZEOF_STRUCT_PBUF	sizeof(struct pbuf)
#define PBUF_TRANSPORT_HLEN	20

#define PBUF_FLAG_PUSH	0x01U

#define ETHADDR32_COPY(src, dst)	memcpy(src, dst, ETHARP_HWADDR_LEN)

#define IPADDR_ANY	((u32_t)0x00000000UL)

#define TCP_DATA_COPY(dst, src, len, seg) memcpy(dst, src, len)
#define TCP_DATA_COPY2(dst, src, len, chksum, chksum_swapped) memcpy(dst, src, len)

#define TCP_OVERSIZE	TCP_MSS

#ifdef __cplusplus
}
#endif

#endif // __MICRO_TCP_TCP_H__


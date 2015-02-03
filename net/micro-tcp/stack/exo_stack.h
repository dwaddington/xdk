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

#ifndef _EXO_STACK_H_
#define _EXO_STACK_H_

#include <libexo.h>
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include <x540/driver_config.h>
#include <x540/x540_types.h>
#include <network_stack/protocol.h>
#include <network_stack/tcp.h>
#include <shm_channel.h>

using namespace Exokernel;

typedef Shm_channel<struct pbuf, CHANNEL_SIZE, SPMC_circular_buffer> Shm_channel_t;

class Exo_stack {

  INic * _inic;
  IStack * _istack;
  IMem * _imem;
  int _index; // corresponding to NIC driver index

  union {
    struct {
      unsigned pkt;
      cpu_time_t timer;
      unsigned accum_pkt;
    } ctr;
    char padding[CACHE_LINE_SIZE];
  } recv_counter[NUM_RX_QUEUES];

  struct etharp_entry arp_table[ARP_TABLE_SIZE];
  uint8_t mac[6];
  struct eth_addr ethaddr;
  uint8_t ip[4];
  uint16_t udp_port;
  uint16_t remote_port;
  ip_addr_t ip_addr;
  ip_addr_t remote_ip_addr;
  struct ip_reassdata *reassdatagrams[NUM_RX_THREADS_PER_NIC];
  struct udp_pcb *udp_pcbs[NUM_RX_THREADS_PER_NIC];
  uint16_t tx_core[NUM_TX_THREADS_PER_NIC];

  Shm_channel_t * _net_side_channel[NUM_CHANNEL];

	/* TCP member variables */

	//static const ip_addr_t ip_addr_any;

	struct tcp_pcb *_tcp_pcbs[NUM_RX_THREADS_PER_NIC];

	// Arrray with all (non-temporary) PCB lists, mainly used for smaller 
	// code size
	struct tcp_pcb ** _tcp_pcb_lists[4];

	union tcp_listen_pcbs_t {
		struct tcp_pcb_listen *listen_pcbs;
		struct tcp_pcb *pcbs;
	};
	/* List of all TCP PCBS in LISTEN state */
	union tcp_listen_pcbs_t _tcp_listen_pcbs;
	/* List of all TCP PCBs bound but not yet (connected || listening) */
	struct tcp_pcb *_tcp_bound_pcbs;
	/* List of all TCP PCBs that are in a state in which they accept or 
	 * send data */
	/*static*/ struct tcp_pcb *_tcp_active_pcbs;
	/* List of all TCP PCBs in TIME-WAIT state */
	/*static*/ struct tcp_pcb *_tcp_tw_pcbs;

	u32_t _tcp_ticks;
	u8_t _tcp_timer_ctr;
	/*static*/ int _tcpip_tcp_timer_active;
	struct tcp_pcb *_tcp_tmp_pcb; // for TCP_RMV

	u16_t _tcp_port;

	u8_t _tcp_active_pcbs_changed;

	struct tcp_pcb *_tcp_input_pcb;

	u8_t _tcp_timer;

	/*static*/ struct sys_timeo *_next_timeout;

	/*const static*/ u8_t _tcp_backoff[13];
	/*const static*/ u8_t _tcp_persist_backoff[7];

	u32_t _seqno, _ackno;
	u8_t _flags;
	u16_t _tcplen;
	u8_t _recv_flags;
	struct pbuf2 *_recv_data;

	struct tcp_seg2 _inseg;
	struct tcp_hdr *_tcphdr;

	/** The IP header ID of the next outgoing IP packet */
	u16_t _ip_id;

	/* Global variables of this module, kept in a struct for efficient access using base+index. */
	struct ip_globals
	{
		const struct ip_hdr *current_ip4_header;
		u16_t current_ip_header_tot_len;
		ip_addr_t current_iphdr_src;
		ip_addr_t current_iphdr_dest;
	};
	/** Global data for both IPv4 and IPv6 */
	struct ip_globals _ip_data;

	const static char* _tcp_state_str[];

	int _exoprot_count;
	pthread_t _exoprot_thread;
	pthread_mutex_t _exoprot_mutex;

public:
  /** 
    * Constructor
    * 
    */
  Exo_stack(IStack * istack, INic * inic, IMem * imem, unsigned index) {
    assert(inic);
    assert(istack);
    assert(imem);

    _inic = inic;
    _istack = istack;
    _imem = imem;
    _index = index;

    init_param();

    setup_tx_threads();
		
    setup_channels();
  }

  /* stack functions */
  void init_param();
  void send_pkt_test(unsigned queue);
  void send_udp_pkt_test(unsigned queue);
  void setup_tx_threads();
  ip_addr_t* get_remote_ip() { return &remote_ip_addr; }
  INic * get_nic_component() { return _inic; }
  void setup_channels();
  Shm_channel_t * get_channel(unsigned id) { return _net_side_channel[id]; }
  /**
  * To free a pbuf list associated memory.
  *
  * @param pbuf_list The pointer to the pbuf struct.
  * @param flag True means to free packet buffer and pbuf meta data; False means to free pbuf meta data only.
  */
  void free_packets(struct pbuf* pbuf_list, bool flag);
  
  /* stack component APIs*/
  pkt_status_t ethernet_input(struct exo_mbuf ** p, size_t cnt, unsigned queue);
  status_t ethernet_output_simple(struct exo_mbuf **p, size_t& cnt, unsigned queue);
  status_t ethernet_output(struct exo_mbuf **p, size_t& cnt, unsigned queue);

  /* ARP functions */
  pkt_status_t arp_input(uint8_t *pkt);
  void update_arp_entry(ip_addr_t *ipaddr, struct eth_addr *ethaddr);
  int find_arp_entry(ip_addr_t *ipaddr);
  void arp_output(ip_addr_t *ipdst_addr);
  void send_arp_reply(uint8_t *pkt);
  int get_entry_in_arp_table(ip_addr_t* ip_addr);
  void send_arp(uint8_t *pkt, uint32_t length, unsigned tx_queue, bool is_arp_reply);

  /* IP functions */
  pkt_status_t ip_input(uint8_t *pkt, uint16_t len, unsigned queue);
  pkt_status_t icmp_input(uint8_t *pkt);
  void send_ping_reply(uint8_t *pkt);
  bool is_sent_to_me(uint8_t *pkt); 
  struct pbuf * ip_reass(struct pbuf *pkt, pkt_status_t * t, unsigned queue);
  struct ip_reassdata* ip_reass_enqueue_new_datagram(struct ip_hdr *fraghdr, unsigned queue);
  bool ip_reass_chain_frag_into_datagram_and_validate(struct ip_reassdata *ipr, struct pbuf *new_p, pkt_status_t * t);
  void ip_reass_dequeue_datagram(struct ip_reassdata *ipr, struct ip_reassdata *prev, unsigned queue);

  /* UDP functions */
  pkt_status_t udp_input(struct pbuf *pkt, unsigned queue);
  void udp_send_pkt(uint8_t *vaddr, addr_t paddr, uint32_t udp_payload_len, unsigned queue);
  void udp_bind(ip_addr_t *ipaddr, uint16_t port);

	/* TCP functions */
	void tcp_server_test(unsigned queue);
	struct tcp_pcb *tcp_new(void);
	struct tcp_pcb *tcp_alloc(uint8_t prio);
	u32_t tcp_next_iss(void);
	err_t tcp_recv_null(void *arg, struct tcp_pcb *pcb, struct pbuf2 *p, err_t err);
	err_t tcp_bind(struct tcp_pcb *pcb, ip_addr_t *ipaddr, u16_t port);

#define TCP_REG(pcbs, npcb) \
	do { \
		(npcb)->next = *pcbs; \
		*(pcbs) = (npcb); \
		tcp_timer_needed(); \
	} while (0)

	void tcp_timer_needed(void);

typedef void (Exo_stack::*sys_timeout_handler)(void *arg);

	void sys_timeout(u32_t msecs, sys_timeout_handler, void *arg);

	/*static*/ void tcpip_tcp_timer(void *arg);

	struct tcp_pcb *tcp_listen_with_backlog(struct tcp_pcb *pcb, u8_t backlog);
#define tcp_listen(pcb) tcp_listen_with_backlog(pcb, TCP_DEFAULT_LISTEN_BACKLOG)


#define TCP_RMV(pcbs, npcb) \
	do { \
		assert(pcbs); \
		if (*(pcbs) == (npcb)) { \
			*(pcbs) = (*pcbs)->next; \
		} else { \
			for (_tcp_tmp_pcb = *(pcbs); _tcp_tmp_pcb != NULL; _tcp_tmp_pcb = _tcp_tmp_pcb->next) { \
				if (_tcp_tmp_pcb->next == (npcb)) { \
					_tcp_tmp_pcb->next = (npcb)->next; \
					break; \
				} \
			} \
			(npcb)->next = NULL; \
		} \
	} while (0)

	/*static*/ err_t tcp_accept_null(void *arg, struct tcp_pcb *pcb, err_t err);

typedef err_t (Exo_stack::*tcp_accept_fn)(void *arg, struct tcp_pcb *newpcb, err_t err);

	void tcp_accept(struct tcp_pcb *pcb, tcp_accept_fn accept);
	/*static*/ s8_t server_accept(void *arg, struct tcp_pcb *pcb, err_t err);
	void tcp_setprio(struct tcp_pcb *pcb, u8_t prio);
	void tcp_arg(struct tcp_pcb *pcb, void *arg);
	err_t server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf2 *p, err_t err);
	//err_t server_sent(void *arg, struct tcp_pcb *pcb, u16_t len);
	void server_close(struct tcp_pcb *pcb);
	void tcp_recv(struct tcp_pcb *pcb, tcp_recv_fn recv);
	void tcp_recved(struct tcp_pcb *pcb, u16_t len);
	void tcp_sent(struct tcp_pcb *pcb, tcp_sent_fn sent);
	err_t tcp_close(struct tcp_pcb *pcb);
	err_t tcp_close_shutdown(struct tcp_pcb *pcb, uint8_t rst_on_unacked_data);
	err_t server_err(void *arg, err_t err);
	void tcp_err(struct tcp_pcb *pcb, tcp_err_fn err);
	err_t server_poll(void *arg, struct tcp_pcb *pcb);
	void tcp_poll(struct tcp_pcb *pcb, tcp_poll_fn poll, u8_t interval);
#define tcp_accepted(pcb) \
	PLOG("[EXO-LIB]: tcp_accepted:starts"); \
	assert(pcb->state != LISTEN); \
	PLOG("[EXO-LIB]: tcp_Accepted:ends")

	u32_t tcp_update_rcv_ann_wnd(struct tcp_pcb *pcb);
	void tcp_rst(u32_t seqno, u32_t ackno,
												ip_addr_t *local_ip, ip_addr_t *remote_ip,
												u16_t local_port, u16_t remote_port);
	void tcp_pcb_purge(struct tcp_pcb *pcb);

#define TCP_RMV_ACTIVE(npcb) \
	do { \
		TCP_RMV(&_tcp_active_pcbs, npcb); \
		_tcp_active_pcbs_changed = 1; \
	} while (0)

	void tcp_pcb_remove(struct tcp_pcb **pcblist, struct tcp_pcb *pcb);

#define TCP_PCB_REMOVE_ACTIVE(pcb) \
	do { \
		tcp_pcb_remove(&_tcp_active_pcbs, pcb); \
		_tcp_active_pcbs_changed = 1; \
	} while (0)

	err_t tcp_send_fin(struct tcp_pcb *pcb);
	void tcp_segs_free(struct tcp_seg2 *seg);
	void tcp_seg_free(struct tcp_seg2 *seg);

	pkt_status_t tcp_input(struct pbuf2 *p, unsigned queue);
	err_t tcp_process_refused_data(struct tcp_pcb *pcb);

	void tcp_client_test(unsigned queue);

typedef err_t (Exo_stack::*tcp_connected_fn)(void *arg, struct tcp_pcb *tpcb, err_t err);

	err_t tcp_connect(struct tcp_pcb *pcb, ip_addr_t *ipaddr, u16_t port, tcp_connected_fn connected);
	err_t client_connected(void *arg, struct tcp_pcb *pcb, err_t err);
	u16_t tcp_new_port(void);
	err_t tcp_enqueue_flags(struct tcp_pcb *pcb, u8_t flags);

#define TCP_REG_ACTIVE(npcb) \
	do { \
		TCP_REG(&_tcp_active_pcbs, npcb); \
		_tcp_active_pcbs_changed = 1; \
	} while (0)

	err_t tcp_output(struct tcp_pcb *pcb);
	struct tcp_seg2 *tcp_create_segment(struct tcp_pcb *pcb, struct pbuf2 *p, u8_t flags, u32_t seqno, u8_t optflags);
	err_t tcp_send_empty_ack(struct tcp_pcb *pcb);
	void tcp_output_segment(struct tcp_seg2 *seg, struct tcp_pcb *pcb);
	u16_t tcp_eff_send_mss(u16_t sendmss, ip_addr_t *dest);
	void tcp_build_timestamp_option(struct tcp_pcb *pcb, u32_t *opts);

	void tcp_tmr(void);
	void tcp_fasttmr(void);
	void tcp_slowtmr(void);
	void tcp_zero_window_probe(struct tcp_pcb *pcb);
	void tcp_rexmit_rto(struct tcp_pcb *pcb);
	void tcp_keepalive(struct tcp_pcb *pcb);
	int16_t tcp_pcbs_sane(void);

	void tcp_kill_timewait(void);
	void tcp_abort(struct tcp_pcb *pcb);
	void tcp_abandon(struct tcp_pcb *pcb, int reset);
	void tcp_kill_prio(u8_t prio);
	void tcp_debug_print(struct tcp_hdr *tcphdr);

	pbuf2 *pbuf2_alloc(pbuf_layer layer, u16_t length, pbuf_type type);
	u8_t pbuf2_free(struct pbuf2 *p);
	u8_t pbuf2_header(struct pbuf2 *p, s16_t header_size_increment);
	u8_t pbuf2_clen(struct pbuf2 *p);
	void pbuf2_cat(struct pbuf2 *h, struct pbuf2 *t);
	void pbuf2_realloc(struct pbuf2 *p, u16_t new_len);
	u16_t pbuf2_copy_partial(struct pbuf2 *buf, void *dataptr, u16_t len, u16_t offset);

	err_t ip_output(struct pbuf2 *p, ip_addr_t *src, ip_addr_t *dest, u8_t ttl, u8_t tos, u8_t proto);
	void ip_debug_print(struct pbuf2 *p);
	err_t etharp_output(struct pbuf2 *q, ip_addr_t *ipaddr);
	err_t etharp_send_ip(struct pbuf2 *p, struct eth_addr *src, struct eth_addr *dst);
	err_t linkoutput(struct pbuf2 *p);
#define ip_addr_isbroadcast(ipaddr)	ip4_addr_isbroadcast((ipaddr)->addr)
	u8_t ip4_addr_isbroadcast(u32_t addr);
#define ip4_addr_set_u32(dest_ipaddr, src_u32)	((dest_ipaddr)->addr = (src_u32))
#define ip_addr_isany(addr1) ((addr1) == NULL || (addr1)->addr == IPADDR_ANY)
	err_t tcp_listen_input(struct tcp_pcb_listen *pcb);
	err_t tcp_process(struct tcp_pcb *pcb);
	void tcp_debug_print_flags(u8_t flags);
	void tcp_parseopt(struct tcp_pcb *pcb);
	void tcp_debug_print_state(enum tcp_state s);
	err_t client_sent(void *arg, struct tcp_pcb *pcb, u16_t len);
	void client_close(struct tcp_pcb *pcb);
	err_t tcp_write(struct tcp_pcb *pcb, const void *arg, u16_t len, u8_t apiflags);
	pbuf2* tcp_output_alloc_header(struct tcp_pcb *pcb, u16_t optlen, u16_t datalen, u32_t seqno_be /* already in network byte order */);
	err_t tcp_write_checks(struct tcp_pcb *pcb, u16_t len);	
	void tcp_receive(struct tcp_pcb *pcb);
	void tcp_rexmit(struct tcp_pcb *pcb);
	void tcp_rexmit_fast(struct tcp_pcb *pcb);
	err_t tcp_timewait_input(struct tcp_pcb *pcb);
	struct pbuf2* tcp_pbuf2_prealloc(pbuf_layer layer, u16_t length, 
			u16_t max_length, u16_t *oversize, struct tcp_pcb *pcb, u8_t apiflags,
			u8_t first_seg);	

	sys_prot_t sys_arch_protect(void);
	void sys_arch_unprotect(sys_prot_t pval);
};

//const u8_t _tcp_backoff[13] = { 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7};

//const u8_t _tcp_persist_backoff[7] = { 3, 6, 12, 24, 48, 96, 120 };

const char* Exo_stack::_tcp_state_str[] = { 
	"CLOSED",
	"LISTEN",
	"SYN_SENT",
	"SYN_RCVD",
	"ESTABLISHED",
	"FIN_WAIT_1",
	"FIN_WAIT_2",
	"CLOSE_WAIT",
	"CLOSING",
	"LAST_ACK",
	"TIME_WAIT"
};

#endif // _EXO_STACK_H_

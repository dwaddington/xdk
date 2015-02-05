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
*/

#ifndef __EXO_STACK_H__
#define __EXO_STACK_H__

#include <libexo.h>
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include <x540/driver_config.h>
#include <x540/x540_types.h>
#include <net/udp.h>
#include <net/msg_processor.h>
#include <x540/xml_config_parser.h>

using namespace Exokernel;

namespace Exokernel {

class Exo_stack {

  INic * _inic;
  IStack * _istack;
  IMem * _imem;
  int _index; // corresponding to NIC driver index
  Msg_processor * _msg_processor;
  Config_params * _params;

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
  uint64_t cpu_freq;
  ip_addr_t ip_addr;
  ip_addr_t remote_ip_addr;
  struct ip_reassdata *reassdatagrams[NUM_RX_THREADS_PER_NIC];
  struct udp_pcb *udp_pcbs[NUM_RX_THREADS_PER_NIC];
  unsigned rx_core[NUM_RX_THREADS_PER_NIC];
  struct exo_mbuf ** mbuf[NUM_TX_THREADS_PER_NIC]; 

  union {
    uint16_t ctr;
    char padding[CACHE_LINE_SIZE];
  } ip_iden_counter[NUM_TX_QUEUES];
  uint16_t ip_iden_space_per_queue;
  uint16_t ip_iden_base[NUM_TX_QUEUES];

  unsigned stats_num;
  unsigned client_rx_flow_num;
  unsigned server_port;

public:
  /** 
    * Constructor
    * 
    */
  Exo_stack(IStack * istack, INic * inic, IMem * imem, 
            unsigned index, Msg_processor * msg, Config_params * params) {
    assert(inic);
    assert(istack);
    assert(imem);

    _params = params;

    stats_num = _params->stats_num;
    client_rx_flow_num = _params->client_rx_flow_num;
    server_port = _params->server_port;
    remote_port = _params->client_port;

    std::string client_ip;
    client_ip = _params->client_ip[index];
    add_ip((char *)client_ip.c_str());

    _inic = inic;
    _istack = istack;
    _imem = imem;
    _index = index;
    _msg_processor = msg;   

    init_param();
  }

  /* stack functions */
  void init_param();
  void add_ip(char* myip);
  void parse_ip_format(char *s, uint8_t * ip);
  void send_pkt_test(unsigned queue);
  void send_udp_pkt_test(unsigned queue);
  ip_addr_t* get_remote_ip() { return &remote_ip_addr; }
  INic * get_nic_component() { return _inic; }
  IMem * get_mem_component() { return _imem; }
  Msg_processor * get_msg_processor() { return _msg_processor; }
  unsigned get_rx_core(unsigned idx) { return rx_core[idx]; }
  /**
  * To free a pbuf list associated memory.
  *
  * @param pbuf_list The pointer to the pbuf struct.
  * @param flag True means to free packet buffer and pbuf meta data; False means to free pbuf meta data only.
  */
  void free_packets(pbuf_t* pbuf_list, bool flag);
  
  /* stack component interfaces*/
  pkt_status_t ethernet_input(uint8_t *pkt, size_t len, unsigned queue);
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
  pbuf_t * ip_reass(pbuf_t *pkt, pkt_status_t * t, unsigned queue);
  struct ip_reassdata* ip_reass_enqueue_new_datagram(struct ip_hdr *fraghdr, unsigned queue);
  bool ip_reass_chain_frag_into_datagram_and_validate(struct ip_reassdata *ipr, pbuf_t *new_p, pkt_status_t * t);
  void ip_reass_dequeue_datagram(struct ip_reassdata *ipr, struct ip_reassdata *prev, unsigned queue);

  /* UDP functions*/
  pkt_status_t udp_input(pbuf_t *pkt, unsigned queue);
  void udp_bind(ip_addr_t *ipaddr, uint16_t port);

  /**
   * To send UDP packets.
   *
   * @param vaddr Virtual address of the UDP payload.
   * @param paddr Physical address of the UDP payload.
   * @param udp_payload_len The length of UDP payload.
   * @param queue The TX queue where the packets are sent.
   * @param recycle packet memory recycle flag.
   * @param allocator_id The memory allocator id.
   */
  void udp_send_pkt(uint8_t *vaddr, addr_t paddr, unsigned udp_payload_len, unsigned queue, 
                    bool recycle, unsigned allocator_id);
};

}
#endif

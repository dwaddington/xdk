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
   @author Changhui Lin (changhui.lin@samsung.com)
*/

#ifndef __RAW_DEVICE_H__
#define __RAW_DEVICE_H__

#include <libexo.h>
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include <network_stack/protocol.h>
#include "driver_config.h"
#include "x540_types.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
//#include <netinet/ip.h>
//#include <netinet/udp.h>


using namespace Exokernel;

typedef status_t (*recv_packets_fn)(pkt_buffer_t * p, size_t cnt, unsigned device, unsigned queue);

class Raw_device {

public:

	INic * _inic;
	IStack * _istack;
	IMem * _imem;
	unsigned _index;
	bool rx_start_ok;

	int _sockfd_send, _sockfd_recv;
	struct ifreq _if_idx, _if_mac;
	struct sockaddr_ll _socket_addr_send, _socket_addr_recv;
	struct ether_addr _lcl_ether_addr, _rmt_ether_addr;
	//recv_packets_fn _recv_pckets; /*this function is called by the network device driver to pass a packet up to the IP stack*/

	uint16_t rx_core[NUM_RX_THREADS_PER_NIC];

	/*nic parameters*/
	uint8_t _mac[6];
	struct eth_addr _ethaddr;
	uint8_t _ip[4];
	ip_addr_t _ip_addr;

	/** 
	 * Constructor
	 */
	Raw_device(INic * inic, IStack * istack, IMem * imem, unsigned index) {
		assert(inic);
		assert(istack);
		assert(imem);
		_inic = inic;
		_istack = istack;
		_imem = imem;
		_index = index;

		_inic->set_comp_state(NIC_CREATED_STATE, index); // Memory component can start
	}

	/**
	 *  Initialization
	 */   
	void init_device(char *if_name, char* lcl_ether_addr, char* rmt_ether_addr, char *lcl_ip);
	//void init_nic_param();

  /**
    *  Send packets using simple tx descriptor. One packet per descriptor. 
    *
    *  @param tx_pkts The pointer for the packet buffer chain.
    *  @param nb_pkts The number of packets to be sent.
    *  @param queue The TX queue.
    *  @return The actual number of packets that have been sent.
    */
  uint16_t send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned queue);

  /**
    *  Send packets using advanced tx descriptor. Each packet can consists of up to 3 segments. IP checksum offload support.
    *
    *  @param tx_pkts The pointer for the packet buffer chain.
    *  @param nb_pkts The number of packets to be sent.
    *  @param queue The TX queue.
    *  @return The actual number of packets that have been sent.
    */
  uint16_t multi_send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned queue);

	/**
	 * Receive packets
	 */
	uint16_t recv(struct exo_mbuf ** rx_pkts, size_t cnt, unsigned queue);

	void recv_handler(unsigned tid);

	void setup_rx_threads();

	void add_ip(char * ip, uint8_t * ip_array);
	void parse_ip_format(char *s, uint8_t * ip);
	unsigned rx_queue_2_core(unsigned queue);

};

#endif // __RAW_DEVICE_H__

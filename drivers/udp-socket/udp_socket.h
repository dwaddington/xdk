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

#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__

#include <libexo.h>
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include <network_stack/protocol.h>
#include "driver_config.h"
#include "x540_types.h"
#include <spmc_circbuffer.h>
#include <channel/shm_channel.h>
#include "../../app/kvcache/job_desc.h"
#include "../../network_stack/protocol.h"
#include "xml_config_parser.h"
#include "evict_support.h"

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

#define MAX_VIRTUAL 8

using namespace Exokernel;
using namespace KVcache;

typedef SPMC_circular_buffer<addr_t, CHANNEL_SIZE> channel_t;
typedef Shm_channel<job_descriptor_t, CHANNEL_SIZE, SPMC_circular_buffer> Shm_channel_t;

class UDP_socket {

public:

	INic * _inic;
//	IStack * _istack;
	IMem * _imem;
	unsigned _index;
	bool rx_start_ok;

	int _sockfd_send[NUM_TX_THREADS_PER_NIC], _sockfd_recv[NUM_RX_THREADS_PER_NIC];
	struct sockaddr_in _socket_addr_send[NUM_TX_THREADS_PER_NIC], _socket_addr_recv[NUM_RX_THREADS_PER_NIC];

	channel_t* _buffer[NUM_RX_THREADS_PER_NIC];
	Shm_channel_t *_net_side_channel[NUM_CHANNEL];

#define MAX_N_PORT 8
	unsigned server_port[MAX_N_PORT];
	char server_ip[MAX_N_PORT][32];

	uint16_t rx_core[CPU_NUM];
	uint16_t tx_core[CPU_NUM];

	List<Frames_list_node> __pending_frame_lists[NUM_TX_THREADS_PER_NIC]; 

	/*multiple msg - mmsg*/
	typedef struct {
		struct mmsghdr msgvec[VLEN];
		unsigned len;
		struct iovec iovecs[VLEN][MAX_FRAG];
		struct sockaddr_in msg_name_v[VLEN];
		socklen_t *sock_len_v[VLEN];
		char padding[CACHE_LINE_SIZE];
	} mmsghdr_t;
	mmsghdr_t mmsghdr_recv[NUM_CHANNEL], mmsghdr_send[NUM_CHANNEL];

	/*statistics*/
	union {
		struct {
			unsigned pkt;
			cpu_time_t timer;
			unsigned accum_pkt;
		} ctr;
		char padding[CACHE_LINE_SIZE];
	} recv_counter[NUM_RX_QUEUES], send_counter[NUM_TX_QUEUES];

	uint64_t cpu_freq_khz;
	unsigned _channel_size;
	unsigned _channels_per_nic;
	unsigned _request_rate;
	unsigned _eviction_threshold;
	unsigned _eviction_period;
	unsigned _server_timestamp;

	/** 
	 * Constructor
	 */
	UDP_socket(INic * inic, IMem * imem, unsigned index) {
		assert(inic);
		assert(imem);
		_inic = inic;
		_imem = imem;
		_index = index;

		_inic->set_comp_state(NIC_CREATED_STATE, index); // Memory component can start
	}

	/**
	 *  Initialization
	 */   
	void init_device();

	void recv_handler(unsigned tid);
	unsigned recv_single(unsigned tid);
	unsigned recv_burst(unsigned tid);

	void send_handler(unsigned tid);
	unsigned send_single(unsigned tid);
	unsigned send_burst(unsigned tid);

	void setup_rx_threads();
	void setup_tx_threads();

	void read_config();

	inline unsigned rx_queue_2_core(unsigned queue);

	void free_packets(pbuf_t* pbuf_list, bool flag);

	/*Ugly. Hardcode to adapt to allocator type defined in memory_itf.h
	 *      Instead, numa_allocator should be able to handle this.
	 **/
	inline unsigned cvt_alloc(unsigned alloc_type) {
		switch(alloc_type){
			case DESC_ALLOCATOR:
				return 0;
			case PACKET_ALLOCATOR:
				return 1;
			case JOB_DESC_ALLOCATOR:
				return 2;
			case PBUF_ALLOCATOR:
				return 3;
			case FRAME_LIST_ALLOCATOR:
				return 4;
		}
	}
};

#endif // __UDP_SOCKET_H__

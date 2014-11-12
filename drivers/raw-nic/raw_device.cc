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


#include "raw_device.h"
#include "raw_threads.h"
#include <unistd.h>
#include <netinet/in.h>
#include <emmintrin.h>
#include "dump_mcd.h"

void Raw_device::init_device(char *if_name, char* lcl_ether_addr, char* rmt_ether_addr, char *lcl_ip) {
	while(_imem->get_comp_state(0) < MEM_READY_STATE) { sleep(1); }

	_lcl_ether_addr = *(ether_aton(lcl_ether_addr));
	_rmt_ether_addr = *(ether_aton(rmt_ether_addr));

	//init_nic_param();
	//TODO  FIXME 
	for(unsigned idx = 0; idx < ETHER_ADDR_LEN; idx++) {
		_ethaddr.addr[idx] = _lcl_ether_addr.ether_addr_octet[idx];
		_mac[idx]          = _lcl_ether_addr.ether_addr_octet[idx];
	}
	add_ip(lcl_ip, _ip);

	printf("Local MAC:\t");
	for(unsigned idx = 0; idx < ETHER_ADDR_LEN; idx++) 
		printf("%x:", _lcl_ether_addr.ether_addr_octet[idx]);/*Local MAC*/
	printf("\n");

	printf("Remote MAC:\t");
	for(unsigned idx = 0; idx < ETHER_ADDR_LEN; idx++) 
		printf("%x:", _rmt_ether_addr.ether_addr_octet[idx]);/*Destination MAC*/
	printf("\n");

	if ((_sockfd_send = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
		perror("socket() error for send");
		exit(-1);
	} else {
		printf("socket() OK for send\n");
	}

	/* Get the index of the interface to send/recv on */
	memset(&_if_idx, 0, sizeof(struct ifreq));
	strncpy(_if_idx.ifr_name, if_name, IFNAMSIZ-1);
	if (ioctl(_sockfd_send, SIOCGIFINDEX, &_if_idx) < 0)
		perror("SIOCGIFINDEX");
	/* Get the MAC address of the interface to send/recv on */
	memset(&_if_mac, 0, sizeof(struct ifreq));
	strncpy(_if_mac.ifr_name, if_name, IFNAMSIZ-1);
	if (ioctl(_sockfd_send, SIOCGIFHWADDR, &_if_mac) < 0)
		perror("SIOCGIFHWADDR");

	/* Prepare socket address with Destination MAC -- for send*/
	memset(&_socket_addr_send, 0, sizeof(_socket_addr_send));
	_socket_addr_send.sll_ifindex = _if_idx.ifr_ifindex;/*Index of network device*/
	_socket_addr_send.sll_halen = ETH_ALEN;            /*Address length*/
	//for(unsigned idx = 0; idx < ETHER_ADDR_LEN; idx++) 
	//	_socket_addr_send.sll_addr[idx] = _rmt_ether_addr.ether_addr_octet[idx];/*Destination MAC*/
	memcpy(_socket_addr_send.sll_addr, &_rmt_ether_addr, ETHER_ADDR_LEN);

	//////////////////////////////////////////
	// RAW socket for recv
	/////////////////////////////////////// 
	if ((_sockfd_recv = socket(AF_PACKET, SOCK_RAW,  htons(ETH_P_IP))) == -1) {
		perror("socket() error for recv");
		exit(-1);
	} else {
		printf("socket() OK for recv\n");
	}

	/* Prepare socket address -- Local */
	memset(&_socket_addr_recv, 0, sizeof(_socket_addr_recv));
	_socket_addr_recv.sll_family = AF_PACKET;          /*Address Farmily*/
	_socket_addr_recv.sll_ifindex = _if_idx.ifr_ifindex;/*Index of network device*/
	_socket_addr_recv.sll_protocol = htons(ETH_P_IP);  /*Protocol*/
	_socket_addr_recv.sll_halen = ETH_ALEN;          /*Address length - not used*/
	memcpy(_socket_addr_recv.sll_addr, &_lcl_ether_addr, ETHER_ADDR_LEN);

	/* Set interface to promiscuous mode - do we need to do this every time? */
	struct ifreq ifopts;
	strncpy(ifopts.ifr_name, if_name, IFNAMSIZ-1);
	ioctl(_sockfd_recv, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(_sockfd_recv, SIOCSIFFLAGS, &ifopts);

	/* Bind to device */
	if (bind(_sockfd_recv, (struct sockaddr *)&_socket_addr_recv, sizeof(struct sockaddr_ll)) < 0) {
		printf("bind() error : %d : %s\n", errno, strerror(errno));
		exit(-1);
	} else {
		printf("bind() OK!\n");
	}

	setup_rx_threads();

	printf("\n=============== Nic Initialization done! ==============\n\n");
}

void Raw_device::setup_rx_threads() {

	PINF("Creating RX threads ...");

	Raw_thread** raw_threads = new Raw_thread*[NUM_RX_THREADS_PER_NIC];
	assert(raw_threads);

	/* derive cpus for RX threads from RX_THREAD_CPU_MASK */
	for (unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
		rx_core[i] = 0;
	}

	uint16_t pos = 1;
	unsigned n = 0;
	for (unsigned i = 0; i < CPU_NUM; i++) {
		pos = 1 << i;
		if ((RX_THREAD_CPU_MASK & pos) != 0) {
			rx_core[n] = i;
			n++;
		}
	}
#if 0
	printf("RX Thread assigned core: ");
	for (unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
		printf("%d ",rx_core[i]);
	}
	printf("\n");
#endif

	for(unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {

		unsigned global_id = i + _index * NUM_RX_THREADS_PER_NIC;
		unsigned core_id = rx_core[i] + _index * CPU_NUM;
		unsigned local_id = i;
		raw_threads[i]= new Raw_thread(this, local_id, global_id, core_id);
		PLOG("RX Thread[%d] is running on core %d", global_id , core_id);
		assert(raw_threads[i]);
		raw_threads[i]->activate();
	}
}

/** 
 * Full-feature packet send. Each frame consists of multiple segments (up to 3). IP checksum offloading.
 * 
 * @param tx_pkts 
 * @param nb_pkts
 * @param tx_queue
 */
uint16_t Raw_device::multi_send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned tx_queue) {

	printf("multi_send() Not implemented!!!\n");
	return 0;
}


/** 
 * Simple packet send. One packet per descriptor. No offloading.
 * 
 * @param tx_pkts 
 * @param nb_pkts
 * @param tx_queue
 */
uint16_t Raw_device::send(struct exo_mbuf ** tx_pkts, size_t nb_pkts, unsigned tx_queue) {

	unsigned idx=0;
	for(idx = 0; idx < nb_pkts; idx++) {

		struct exo_mbuf *mbuf = tx_pkts[idx];

#if 0
		char *tmp = (char *)(mbuf->virt_addr);
		printf("\n======= in Raw_device:send() ======\n");
		for (unsigned i=0; i<mbuf->len; i++) printf("%02x ", tmp[i]);
		printf("\n");
#endif

		if(sendto(_sockfd_send, (void *)mbuf->virt_addr, mbuf->len, 0, (struct sockaddr *)&_socket_addr_send, sizeof(_socket_addr_send)) < 0) {
			perror("sendto() error");
			break;
		} else {
			printf("sendto() ok.\n");
			_imem->free((void *)mbuf->virt_addr, PACKET_ALLOCATOR, _index);
			_imem->free((void *)mbuf, MBUF_ALLOCATOR, _index);
		}
	} //for
	return idx;
}


uint16_t Raw_device::recv(struct exo_mbuf ** rx_pkts, size_t cnt, unsigned queue) {

	static unsigned count = 0;

	printf("** Waiting for pkts ......\n");

	unsigned idx_pkts;
	for(idx_pkts = 0; idx_pkts < cnt; idx_pkts++) {

		void * tmp_mem;
		unsigned core_idx = rx_queue_2_core(queue);

		if (_imem->alloc((addr_t *)&tmp_mem, MBUF_ALLOCATOR, _index, core_idx) != Exokernel::S_OK) {
			panic("MBUF_ALLOCATOR failed!\n");
		}
		assert(tmp_mem);
		struct exo_mbuf *mbuf = (exo_mbuf *)tmp_mem; 

		if (_imem->alloc((addr_t *)&tmp_mem, PACKET_ALLOCATOR, _index, core_idx) != Exokernel::S_OK) {
			panic("PACKET_ALLOCATOR failed!\n");
		}
		assert(tmp_mem);
		void *pkt = (void *)tmp_mem;

		socklen_t clen = sizeof(_socket_addr_recv);
		int rx_len = recvfrom(_sockfd_recv, pkt, PKT_MAX_SIZE, 0, (struct sockaddr *)&_socket_addr_recv, &clen);

		//dump_mcd_pkt_offset(pkt, 42);

		if(rx_len < 0) {
			printf("Recv Error\n");
			_imem->free((void *)pkt, PACKET_ALLOCATOR, _index);
			_imem->free((void *)mbuf, MBUF_ALLOCATOR, _index);
			exit(-1);
		} else {
			printf("Successfully received packets #%d\n", ++count);

			char *tmp = (char *)pkt;
			printf("\n======= in Raw_device:recv() ======\n");
			for (unsigned i=0; i<rx_len; i++) printf("%02x ", tmp[i]);
			printf("\n");

			//set metadata
			assert(pkt==tmp_mem);
			mbuf->phys_addr = (addr_t)_imem->get_phys_addr(pkt, PACKET_ALLOCATOR, _index);
			mbuf->virt_addr = (addr_t)pkt;
			mbuf->len = rx_len;
			mbuf->ref_cnt = 0;
			mbuf->flag = 0;
			mbuf->id = queue;
			mbuf->nb_segment = 1; 

			rx_pkts[idx_pkts]=mbuf;
		}
	}
	return idx_pkts;
}

/*called by thread entry function*/
/*recv packets, then send to the upper layer*/
void Raw_device::recv_handler(unsigned tid) {

	unsigned queue = (tid << 1); //do we need global???
	struct exo_mbuf *rx_pkts[MAX_PKT_BURST];

	/*only one packet for now*/
	size_t cnt = 1;
	cnt = recv(rx_pkts, cnt, tid);

	/*send packets to the upper network stack layer*/
	pkt_status_t t = (pkt_status_t) _istack->receive_packets((pkt_buffer_t *)rx_pkts, cnt, _index, queue);

	//TODO
	switch (t) {
		case KEEP_THIS_PACKET:
			//allocate a new packet buffer in the RX descriptor
			//new_pkt=true;
			break;
		case REUSE_THIS_PACKET:
			//reuse the old packet buffer in the RX descriptor
			break;
		default:
			PINF("[RX Queue %d] Found an error when processing a packet, error code (%x)\n",queue,t);
	}
}

void Raw_device::add_ip(char* myip, uint8_t *ip) {
	int len = strlen(myip);
	char s[len+1];
	__builtin_memcpy(s,myip,len);
	s[len]='\0';
	parse_ip_format(s,ip);
	printf("IP: %d.%d.%d.%d is added to NIC[%u]\n",ip[0],ip[1],ip[2],ip[3],_index);
	uint32_t temp=0;
	temp=(ip[0]<<24)+(ip[1]<<16)+(ip[2]<<8)+ip[3];
	_ip_addr.addr=ntohl(temp);
}

void Raw_device::parse_ip_format(char *s, uint8_t *ip) {
	char* tmp = s;
	int i=0;
	while (*tmp) {
		if (*tmp == '.') {
			*tmp='\0';
			ip[i++] = atoi(s);
			tmp++;
			s = tmp;
		}
		else {
			tmp++;
		}
	}
	if (i!=3)  {
		panic("IP address is wrong. [e.g. 105.144.29.xxx]");
	}
	ip[i]=atoi(s);
}

inline unsigned Raw_device::rx_queue_2_core(unsigned queue) {
	unsigned tid = queue >> 1;
	return (rx_core[tid] + _index * CPU_NUM);
}


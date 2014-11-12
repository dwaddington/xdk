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


#include "udp_socket.h"
#include "rx_threads.h"
#include "tx_threads.h"
#include <unistd.h>
#include <netinet/in.h>
#include <emmintrin.h>
#include "dump_mcd.h"

void UDP_socket::read_config() {

	/*FIXME. Also consider multiple devices*/
	server_port[0]=SERVER_PORT_UDP_0;
	server_port[1]=SERVER_PORT_UDP_1;
	server_port[2]=SERVER_PORT_UDP_2;
	server_port[3]=SERVER_PORT_UDP_3;
	server_port[4]=SERVER_PORT_UDP_4;
	server_port[5]=SERVER_PORT_UDP_5;
	server_port[6]=SERVER_PORT_UDP_6;
	server_port[7]=SERVER_PORT_UDP_7;

	strcpy(server_ip[0], SERVER_IP_UDP_0);
	strcpy(server_ip[1], SERVER_IP_UDP_1);
	strcpy(server_ip[2], SERVER_IP_UDP_2);
	strcpy(server_ip[3], SERVER_IP_UDP_3);
	strcpy(server_ip[4], SERVER_IP_UDP_4);
	strcpy(server_ip[5], SERVER_IP_UDP_5);
	strcpy(server_ip[6], SERVER_IP_UDP_6);
	strcpy(server_ip[7], SERVER_IP_UDP_7);
}

void UDP_socket::init_device() {
	while(_imem->get_comp_state(0) < MEM_READY_STATE) { sleep(1); }

	/*read from config file*/
	read_config();

	unsigned n_flow = NUM_CHANNEL;
	//////////////////////////////////////////
	// UDP socket for send 
	////////////////////////////////////////// 
	for(unsigned sock_idx = 0; sock_idx < n_flow; sock_idx++) {
		if ((_sockfd_send[sock_idx] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			perror("socket() error for send");
			exit(-1);
		} else {
			printf("socket() OK for send\n");
		}
		
		//TODO: bind to a local port if necessary
		
		__builtin_memset((char *)&_socket_addr_send[sock_idx], 0, sizeof(_socket_addr_send[sock_idx]));
		_socket_addr_send[sock_idx].sin_family = AF_INET;
		_socket_addr_send[sock_idx].sin_port = htons(server_port[sock_idx]);
		if (inet_aton(REMOTE_IP, &_socket_addr_send[sock_idx].sin_addr)==0) {
			perror("inet_aton() failed");
			exit(1);
		}

		if (connect(_sockfd_send[sock_idx], (struct sockaddr *) &_socket_addr_send[sock_idx], sizeof(_socket_addr_send[sock_idx])) == -1) {
			perror("connect() error!");
			exit(-1);
		}
	}//send socket

	//////////////////////////////////////////
	// UDP socket for recv
	//////////////////////////////////////////
	for(unsigned sock_idx = 0; sock_idx < n_flow; sock_idx++) {
		if ((_sockfd_recv[sock_idx] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			perror("socket() error for recv");
			exit(-1);
		} else {
			printf("socket() OK for recv\n");
		}

		printf("\nidx=%d, port=%d, ip=%s\n", 
				sock_idx, server_port[sock_idx], server_ip[sock_idx]);
		/* Prepare socket address -- Local */
		__builtin_memset(&_socket_addr_recv[sock_idx], 0, sizeof(_socket_addr_recv[sock_idx]));
		_socket_addr_recv[sock_idx].sin_family = AF_INET;
		_socket_addr_recv[sock_idx].sin_port = htons(server_port[sock_idx]);
		//_socket_addr_recv[sock_idx].sin_addr.s_addr = htonl(INADDR_ANY);
		_socket_addr_recv[sock_idx].sin_addr.s_addr = inet_addr(server_ip[sock_idx]);

		/* Bind to device */
		if (bind(_sockfd_recv[sock_idx], (struct sockaddr *)&_socket_addr_recv[sock_idx], sizeof(struct sockaddr_ll)) < 0) {
			printf("bind() error : %d : %s (idx=%d)\n", errno, strerror(errno), sock_idx);
			exit(-1);
		} else {
			printf("bind() OK! (idx=%d)\n", sock_idx);
		}

		/*set socket IP_PKTINFO*/
		/*
		int pktinfo=1;
		if (setsockopt(_sockfd_recv[sock_idx], SOL_IP, IP_PKTINFO, &pktinfo,
					sizeof(int)) == -1) {
			perror("setsockopt() error!!");
			exit(-1);
		}
		*/
	}//recv socket

	/*init other nic parameters*/
	for (unsigned i = 0; i < NUM_CHANNEL; i++) {
		__builtin_memset(&recv_counter[i], 0, sizeof(recv_counter[i]));
		__builtin_memset(&send_counter[i], 0, sizeof(send_counter[i]));
	}

	//cpu_freq_khz = 3070000;
	cpu_freq_khz = (uint64_t)(get_tsc_frequency_in_mhz() * 1000);
	printf("CPU Frequency = %lu MHZ\n", cpu_freq_khz/1000);

	for(unsigned i = 0; i < NUM_CHANNEL; i++) {
		_buffer[i] = new channel_t();
	}

	for(unsigned i = 0; i < NUM_CHANNEL; i++) {
		_net_side_channel[i] = new Shm_channel_t(i, true);
	}

	/*initialize the mmsg vector*/
	for(unsigned i = 0; i < NUM_CHANNEL; i++) {
		mmsghdr_t *msgs;
	  /*recv*/
		msgs = &(mmsghdr_recv[i]);
		__builtin_memset( msgs, 0, sizeof(mmsghdr_t) );
		for (unsigned j = 0; j < VLEN; j++) {
			//msgs->iovecs[j].iov_base         	= pkts[j]; //get memory at runtime
			for(unsigned k = 0; k < MAX_FRAG; k++)
				msgs->iovecs[j][k].iov_len          		= PKT_MAX_SIZE;

			msgs->msgvec[j].msg_hdr.msg_iov    	= msgs->iovecs[j];
			msgs->msgvec[j].msg_hdr.msg_iovlen 	= MAX_FRAG;
			msgs->msgvec[j].msg_hdr.msg_name		= &(msgs->msg_name_v[j]);
			msgs->sock_len_v[j] 						= &(msgs->msgvec[j].msg_hdr.msg_namelen);
			*(msgs->sock_len_v[j])							= sizeof(struct sockaddr_in);
		}
		/*send*/
		msgs = &(mmsghdr_send[i]);
		__builtin_memset( msgs, 0, sizeof(mmsghdr_t) );
		for (unsigned j = 0; j < VLEN; j++) {
			//msgs->iovecs[j].iov_base         	= pkts[j]; //get memory at runtime
			for(unsigned k = 0; k < MAX_FRAG; k++)
				msgs->iovecs[j][k].iov_len				= PKT_MAX_SIZE;

			msgs->msgvec[j].msg_hdr.msg_iov    	= msgs->iovecs[j];
			msgs->msgvec[j].msg_hdr.msg_iovlen 	= MAX_FRAG;
		}
	}

	_server_timestamp = SERVER_TIMESTAMP;
	_channel_size     = CHANNEL_SIZE;
	_channels_per_nic = NUM_CHANNEL;
	_eviction_period  = EVICTION_PERIOD;
	_request_rate     = REQUEST_RATE;
	_eviction_threshold = calculate_threshold(_channel_size, _channels_per_nic, 
			                                      _request_rate, _eviction_period);

	/*setup recv/send threads*/
	setup_rx_threads();
	setup_tx_threads();

	printf("\n=============== Initialization done! ==============\n\n");
}

void UDP_socket::setup_rx_threads() {

	PINF("Creating RX threads ...");

	Rx_thread** rx_threads = new Rx_thread*[NUM_RX_THREADS_PER_NIC];
	assert(rx_threads);

	/* derive cpus for RX threads from RX_THREAD_CPU_MASK */
	//obtain NUMA cpu mask
	struct bitmask * cpumask;
	cpumask = numa_allocate_cpumask();
	numa_node_to_cpus(_index, cpumask);

	for (unsigned i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
		rx_core[i] = 0;
	}

	std::string rx_threads_cpu_mask = std::string(RX_THREAD_CPU_MASK);
	Cpu_bitset thr_aff_per_node(rx_threads_cpu_mask);

	unsigned pos = 0;
	unsigned n = 0;
	for (unsigned i = 0; i < CPU_NUM; i++) {
		if (thr_aff_per_node.test(pos)) {
			rx_core[n] = get_cpu_id(cpumask,i+1);
			n++;
		}
		pos++;
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
		unsigned core_id = rx_core[i];
		unsigned local_id = i;
		rx_threads[i]= new Rx_thread(this, local_id, global_id, core_id);
		PLOG("RX Thread[%d] is running on core %d", global_id , core_id);
		assert(rx_threads[i]);
		rx_threads[i]->activate();
	}

	PINF("== RX threads DONE ==");
}

void UDP_socket::setup_tx_threads() {

	PINF("Creating TX threads ...");

	Tx_thread** tx_threads = new Tx_thread*[NUM_TX_THREADS_PER_NIC];
	assert(tx_threads);

	/* derive cpus for TX threads from TX_THREAD_CPU_MASK */
	//obtain NUMA cpu mask
	struct bitmask * cpumask;
	cpumask = numa_allocate_cpumask();
	numa_node_to_cpus(_index, cpumask);

	for (unsigned i = 0; i < NUM_TX_THREADS_PER_NIC; i++) {
		tx_core[i] = 0;
	}

	std::string tx_threads_cpu_mask = std::string(TX_THREAD_CPU_MASK);
	Cpu_bitset thr_aff_per_node(tx_threads_cpu_mask);

	unsigned pos = 0;
	unsigned n = 0;
	for (unsigned i = 0; i < CPU_NUM; i++) {
		if (thr_aff_per_node.test(pos)) {
			tx_core[n] = get_cpu_id(cpumask,i+1);
			n++;
		}
		pos++;
	}

#if 0
	printf("TX Thread assigned core: ");
	for (unsigned i = 0; i < NUM_TX_THREADS_PER_NIC; i++) {
		printf("%d ",tx_core[i]);
	}
	printf("\n");
#endif

	for(unsigned i = 0; i < NUM_TX_THREADS_PER_NIC; i++) {

		unsigned global_id = i + _index * NUM_TX_THREADS_PER_NIC;
		unsigned core_id = tx_core[i];
		unsigned local_id = i;
		tx_threads[i]= new Tx_thread(this, local_id, global_id, core_id);
		PLOG("TX Thread[%d] is running on core %d", global_id , core_id);
		assert(tx_threads[i]);
		tx_threads[i]->activate();
	}
	PINF("== TX threads DONE ==");
}


unsigned UDP_socket::send_single(unsigned tid) {
	unsigned sock_idx = tid;
	job_descriptor_t * jd;

	while (_net_side_channel[tid]->consume(&jd) != E_SPMC_CIRBUFF_OK) {
		cpu_relax();
	}
	void *pkt = (void*)(jd->protocol_frame);
	assert(pkt);
	_imem->free(jd, cvt_alloc(JOB_DESC_ALLOCATOR), _index);

#define TX_SEND
#ifdef TX_SEND
	//FIXME: need to get len here from jd
	unsigned pkt_len = PKT_LEN;
	if(sendto(_sockfd_send[sock_idx], pkt, pkt_len, 0, (struct sockaddr *)&_socket_addr_send[sock_idx], sizeof(_socket_addr_send[sock_idx])) < 0) {
		_imem->free((void *)pkt, cvt_alloc(PACKET_ALLOCATOR), _index);
		perror("sendto() error");
		exit(-1);
	}
#endif
	//printf("sendto() ok.\n");
	_imem->free((void *)pkt, cvt_alloc(PACKET_ALLOCATOR), _index);

	return 1;
}

unsigned UDP_socket::send_burst(unsigned tid) {

	unsigned sock_idx = tid;
	mmsghdr_t *msgs = &(mmsghdr_send[tid]);
	unsigned core_idx = rx_queue_2_core(tid); /*map to rx core*/
	msgs->len=0;
	job_descriptor_t *jd_v[VLEN];

	for(unsigned idx_pkt = 0; idx_pkt < VLEN; idx_pkt++) {
		job_descriptor_t *jd;
		while (_net_side_channel[tid]->consume(&jd) != E_SPMC_CIRBUFF_OK) {
			cpu_relax();
		}
		assert(jd);
		jd_v[idx_pkt] = jd; /* record to free later */

		/* prepare iovecs for send*/
		pbuf_t *pbuf_list = (pbuf_t*)(jd->protocol_frame);
		pbuf_t *pbuf = pbuf_list;
		assert(pbuf);

#define RUN_TERACACHE
#ifdef RUN_TERACACHE
		////////////////////////////////////////////////////////////
		// Processing Response 
		////////////////////////////////////////////////////////////

		if (_server_timestamp > 1)
			jd->ts_array[5] = rdtsc(); // timestamp after fetching from tx channel

		if (jd->command == GET || jd->command == GET_K) {
			if (jd->status == NO_ERROR.code) {
				assert(pbuf_list);
				uint8_t* pkt = pbuf_list->pkt;
				assert(pkt);
			} else {
				assert(pbuf_list == NULL);
			}

			/* recycle the original request frame */
			pbuf_t* discard_pbuf_list_0 = (pbuf_t*) jd->protocol_frame_discard_0;
			assert(discard_pbuf_list_0);

			pbuf_t* discard_pbuf_list_1 = (pbuf_t*) jd->protocol_frame_discard_1;
			assert(discard_pbuf_list_1 == NULL);
		} else if (jd->command == SET ||
				jd->command == ADD ||
				jd->command == REPLACE) {
			assert(pbuf_list == NULL);
			pbuf_t* discard_pbuf_list_1 = (pbuf_t*) jd->protocol_frame_discard_1;
			assert(discard_pbuf_list_1 == NULL);
		} else if (jd->command == DELETE) {
			assert(pbuf_list == NULL);
		} else {
			assert(pbuf_list == NULL);
		}

		////////////////////////////////////////////////////////////////
		// Eviction and Recycle Logic (Refer to micro-udp/tx_threads.h)
		////////////////////////////////////////////////////////////////
		// TODO: Add support for proactive (periodic) eviction. 
		// In this case, the pending frame lists will also be checked and removed
		// by a periodic auxiliary thread.

		if (operation_involves_eviction(jd->command) || jd->command == DELETE) {
			// Here we attempt to free discarded packets.
			// If some list of packets is still referenced (i.e., its reference counter > 0),
			// the list of packets is saved in the '__pending_frame_lists' for later removal.

			// NOTE:
			// We optimize for GET/GETK/DELETE requests. 
			// Here we free packets associated with operations that may lead to 
			// eviction (e.g., ADD, REPLACE, and SET). But we postpone freeing 
			// packets for GET/GETK/DELETE requests.  For those requests, the 
			// corresponding frames are freed regularly, after ensuring they have 
			// been sent by the NIC.

			pbuf_t* dpl[2]; // Pair of lists of frames to discard. 
			dpl[0] = (pbuf_t*) jd->protocol_frame_discard_0;
			dpl[1] = (pbuf_t*) jd->protocol_frame_discard_1;

			for (unsigned l = 0; l < 2; l++) {
				//PLOG("tid = %d: BEFORE INSERTING in pending list %d", _tid, l);
				if (dpl[l] != NULL) {
					pbuf_t* head = dpl[l];
					while (head != NULL) {
						pbuf_t* tail = head->last;
						pbuf_t* next_head = tail->next;
						if (next_head != NULL) {
							tail->next = NULL;
						}
						void * temp;
						if (_imem->alloc((addr_t *)&temp, cvt_alloc(FRAME_LIST_ALLOCATOR), 
									_index, core_idx) != Exokernel::S_OK) {
							panic("FRAME_LIST_ALLOCATOR failed!\n");
						}
						assert(temp);

						Frames_list_node* n = (Frames_list_node*) temp;
						n->frame_list = head;
						__pending_frame_lists[tid].insert(n);

						head = next_head;
					}
				}
				//PLOG("tid = %d: AFTER INSERTING in pending list %d", _tid, l);
			}

			// Deattach the discarded frames from the job descriptor. 
			// They won't be processed any further in the trasmit logic.
			jd->protocol_frame_discard_0 = NULL;
			jd->protocol_frame_discard_1 = NULL;
			//PLOG("tid = %d: BEFORE CLEANING pending list", _tid);

#if 0 //moved after sendmmsg()
			//Iterate over the list of pending frame list to see if they can be freed.
			Frames_list_node* cur = __pending_frame_lists[tid].head();
			Frames_list_node* next = NULL;
			while (cur != NULL) {
				next = cur->next();

				pbuf_t* frame_list = cur->frame_list;
				assert(frame_list != NULL);

				if (frame_list->ref_counter <= 0) {
					//PLOG("tid = %d: BEFORE freeing frame_list ", _tid);
					free_packets(frame_list, true);
					//PLOG("tid = %d: AFTER freeing frame_list ", _tid);
					cur->frame_list = NULL;

					__pending_frame_lists[tid].remove(cur);
					_imem->free(cur, FRAME_LIST_ALLOCATOR, _index);
				} else {
					// Do nothing.
				}
				cur = next;
			}
			// PLOG("tid = %d: AFTER CLEANING pending list", _tid);
#endif
		} else {
			/* GET and GETK command only */
			pbuf_t* discard_pbuf_list_0 = (pbuf_t*) jd->protocol_frame_discard_0;
			pbuf_t* discard_pbuf_list_1 = (pbuf_t*) jd->protocol_frame_discard_1;
			assert(discard_pbuf_list_0 != NULL);
			assert(discard_pbuf_list_1 == NULL);

			assert((jd->command == GET) || (jd->command == GET_K));
			free_packets(discard_pbuf_list_0, true); // On success or error, free the received packet anyway.
			jd->protocol_frame_discard_0 = NULL;
		}

		/* to this point, all discarded frames should be already freed */
		assert(jd->protocol_frame_discard_0 == NULL);      
		assert(jd->protocol_frame_discard_1 == NULL);     

#endif /////////////////// RUN_TERACACHE //////////////////

		/*UDP socket send*/
		unsigned idx_seg = 0;
		msgs->msgvec[idx_pkt].msg_len=0;
		msgs->msgvec[idx_pkt].msg_hdr.msg_iovlen=0;

		while(pbuf) {
			msgs->iovecs[idx_pkt][idx_seg].iov_base 	= pbuf->pkt;
			msgs->iovecs[idx_pkt][idx_seg].iov_len		= pbuf->ippayload_len;

			//((char *)pkt)[pkt_len]='\0';
			//printf("TX-%d: %s\n", tid, (char *)pkt);
			msgs->msgvec[idx_pkt].msg_len += pbuf->ippayload_len;
			msgs->msgvec[idx_pkt].msg_hdr.msg_iovlen += 1;

			pbuf = pbuf->next;
			idx_seg++;
		}
		//FIXME: need to drain when there are no enough pkts 
	}
	msgs->len=VLEN;

	//send
	int retval = sendmmsg(_sockfd_send[sock_idx], msgs->msgvec, msgs->len, 0);
	if (retval == -1) {
		perror("sendmmsg() error!!");
		exit(-1);
	}

#ifdef RUN_TERACACHE
	//Decrement reference counter for a frame list that was used in a response
	for(unsigned idx = 0; idx < msgs->len; idx++) {
		job_descriptor_t *jd = jd_v[idx];
		pbuf_t *pbuf_list = (pbuf_t *)jd->protocol_frame;
		assert(pbuf_list->ref_counter > 0); //new pbuf_list, handle ref_counter
		int64_t ref_c = __sync_sub_and_fetch(&(pbuf_list->ref_counter), 1);
	}

	//Iterate over the list of pending frame list to see if they can be freed.
	Frames_list_node* cur = __pending_frame_lists[tid].head();
	Frames_list_node* next = NULL;
	while (cur != NULL) {
		next = cur->next();

		pbuf_t* frame_list = cur->frame_list;
		assert(frame_list != NULL);

		if (frame_list->ref_counter <= 0) {
			//PLOG("tid = %d: BEFORE freeing frame_list ", _tid);
			free_packets(frame_list, true);
			//PLOG("tid = %d: AFTER freeing frame_list ", _tid);
			cur->frame_list = NULL;

			__pending_frame_lists[tid].remove(cur);
			_imem->free(cur, FRAME_LIST_ALLOCATOR, _index);
		} else {
			// Do nothing.
		}
		cur = next;
	}
	// PLOG("tid = %d: AFTER CLEANING pending list", _tid);
  
	//free packets used in iovecs
	for(unsigned idx = 0; idx < msgs->len; idx++) {
	
	}
#else 
	//free
	for(unsigned idx = 0; idx < msgs->len; idx++) {
		job_descriptor_t *jd = jd_v[idx];
		assert(jd);
		assert(jd->protocol_frame);

		free_packets((pbuf_t *)jd->protocol_frame, true);
		_imem->free(jd, cvt_alloc(JOB_DESC_ALLOCATOR), _index);
		//_imem->free((void *)(msgs->iovecs[idx].iov_base), cvt_alloc(PACKET_ALLOCATOR), _index);
	}
#endif
	return retval;
}

/*called by send thread entry function*/
void UDP_socket::send_handler(unsigned tid) {

	unsigned cnt;

#define SEND_MMSG
#ifdef SEND_MMSG
	cnt = send_burst(tid);
#else
	cnt = send_single(tid);
#endif

#define SEND_STAT 
#ifdef SEND_STAT 
	/************************** collect data (TX) **************************/
	if ( unlikely(send_counter[tid].ctr.pkt == 0) )
		send_counter[tid].ctr.timer = rdtsc(); /* timer reset */

	/* increment packet count */
	send_counter[tid].ctr.pkt += cnt;

#define INTERVAL 1000000
	unsigned core = tx_core[tid];
	if ( unlikely(send_counter[tid].ctr.pkt >= INTERVAL) ) {
		send_counter[tid].ctr.accum_pkt += send_counter[tid].ctr.pkt/1000;
		cpu_time_t finish_t = rdtsc();
		PLOG("[NIC %d TX %d @ CORE %d] TX %.2fK PPS (UDP) | %uK",
				_index,
				tid,
				core,
				(1.0 * cpu_freq_khz * send_counter[tid].ctr.pkt) / (finish_t - send_counter[tid].ctr.timer),
				send_counter[tid].ctr.accum_pkt);
		send_counter[tid].ctr.pkt=0;
	}
	/*************************DONE: collect data **************************/
#endif
}


unsigned UDP_socket::recv_single(unsigned tid) {

	unsigned sock_idx = tid;

	//printf("** Waiting for pkts ......\n");

	void * tmp_mem;
	unsigned core_idx = rx_queue_2_core(tid);
	if (_imem->alloc((addr_t *)&tmp_mem, cvt_alloc(PACKET_ALLOCATOR), _index, core_idx) != Exokernel::S_OK) {
		panic("PACKET_ALLOCATOR failed!\n");
	}
	assert(tmp_mem);
	void *pkt = (void *)tmp_mem;

	socklen_t clen = sizeof(_socket_addr_recv[sock_idx]);
	int rx_len = recvfrom(_sockfd_recv[sock_idx], pkt, PKT_MAX_SIZE, 0, (struct sockaddr *)&_socket_addr_recv[sock_idx], &clen);

	if( unlikely(rx_len < 0) ) {
		printf("Recv Error\n");
		_imem->free((void *)pkt, cvt_alloc(PACKET_ALLOCATOR), _index);
		exit(-1);
	} else {
		//((char*)pkt)[rx_len]=0;
		//printf("Data: %s\n", (char*)pkt);
		//count++;
		//printf("Successfully received packets #%d, rx_len = %d\n", count, rx_len);

		//dump_mcd_pkt_offset(pkt, 42);
	}

	//_imem->free((void *)pkt, cvt_alloc(PACKET_ALLOCATOR), _index);

	/* Construct the job descriptor to pass it to Memcached app*/
	void * temp;
	assert(_imem);
	if (_imem->alloc((addr_t *)&temp, cvt_alloc(JOB_DESC_ALLOCATOR), _index, core_idx) != Exokernel::S_OK) {
		panic("JOB_DESC_ALLOCATOR failed!\n");
	}
	assert(temp);
	job_descriptor_t * jd = (job_descriptor_t *) temp;
	__builtin_memset(jd, 0, sizeof(job_descriptor_t));

	//FIXME
	jd->protocol_frame = pkt;

	while (_net_side_channel[tid]->produce(jd) != E_SPMC_CIRBUFF_OK) {
		cpu_relax();
	}
	return 1; 
}


unsigned UDP_socket::recv_burst(unsigned tid) {

	unsigned sock_idx = tid;
	mmsghdr_t *msgs = &(mmsghdr_recv[tid]);
	struct timespec timeout;
	unsigned core_idx = rx_queue_2_core(tid);

	//printf("** Waiting for pkts ......\n");

	for (unsigned i = 0; i < VLEN; i++) {

		*(msgs->sock_len_v[i]) = sizeof(struct sockaddr_in);

		for(unsigned j = 0; j < MAX_FRAG; j++) {

			/* stop if there is already allocated memory */
			if(msgs->iovecs[i][j].iov_base != NULL) break; 

			void *tmp_mem;
			if(_imem->alloc((addr_t *)&tmp_mem, cvt_alloc(PACKET_ALLOCATOR), _index, core_idx) != Exokernel::S_OK) {
				panic("PACKET_ALLOCATOR failed!\n");
			}
			assert(tmp_mem);

			//msgs->msgvec[i].msg_hdr.msg_name = &(msgs->msg_name_v[i]);
			assert(msgs->msgvec[i].msg_hdr.msg_name);

			msgs->iovecs[i][j].iov_base    	= (void *)tmp_mem;
			msgs->iovecs[i][j].iov_len      = PKT_MAX_SIZE;
			//msgs->msgvec[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
			//__builtin_memset (&msgs->msg_name_v[i], 0, sizeof(struct sockaddr_in));
		}
	}
#if 0
	if(unlikely(_imem->get_num_avail_per_core(cvt_alloc(PACKET_ALLOCATOR), 0, core_idx)%10==0) ) {
		printf("********* core=%d, avail=%lu **********\n",
				core_idx,
				_imem->get_num_avail_per_core(cvt_alloc(PACKET_ALLOCATOR), 0, core_idx));
	}
#endif

#define TIMEOUT 1
	timeout.tv_sec = TIMEOUT;
	timeout.tv_nsec = 0;

	int recv_cnt = recvmmsg(_sockfd_recv[sock_idx], msgs->msgvec, VLEN, MSG_WAITFORONE,/*0*/ &timeout);
	if (recv_cnt == -1) {
		perror("recvmmsg() error!!");
		exit(-1);
	} else {
		//pkt[rx_len]=0;
		//printf("Data: %s", pkt);
		//count++;
		//printf("Received packets cnt=%d (sock_id=%d)\n", recv_cnt, sock_idx);

		//dump_mcd_pkt_offset(pkt, 42);
	}
	
	assert(_imem);
	for(unsigned i = 0; i < recv_cnt; i++) {

		/*make sure the size of allocated memory is large enough for large pkts*/
		assert(msgs->msgvec[i].msg_len <= PKT_MAX_SIZE * MAX_FRAG);

		//printf("msg_len = %d\n", msgs->msgvec[i].msg_len);
		
		void *temp;
		pbuf_t *pbuf_list = 0;
		unsigned msg_len = msgs->msgvec[i].msg_len;

		/*handle each payload segment*/
		for(unsigned j = 0; j < MAX_FRAG; j++) {

			unsigned payload_len;
			if(msg_len <= 0) break;
			if(msg_len >= msgs->iovecs[i][j].iov_len)	{
				payload_len = msgs->iovecs[i][j].iov_len;
			} else {
				payload_len = msg_len;
			}
			msg_len -= payload_len;

			/* allocate pbuf */
			if (_imem->alloc((addr_t *)&temp, cvt_alloc(PBUF_ALLOCATOR), _index, core_idx) != Exokernel::S_OK) {
				panic("PBUF_ALLOCATOR failed!\n");
			}
			pbuf_t *pbuf = (pbuf_t *)temp; 
			__builtin_memset(pbuf, 0, sizeof(pbuf_t));

			pbuf->pkt = (uint8_t*)(msgs->iovecs[i][j].iov_base);
			/*make it null, so we know a new memory is needed in this iovecs entry*/
			msgs->iovecs[i][j].iov_base = 0; 

			pbuf->ippayload_len = payload_len;
			//printf("(%d, %d) payload_len = %d\n", i, j, payload_len);

			//		printf("len = %d\n", pbuf->ippayload_len);
			//		char *pkt = (char*)pbuf->pkt;
			//		pkt[pbuf->ippayload_len]='\0';
			//		printf("pkt = %s\n", pkt);

			if(pbuf_list) {
				pbuf_list->last->next = pbuf;
				pbuf_list->last = pbuf;
			} else {
				pbuf_list = pbuf;
				pbuf_list->last = pbuf;
			}

			pbuf_list->total_frames += 1;; /*first pbuf*/
		}
		//printf("frag = %d\n", pbuf_list->total_frames);

		/*Construct the job descriptor to pass it to Memcached app*/
		if (_imem->alloc((addr_t *)&temp, cvt_alloc(JOB_DESC_ALLOCATOR), _index, core_idx) != Exokernel::S_OK) {
			panic("JOB_DESC_ALLOCATOR failed!\n");
		}
		job_descriptor_t * jd = (job_descriptor_t *) temp;
		__builtin_memset(jd, 0, sizeof(job_descriptor_t));

		if (_server_timestamp > 0)
			jd->ts_array[0] = rdtsc();  // timestamp before pushed to channel

		/* populate the job descriptor */
		jd->protocol_frame = pbuf_list;

		//getnameinfo, gethostbyaddr, gethostbyport

		ip_addr_t src_ip;
		memcpy(&src_ip, &msgs->msg_name_v[i].sin_addr, 4);

		jd->client_ip_addr = src_ip ;
		jd->client_udp_port = ntohs(msgs->msg_name_v[i].sin_port);

		/*
		char ip_char[64];
		strcpy(ip_char, inet_ntoa(msgs->msg_name_v[i].sin_addr));
		printf("idx=%d: port=%d, ip=%s \n", i, jd->client_udp_port, ip_char);

		char *ptr=(char *)&( msgs->msg_name_v[i]);
		for(unsigned k=0; k<sizeof(struct sockaddr_in); k++) 
			printf("%x ", *(ptr+k) );
		printf("\n");
		*/

		unsigned frag_number = pbuf_list->total_frames; //TODO
		jd->num_frames = frag_number;
		jd->num_frames_2_evict = frag_number;
		/*see protocol.h*/
		jd->evict = evict_needed(pbuf_list,
				                     jd->num_frames_2_evict,
				                     _imem->get_total_avail(cvt_alloc(PACKET_ALLOCATOR), _index), 
				                     _eviction_threshold);
		jd->core_id = core_idx;

		assert(jd);
		assert(jd->protocol_frame);
		while (_net_side_channel[tid]->produce(jd) != E_SPMC_CIRBUFF_OK) {
			cpu_relax();
		}

		if (_server_timestamp > 1)
			jd->ts_array[1] = rdtsc();   // timestamp after pushed to channel
	}

#if 0 //by testing whether the iov_base entry is null, we do not need to free
	/*free the pkt memory that are not pushed to the channel*/
	if(recv_cnt < VLEN) {
		for(unsigned i = recv_cnt; i<VLEN; i++)
			_imem->free((void *)(msgs->iovecs[i].iov_base), cvt_alloc(PACKET_ALLOCATOR), _index);
	}
#endif
	return recv_cnt;
}

/*called by recv thread entry function*/
void UDP_socket::recv_handler(unsigned tid) {

	unsigned cnt;

#define RECV_MMSG
#ifdef RECV_MMSG
	cnt = recv_burst(tid);
#else
	cnt = recv_single(tid);
#endif

//#define RECV_STAT
#ifdef RECV_STAT
	/************************** collect data (RX) **************************/
	if ( unlikely(recv_counter[tid].ctr.pkt == 0) )
		recv_counter[tid].ctr.timer = rdtsc(); /* timer reset */

	/* increment packet count */
	recv_counter[tid].ctr.pkt += cnt;

#define INTERVAL 1000000
	unsigned core = rx_core[tid];
	if ( unlikely(recv_counter[tid].ctr.pkt >= INTERVAL) ) {
		recv_counter[tid].ctr.accum_pkt += recv_counter[tid].ctr.pkt/1000;
		cpu_time_t finish_t = rdtsc();
		PLOG("[NIC %d RX %d @ CORE %d] RX %.2fK PPS (UDP) | %uK",
				_index,
				tid,
				core,
				(1.0 * cpu_freq_khz * recv_counter[tid].ctr.pkt) / (finish_t - recv_counter[tid].ctr.timer),
				recv_counter[tid].ctr.accum_pkt);
		recv_counter[tid].ctr.pkt=0;
	}
	/**************************DONE: collect data **************************/
#endif
}

inline unsigned UDP_socket::rx_queue_2_core(unsigned queue) {
	//unsigned tid = queue >> 1;
	unsigned tid = queue;
	return (rx_core[tid] + _index * CPU_NUM);
}

/*
inline unsigned UDP_socket::tx_queue_2_core(unsigned queue) {
	//unsigned tid = queue >> 1;
	unsigned tid = queue;
	return (tx_core[tid] + _index * CPU_NUM);
}
*/
void UDP_socket::free_packets(pbuf_t* pbuf_list, bool flag) {

	assert(pbuf_list != NULL);

	pbuf_t* curr_pbuf = pbuf_list;

	while (curr_pbuf != NULL) {
		assert(curr_pbuf->pkt);
		if (flag==true) {
			_imem->free(curr_pbuf->pkt, cvt_alloc(PACKET_ALLOCATOR), _index);
		}
		_imem->free(curr_pbuf, cvt_alloc(PBUF_ALLOCATOR), _index);
		curr_pbuf=curr_pbuf->next;
	}
	/*
	assert(pbuf_list != NULL);
	assert(pbuf_list->pkt != NULL);
	pbuf_t* curr_pbuf=pbuf_list;
	pbuf_t* next_pbuf=curr_pbuf->next;

	while (curr_pbuf->next != NULL) {
		if (flag==true) {
			_imem->free(curr_pbuf->pkt, cvt_alloc(PACKET_ALLOCATOR), _index);
		}
		_imem->free(curr_pbuf, cvt_alloc(PBUF_ALLOCATOR), _index);
		curr_pbuf=next_pbuf;
		next_pbuf=curr_pbuf->next;
	}
	if (flag == true) {
		_imem->free(curr_pbuf->pkt, cvt_alloc(PACKET_ALLOCATOR), _index);
	}
	_imem->free(curr_pbuf, cvt_alloc(PBUF_ALLOCATOR), _index);
	*/
}

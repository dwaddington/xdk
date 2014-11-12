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

#include "exo_stack.h"
#include <network_stack/protocol.h>
#include <network_stack/tcp.h>
#include <x540/x540_types.h>
#include <x540/x540_device.h>
#include "tx_threads.h"
#include <arpa/inet.h>

void Exo_stack::send_udp_pkt_test(unsigned tid) {

  enum { PACKET_LEN = 8000 };
  unsigned bytes_to_alloc = PACKET_LEN;

  /* allocate pkt memory */
  size_t BLOCK_SIZE = bytes_to_alloc;
  size_t NUM_BLOCKS = 1;
  size_t actual_size = round_up_page(Memory::Fast_slab_allocator::actual_block_size(BLOCK_SIZE) 
                                                   * NUM_BLOCKS);

  /* allocate some memory */
  addr_t space_p = 0;

  //allocate memory in normal pages
  void * space_v = ((Intel_x540_uddk_device *)_inic->driver(_index))->
                                        alloc_dma_pages(actual_size/PAGE_SIZE+1, &space_p);
  assert(space_v);
  assert(space_p);

  Memory::Fast_slab_allocator * xmit_pkt_allocator =
      new Memory::Fast_slab_allocator(space_v, space_p,
                                      actual_size,
                                      BLOCK_SIZE,
                                      64, // alignment
                                      1, // id
                                      90); // debug id

  PLOG("Fast_slab_allocator ctor OK for xmit_pkt_allocator.");

  addr_t pkt_v, pkt_p;
  pkt_v = (addr_t)xmit_pkt_allocator->alloc(tid);
  assert(pkt_v!=0);
  __builtin_memset((void *)pkt_v, 0xff, bytes_to_alloc);
  pkt_p = (addr_t)xmit_pkt_allocator->get_block_phy_addr((void *)pkt_v);

  unsigned queue = 2*tid+1;

  while (1) {
    udp_send_pkt((uint8_t *)pkt_v, pkt_p, PACKET_LEN, queue);
    sleep(1);
  }
}

void Exo_stack::send_pkt_test(unsigned tid) {

  enum { PACKET_LEN = 64 };

  unsigned char packet[PACKET_LEN] = {
        0xa0, 0x36, 0x9f, 0x1a, 0x56, 0xa0,
        0xa0, 0x36, 0x9f, 0x25, 0x65, 0xd0,
        0x08, 0x00, // type: ip datagram
        0x45, 0x00, // version
        0x00, 50, // length
        //0x00, 0x05, 0x40, 0x00, 0x3f, 0x11, 0x27, 0xb4,//ip header with checksum
        0x00, 0x05, 0x40, 0x00, 0x3f, 0x06, 0x00, 0x00,//ip header without checksum
        0x0a, 0x00, 0x00, 0x02, // src: 10.0.0.2
        0x0a, 0x00, 0x00, 0x01, // dst: 10.0.0.1
        0xdd, 0xd5, 0x2b, 0xcb, 0x00, 30, 0x00, 0x00, // UDP header
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21};

  unsigned bytes_to_alloc = 64;
  size_t pkt_burst_size = 32;

  /* allocate pkt memory */
  size_t BLOCK_SIZE = bytes_to_alloc;
  size_t NUM_BLOCKS = 1024;
  size_t actual_size = round_up_page(Memory::Fast_slab_allocator::actual_block_size(BLOCK_SIZE)  
						* NUM_BLOCKS);

  /* allocate some memory */
  addr_t space_p = 0;

  //allocate memory in normal pages
  void * space_v = ((Intel_x540_uddk_device *)_inic->driver(_index))->
                                    alloc_dma_pages(actual_size/PAGE_SIZE+1, &space_p);
  assert(space_v);
  assert(space_p);

  Memory::Fast_slab_allocator * xmit_pkt_allocator =
      new Memory::Fast_slab_allocator(space_v, space_p,
                                      actual_size,
                                      BLOCK_SIZE,
                                      64, // alignment
                                      1, // id
                                      90); // debug id

  PLOG("Fast_slab_allocator ctor OK for xmit_pkt_allocator.");

  struct exo_mbuf * xmit_addr_pool[1024];
  unsigned i;
  addr_t pkt_v, pkt_p;

  /* test of 3 segments packet sending (total len 64 = 42 + 10 + 12) */
  for (i = 0; i < 1024; i++) {
    pkt_v = (addr_t)xmit_pkt_allocator->alloc(tid);
    assert(pkt_v != 0);
    __builtin_memset((void *)pkt_v, 0, bytes_to_alloc);

    pkt_p = (addr_t)xmit_pkt_allocator->get_block_phy_addr((void *)pkt_v);

    __builtin_memcpy((void *)pkt_v, (void *)packet, PACKET_LEN);

    xmit_addr_pool[i] = (struct exo_mbuf *)malloc(sizeof(struct exo_mbuf));
    __builtin_memset(xmit_addr_pool[i], 0, sizeof(struct exo_mbuf));
    xmit_addr_pool[i]->phys_addr = pkt_p;
    xmit_addr_pool[i]->virt_addr = pkt_v;
    xmit_addr_pool[i]->len = 42;
    xmit_addr_pool[i]->ref_cnt = 0;
    xmit_addr_pool[i]->id = i;
    xmit_addr_pool[i]->nb_segment = 3;
    xmit_addr_pool[i]->phys_addr_seg[0] = pkt_p + 42;
    xmit_addr_pool[i]->phys_addr_seg[1] = pkt_p + 52;
    xmit_addr_pool[i]->virt_addr_seg[0] = pkt_v + 42;
    xmit_addr_pool[i]->virt_addr_seg[1] = pkt_v + 52;
    xmit_addr_pool[i]->seg_len[0] = 10;
    xmit_addr_pool[i]->seg_len[1] = 12;
  }         
            
  unsigned queue = 2*tid+1; 
  unsigned counter = 0;
  cpu_time_t start_time=0;
  cpu_time_t finish_time=0;

  while (1) {
    if (counter==0)
      start_time=rdtsc();

    unsigned sent_num = 0;
    size_t n;
    /* main TX function, return the actual packets sent out */

    if (pkt_burst_size<=IXGBE_TX_MAX_BURST) {
      /* Try to transmit at least chunks of TX_MAX_BURST pkts */
      n = pkt_burst_size;
      //ethernet_output_simple(&xmit_addr_pool[counter % 512], n, queue);
      ethernet_output(&xmit_addr_pool[counter % 512], n, queue);
      sent_num = n;
    }
    else {
      /* transmit more than the max burst, in chunks of TX_MAX_BURST */
      unsigned nb_pkts = pkt_burst_size;
      while(nb_pkts) {
        uint16_t ret, n;
        n = (uint16_t)EXO_MIN(nb_pkts, IXGBE_TX_MAX_BURST);
        ret = n;
        //ethernet_output_simple(&xmit_addr_pool[counter % 512], (size_t&)ret, queue);
        ethernet_output(&xmit_addr_pool[counter % 512], (size_t&)ret, queue);
        sent_num += ret;
        nb_pkts -= ret;
        if (ret < n)
          break;
      }
    }

    counter += sent_num;

    if (counter>=STATS_NUM) {
      finish_time=rdtsc();
      printf("[TID %d] TX THROUGHPUT %llu PPS\n",
             tid,
             (((unsigned long long)counter) * CPU_FREQ) / (finish_time - start_time));
      counter = 0;
    }
  }
}

void Exo_stack::setup_channels() {
  for (unsigned i = 0; i < NUM_CHANNEL; i++) {
    _net_side_channel[i] = new Shm_channel_t(i + _index * NUM_CHANNEL, true);
  }
}

void Exo_stack::init_param() {
  unsigned i;
  for (i = 0; i < 6; i++) {
    mac[i] = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->mac[i];
  }

  for (i = 0; i < 4; i++) {
    ip[i] = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->ip[i];
  }

  ethaddr = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->ethaddr;
  ip_addr = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->ip_addr;

  uint8_t client_ip[4];
  client_ip[0] = 10;
  client_ip[1] = 0;
  client_ip[2] = 0;
  client_ip[3] = 11;
  uint32_t temp = 0;
  temp = (client_ip[0]<<24) + (client_ip[1]<<16) + (client_ip[2]<<8) + client_ip[3];
  remote_ip_addr.addr = ntohl(temp);

  udp_bind(&ip_addr, SERVER_PORT);
  remote_port = CLIENT_PORT;

  for (unsigned i = 0; i < NUM_RX_QUEUES; i++) {
    __builtin_memset(&recv_counter[i], 0, sizeof(recv_counter[i]));
  }

  for (unsigned i = 0; i < ARP_TABLE_SIZE; i++) {
    __builtin_memset(&arp_table[i], 0, sizeof(struct etharp_entry));
  }

  /* protocol related init*/
  for (int i=0;i<NUM_RX_THREADS_PER_NIC;i++) {
    reassdatagrams[i] = NULL;
    udp_pcbs[i] = NULL;
  }

	//const ip_addr_t ip_addr_any = { (uint32_t)0x00000000UL };
	
#if 0
	for (int i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
		tcp_pcbs[i] = NULL;
	}
#endif
	_tcp_pcb_lists[0] = &_tcp_listen_pcbs.pcbs;
	_tcp_pcb_lists[1] = &_tcp_bound_pcbs;
	_tcp_pcb_lists[2] = &_tcp_active_pcbs;
	_tcp_pcb_lists[3] = &_tcp_tw_pcbs;

	for (int i = 0; i < 13; i++) {
		if (i < 6) 
			_tcp_backoff[i] = i+1;
		else 
			_tcp_backoff[i] = 7;
	}
	_tcp_persist_backoff[0] = 3;
	_tcp_persist_backoff[1] = 6;
	_tcp_persist_backoff[2] = 12;
	_tcp_persist_backoff[3] = 24;
	_tcp_persist_backoff[4] = 48;
	_tcp_persist_backoff[5] = 96;
	_tcp_persist_backoff[6] = 120;

	_exoprot_count = 0;
	_exoprot_thread = (pthread_t)0xDEAD;
	_exoprot_mutex = PTHREAD_MUTEX_INITIALIZER;
}

void Exo_stack::setup_tx_threads() {
  Tx_thread* tx_threads[NUM_TX_THREADS_PER_NIC];
  PINF("Creating TX threads ...");

  /* derive cpus for TX threads from TX_THREAD_CPU_MASK */
  for (unsigned i = 0; i < NUM_TX_THREADS_PER_NIC; i++) {
    tx_core[i] = 0;
  }

  uint16_t pos = 1;
  unsigned n = 0;
  for (unsigned i = 0; i < CPU_NUM; i++) {
    pos = 1 << i;
    if ((TX_THREAD_CPU_MASK & pos) != 0) {
      tx_core[n] = i;
      n++;
    }
  }

#if 0
  printf("TX Thread assigned core: ");
  for (unsigned i = 0; i < NUM_TX_THREADS_PER_NIC; i++) {
    printf("%d ",tx_core[i]);
  }
  printf("\n");
#endif

  for(unsigned i = 0; i < NUM_TX_THREADS_PER_NIC; i++) {
    int global_id = i + _index * NUM_TX_THREADS_PER_NIC;
    int local_id  = i;
    int core_id = tx_core[i] + _index * CPU_NUM;
    tx_threads[i]= new Tx_thread(this, local_id, global_id, core_id);
    PLOG("TX[%d] is running on core %d", global_id, core_id);
    assert(tx_threads[i]);
    tx_threads[i]->activate();
  }
}


status_t Exo_stack::ethernet_output(struct exo_mbuf ** pkt, size_t& cnt, unsigned queue) {
  size_t to_be_sent = cnt;
  status_t s = _inic->send_packets((pkt_buffer_t *)pkt, cnt, _index, queue);
  return s;
}

status_t Exo_stack::ethernet_output_simple(struct exo_mbuf ** pkt, size_t& cnt, unsigned queue) {
  size_t to_be_sent = cnt;
  status_t s = _inic->send_packets_simple((pkt_buffer_t *)pkt, cnt, _index, queue);
  return s;
}

pkt_status_t Exo_stack::ethernet_input(struct exo_mbuf **mbuf, size_t cnt, unsigned queue) {
	PLOG("starts");
  //TODO single packet receive only 
  assert(cnt == 1);

  struct eth_hdr* ethhdr;
  uint16_t type;
  pkt_status_t t;

  unsigned len = (*mbuf)->len;
	PLOG("len %u", len);
  addr_t pkt = (*mbuf)->virt_addr;
 
  if (len<=SIZEOF_ETH_HDR) {
    return PACKET_ERROR_BAD_ETH;
  }

  /* points to packet payload, which starts with an Ethernet header */
  ethhdr = (struct eth_hdr *)pkt;

#if 0 
  printf("ethernet_input: dest:%x:%x:%x:%x:%x:%x, src:%x:%x:%x:%x:%x:%x, type:%04x\n",
     (unsigned)ethhdr->dest.addr[0], (unsigned)ethhdr->dest.addr[1], (unsigned)ethhdr->dest.addr[2],
     (unsigned)ethhdr->dest.addr[3], (unsigned)ethhdr->dest.addr[4], (unsigned)ethhdr->dest.addr[5],
     (unsigned)ethhdr->src.addr[0], (unsigned)ethhdr->src.addr[1], (unsigned)ethhdr->src.addr[2],
     (unsigned)ethhdr->src.addr[3], (unsigned)ethhdr->src.addr[4], (unsigned)ethhdr->src.addr[5],
     (unsigned)htons(ethhdr->type));
#endif

#if 0
  /* shortcut to test raw ethernet frame performance*/
  if (recv_counter[queue].ctr.pkt==0)
    recv_counter[queue].ctr.timer=rdtsc(); /* timer reset */

  /* increment packet count */
  recv_counter[queue].ctr.pkt++;
  recv_counter[queue].ctr.accum_pkt++;

  unsigned num=STATS_NUM;
  if (recv_counter[queue].ctr.pkt==num) {
    cpu_time_t finish_t=rdtsc();
        PLOG("[NIC %d CH %d @ CORE %d] RX THROUGHPUT %llu PPS",
             _index,
             queue/2,
             queue/2,
             (((unsigned long long)num) * CPU_FREQ) / (finish_t - recv_counter[queue].ctr.timer));
    recv_counter[queue].ctr.pkt=0;
  }
#endif

  _imem->free(*mbuf, MBUF_ALLOCATOR);

  type = htons(ethhdr->type);
  switch (type) {
    case ETHTYPE_ARP:
      t = arp_input((uint8_t *)pkt);
      break;

    case ETHTYPE_IP:
      t = ip_input((uint8_t *)pkt, len, queue);
      break;

    default:
      PLOG("UNKNOWN_ETH_TYPE %x (frame address = %p)",type, ethhdr);
      printf("ethernet_input: dest:%x:%x:%x:%x:%x:%x, src:%x:%x:%x:%x:%x:%x, type:%04x\n",
     (unsigned)ethhdr->dest.addr[0], (unsigned)ethhdr->dest.addr[1], (unsigned)ethhdr->dest.addr[2],
     (unsigned)ethhdr->dest.addr[3], (unsigned)ethhdr->dest.addr[4], (unsigned)ethhdr->dest.addr[5],
     (unsigned)ethhdr->src.addr[0], (unsigned)ethhdr->src.addr[1], (unsigned)ethhdr->src.addr[2],
     (unsigned)ethhdr->src.addr[3], (unsigned)ethhdr->src.addr[4], (unsigned)ethhdr->src.addr[5],
     (unsigned)htons(ethhdr->type));

      return PACKET_ERROR_UNKNOWN_ETH_TYPE;
  }
	PLOG("ends.");
  return t;
}

pkt_status_t Exo_stack::ip_input(uint8_t *pkt, uint16_t len, unsigned queue) {
	PLOG();
  static unsigned counter = 0;
  struct ip_hdr *iphdr;
  uint16_t iphdr_hlen;
  uint16_t iphdr_len;
  bool packet_completed = true;
  struct pbuf2 * p = NULL;
  pkt_status_t t = REUSE_THIS_PACKET;

  /* identify the IP header */
  iphdr = (struct ip_hdr *)(pkt + SIZEOF_ETH_HDR);

  if (IPH_V(iphdr) != 4) {
    t = PACKET_ERROR_BAD_IP;
    return t;
  }

  /* obtain IP header length in number of 32-bit words */
  iphdr_hlen = IPH_HL(iphdr);

  /* calculate IP header length in bytes */
  iphdr_hlen *= 4;

  /* obtain ip length in bytes */
  iphdr_len = ntohs(IPH_LEN(iphdr));

	/* copy IP addresses to aligned ip_addr_t */
	ip_addr_copy(*ip_2_ip(&_ip_data.current_iphdr_dest), iphdr->dest);
	ip_addr_copy(*ip_2_ip(&_ip_data.current_iphdr_src), iphdr->src);

	/* allocate pbuf2 for RAW packet */
	//pbuf2 = (struct pbuf2 *)malloc(sizeof(struct pbuf2));
	p = pbuf2_alloc(PBUF_IP, len, PBUF_RAM);
	assert(p);

	/* populate pbuf struct for RAW packet */
	p->next = NULL;
	p->payload = pkt + SIZEOF_ETH_HDR;
	p->tot_len = len - SIZEOF_ETH_HDR;
	p->len = len - SIZEOF_ETH_HDR;
	p->type = 0;
	p->flags = 0;
	p->ref = 1;
	ip_debug_print(p);

	//if (raw_input(p) == 0) {
	if (1) {
		//pbuf2_header(p, -iphdr_hlen);

	  if (is_sent_to_me(pkt)) {
			switch (IPH_PROTO(iphdr)) {
#if 0
				case IP_PROTO_ICMP:
					t = icmp_input(pkt);
 		      _imem->free(pbuf_list, PBUF_ALLOCATOR);
   		    break;

				case IP_PROTO_UDP:
					if (packet_completed == true) {
          	t = udp_input(pbuf, queue);
        	}
        	else {
          	if (t != KEEP_THIS_PACKET) {
           		printf("Unwanted IP frag received...discard!!\n");
            	t = REUSE_THIS_PACKET;
          	}
        	}
        	break;
#endif
				case IP_PROTO_TCP:
					t = tcp_input(p, queue);
					break;

      	default:
       		t = PACKET_ERROR_UNKNOWN_IP_TYPE;
    	};
  	}
	}
	PLOG("ends.");
  return t;
}

struct ip_reassdata* Exo_stack::ip_reass_enqueue_new_datagram(struct ip_hdr *fraghdr, unsigned queue) {
  struct ip_reassdata* ipr;
  unsigned tid = queue/2;

  /* No matching previous fragment found, allocate a new reassdata struct */
  void * p;
  if (_imem->alloc((addr_t *)&p, IP_REASS_ALLOCATOR) != Exokernel::S_OK) {
    panic("IP_REASS_ALLOCATOR failed!\n");
  }
  assert(p != NULL);
  ipr = (struct ip_reassdata *) p;

  __builtin_memset(ipr, 0, sizeof(struct ip_reassdata));

  /* enqueue the new structure to the front of the list */
  ipr->next = reassdatagrams[tid];
  reassdatagrams[tid] = ipr;
  SMEMCPY(&ipr->iphdr, fraghdr, IP_HLEN);
  return ipr;
}

bool Exo_stack::ip_reass_chain_frag_into_datagram_and_validate(struct ip_reassdata *ipr, struct pbuf *new_p, pkt_status_t * t) {
  struct ip_reass_helper *iprh, *iprh_tmp, *iprh_prev=NULL;
  struct pbuf *q;
  uint16_t offset,len;
  struct ip_hdr *fraghdr;
  bool valid = true;

  /* Extract length and fragment offset from current fragment */
  fraghdr = (struct ip_hdr*)(new_p->pkt+SIZEOF_ETH_HDR);
  len = ntohs(IPH_LEN(fraghdr)) - IPH_HL(fraghdr) * 4;
  offset = (ntohs(IPH_OFFSET(fraghdr)) & IP_OFFMASK) * 8;

  /* overwrite the fragment's ip header from the pbuf with our helper struct,
   * and setup the embedded helper structure. */
  /* make sure the struct ip_reass_helper fits into the IP header */
  iprh = (struct ip_reass_helper*) (&new_p->irh);
  iprh->next_pbuf = NULL;
  iprh->start = offset;
  iprh->end = offset + len;

  /* Iterate through until we either get to the end of the list (append),
   * or we find on with a larger offset (insert). */
  for (q = ipr->p; q != NULL;) {
    iprh_tmp = (struct ip_reass_helper*)(&q->irh);
    if (iprh->start < iprh_tmp->start) {
      /* the new pbuf should be inserted before this */
      iprh->next_pbuf = q;

      if (iprh_prev != NULL) {
        /* not the fragment with the lowest offset */
        if ((iprh->start < iprh_prev->end) || (iprh->end > iprh_tmp->start)) {
          /* fragment overlaps with previous or following, throw away */
          printf("[Drop!]fragment overlaps with previous or following!!!\n");
          *t=PACKET_ERROR_BAD_IP_FRAG_OVERLAP1;
          _imem->free(new_p, PBUF_ALLOCATOR);
        }
        iprh_prev->next_pbuf = new_p;
      } else {
        /* fragment with the lowest offset */
        ipr->p = new_p;
      }
      break;
    } else if(iprh->start == iprh_tmp->start) {
      /* received the same datagram twice: no need to keep the datagram */
      printf("[Drop!]received the same datagram twice!!!\n");
      *t=PACKET_ERROR_BAD_IP_FRAG_DUPLICATE;
      _imem->free(new_p, PBUF_ALLOCATOR);
    } else if(iprh->start < iprh_tmp->end) {
      /* overlap: no need to keep the new datagram */
      printf("[Drop!]fragment overlaps!!!\n");
       *t=PACKET_ERROR_BAD_IP_FRAG_OVERLAP2;
      _imem->free(new_p, PBUF_ALLOCATOR);
    } else {
      /* Check if the fragments received so far have no holes. */
      if (iprh_prev != NULL) {
        if (iprh_prev->end != iprh_tmp->start) {
          /* There is a fragment missing between the current
           * and the previous fragment */
          valid = false;
        }
      }
    }
    q = iprh_tmp->next_pbuf;
    iprh_prev = iprh_tmp;
  }
  /* If q is NULL, then we made it to the end of the list. Determine what to do now */
  if (q == NULL) {
    if (iprh_prev != NULL) {
      /* this is (for now), the fragment with the highest offset:
       * chain it to the last fragment */
      iprh_prev->next_pbuf = new_p;
      if (iprh_prev->end != iprh->start) {
        valid = false;
      }
    } else {
      /* this is the first fragment we ever received for this ip datagram */
      ipr->p = new_p;
    }
  }

  /* At this point, the validation part begins: */
  /* If we already received the last fragment */
  if ((ipr->flags & IP_REASS_FLAG_LASTFRAG) != 0) {
    /* and had no wholes so far */
    if (valid) {
      /* then check if the rest of the fragments is here */
      /* Check if the queue starts with the first datagram */
      if (((struct ip_reass_helper*)(&(ipr->p->irh)))->start != 0) {
        valid = false;
      } else {
        /* and check that there are no holes after this datagram */
        iprh_prev = iprh;
        q = iprh->next_pbuf;

        while (q != NULL) {
          iprh = (struct ip_reass_helper*)(&q->irh);
          if (iprh_prev->end != iprh->start) {
            valid = false;
            break;
          }
          iprh_prev = iprh;
          q = iprh->next_pbuf;
        }
        /* if still valid, all fragments are received
         * (because to the MF==0 already arrived */
        if (valid) {
          assert(ipr->p != NULL);
          assert(((struct ip_reass_helper*)(&(ipr->p->irh))) != iprh);
          assert(iprh->next_pbuf == NULL);
          assert(iprh->end == ipr->datagram_len);
        }
      }
    }
  /* If valid is 0 here, there are some fragments missing in the middle
     * (since MF == 0 has already arrived). Such datagrams simply time out if
     * no more fragments are received... */
    return valid;
  }
  /* If we come here, not all fragments were received, yet! */
  return false; /* not yet valid! */
}

/**
 * Dequeues a datagram from the datagram queue. Doesn't deallocate the pbufs.
 * @param ipr points to the queue entry to dequeue
 */
void Exo_stack::ip_reass_dequeue_datagram(struct ip_reassdata *ipr, struct ip_reassdata *prev, unsigned queue) {
  unsigned tid = queue/2;
  /* dequeue the reass struct  */
  if (reassdatagrams[tid] == ipr) {
    /* it was the first in the list */
    reassdatagrams[tid] = ipr->next;
  } else {
    /* it wasn't the first, so it must have a valid 'prev' */
    assert(prev != NULL);
    prev->next = ipr->next;
  }

  /* now we can free the ip_reass struct */
  _imem->free(ipr, IP_REASS_ALLOCATOR);
}

struct pbuf * Exo_stack::ip_reass(struct pbuf *pbuf_pkt, pkt_status_t * t, unsigned queue) {
  struct pbuf *r;
  struct ip_hdr *fraghdr;
  struct ip_reassdata *ipr;
  struct ip_reass_helper *iprh;
  struct ip_reassdata *ipr_prev = NULL;
  uint16_t offset, len, pkt_len;
  int tid=queue/2;

  assert(pbuf_pkt->next==NULL); // this has to be the single pbuf

  fraghdr=(struct ip_hdr *)(pbuf_pkt->pkt + SIZEOF_ETH_HDR);
  pkt_len = ntohs(IPH_LEN(fraghdr));
  offset = (ntohs(IPH_OFFSET(fraghdr)) & IP_OFFMASK) * 8;
  len = pkt_len - IPH_HL(fraghdr) * 4;

  for (ipr = reassdatagrams[tid]; ipr != NULL; ipr = ipr->next) {
    if (IP_ADDRESSES_AND_ID_MATCH(&ipr->iphdr, fraghdr)) {
      //printf("ip_reass: matching previous fragment ID=0x%04x\n", ntohs(IPH_ID(fraghdr)));
      break;
    }
    ipr_prev = ipr;
  }

  if (ipr == NULL) {
    /* Enqueue a new datagram into the datagram queue */
    ipr = ip_reass_enqueue_new_datagram(fraghdr, queue);

  }
  else {
    if (((ntohs(IPH_OFFSET(fraghdr)) & IP_OFFMASK) == 0) &&
      ((ntohs(IPH_OFFSET(&ipr->iphdr)) & IP_OFFMASK) != 0)) {
      /* ipr->iphdr is not the header from the first fragment, but fraghdr is
       * -> copy fraghdr into ipr->iphdr since we want to have the header
       * of the first fragment */
      SMEMCPY(&ipr->iphdr, fraghdr, IP_HLEN);
    }
  }

  /* At this point, we have either created a new entry or pointing 
   * to an existing one
   */

  /* check for 'no more fragments', and update queue entry*/
  if ((IPH_OFFSET(fraghdr) & ntohs(IP_MF)) == 0) {
    ipr->flags |= IP_REASS_FLAG_LASTFRAG;
    ipr->datagram_len = offset + len;
    //printf("ip_reass: last fragment seen, total len %u\n", ipr->datagram_len);
  }

  *t=KEEP_THIS_PACKET;

  /* find the right place to insert this pbuf */
  if (ip_reass_chain_frag_into_datagram_and_validate(ipr, pbuf_pkt, t)) {
    /* the totally last fragment (flag more fragments = 0) was received at least
     * once AND all fragments are received */
    ipr->datagram_len += IP_HLEN;

    /* save the second pbuf before copying the header over the pointer */
    r = ((struct ip_reass_helper*)(&(ipr->p->irh)))->next_pbuf;

    pbuf_pkt = ipr->p;

    struct pbuf * temp=pbuf_pkt;
    /* chain together the pbufs contained within the reass_data list. */
    while (r!=NULL) {
      iprh = (struct ip_reass_helper*)(&r->irh);
      temp->next=r;
      r = iprh->next_pbuf;
      temp=temp->next;
    }
    /* release the sources allocate for the fragment queue entry */
    ip_reass_dequeue_datagram(ipr, ipr_prev, queue);

    /* Return the pbuf chain */
    return pbuf_pkt;
  }
  return NULL;
}

bool Exo_stack::is_sent_to_me(uint8_t *pkt) {
  struct eth_hdr * ethhdr = (struct eth_hdr *)pkt;
  for (unsigned i = 0; i < ETHARP_HWADDR_LEN; i++) {
    if (mac[i] != ethhdr->dest.addr[i]) {
        return false;
      }
    }
    return true;
}

pkt_status_t Exo_stack::icmp_input(uint8_t *pkt) {
  uint8_t type;
  struct icmp_echo_hdr *iecho;
  struct ip_hdr *iphdr;
  uint16_t hlen;

  iphdr = (struct ip_hdr *)(pkt + SIZEOF_ETH_HDR);
  hlen = IPH_HL(iphdr) * 4;
  iecho = (struct icmp_echo_hdr *)(pkt + SIZEOF_ETH_HDR + hlen);

  type = iecho->type;

  switch (type) {
    case ICMP_ECHO:
      send_ping_reply(pkt);
      break;
    default:
      printf("Unsupported ICMP packet types!!!...ignored\n");
  };

  return REUSE_THIS_PACKET;
}

void Exo_stack::send_ping_reply(uint8_t *pkt) {
  struct eth_hdr *ethhdr;
  struct ip_hdr *iphdr;
  struct icmp_echo_hdr *iecho;
  uint16_t hlen,len;
  unsigned pkt_len;

  ethhdr = (struct eth_hdr *)pkt;
  iphdr = (struct ip_hdr *)(pkt + SIZEOF_ETH_HDR);
  iphdr->_chksum=0;
  hlen = IPH_HL(iphdr) * 4;
  iecho = (struct icmp_echo_hdr *)(pkt + SIZEOF_ETH_HDR + hlen);

  ETHADDR16_COPY(&ethhdr->dest, &ethhdr->src);
  ETHADDR16_COPY(&ethhdr->src, &ethaddr);

  ICMPH_TYPE_SET(iecho, ICMP_ER);
  ip_addr_copy(iphdr->dest, iphdr->src);
  ip_addr_copy(iphdr->src, ip_addr);

  /* adjust the checksum */
  if (iecho->chksum >= htons(0xffffU - (ICMP_ECHO << 8))) {
    iecho->chksum += htons(ICMP_ECHO << 8) + 1;
  } else {
    iecho->chksum += htons(ICMP_ECHO << 8);
  }

  len = ntohs(IPH_LEN(iphdr));
  pkt_len = len + SIZEOF_ETH_HDR;

  /* The actual sending routine */
  unsigned tx_queue = 0;
  void * temp;
  if (_imem->alloc((addr_t *)&temp, MBUF_ALLOCATOR) != Exokernel::S_OK) {
    panic("MBUF_ALLOCATOR failed!\n");
  }
  assert(temp);
  __builtin_memset(temp, 0, sizeof(struct exo_mbuf));

  addr_t pkt_p;
  pkt_p = _imem->get_phys_addr(pkt, PACKET_ALLOCATOR);
  assert(pkt_p);

  struct exo_mbuf * mbuf = (struct exo_mbuf *)temp;
  mbuf->phys_addr = pkt_p;
  mbuf->virt_addr = (addr_t)pkt;
  mbuf->len = pkt_len;
  mbuf->ref_cnt = 0;
  mbuf->id = tx_queue;
  mbuf->flag = 0;
  mbuf->nb_segment = 1;

  size_t sent_num = 1;
  if (ethernet_output(&mbuf, sent_num, tx_queue) != Exokernel::S_OK) {
    panic("%s: TX ring buffer is full!\n", __func__);
  };

  _imem->free(temp, MBUF_ALLOCATOR);
}

void Exo_stack::udp_bind(ip_addr_t *ipaddr, uint16_t port) {
  struct udp_pcb *pcb;
  for (int i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
     void * temp;
     if (_imem->alloc((addr_t *)&temp, UDP_PCB_ALLOCATOR) != Exokernel::S_OK) {
       panic("UDP_PCB_ALLOCATOR failed!\n");
     }
     assert(temp);
     __builtin_memset(temp, 0, sizeof(struct udp_pcb));

     pcb = (struct udp_pcb *) temp;

     pcb->ttl = UDP_TTL;
     ip_addr_set(&pcb->local_ip, ipaddr);
     pcb->local_port = port;

     /* add to the new UDP PCB to the head of the UDP PCB list */
     pcb->next = udp_pcbs[i];
     udp_pcbs[i] = pcb;
   }
   udp_port = port;
}

pkt_status_t Exo_stack::udp_input(struct pbuf *pbuf_list, unsigned queue) {
  struct ip_hdr *iphdr;
  struct udp_hdr *udphdr;
  struct pbuf * tmp;
  int frag_number=0;
  int udp_size;
  int tid=queue/2;

  pkt_status_t t=REUSE_THIS_PACKET;

  for (tmp = pbuf_list; tmp != NULL; tmp = tmp->next) {
    frag_number++;
  }
  iphdr = (struct ip_hdr *)(pbuf_list->pkt + SIZEOF_ETH_HDR);

  /** Source IP address of current_header */
  ip_addr_t current_iphdr_src;

  /* copy IP addresses to aligned ip_addr_t */
  ip_addr_copy(current_iphdr_src, iphdr->src);

  udphdr = (struct udp_hdr *)(pbuf_list->pkt + SIZEOF_ETH_HDR + IPH_HL(iphdr) * 4);
  udp_size = ntohs(udphdr->len);

  uint16_t src, dest;

  /* convert src and dest ports to host byte order */
  src = ntohs(udphdr->src);
  dest = ntohs(udphdr->dest);

  /* if this udp is not kvcache request, we discard it */
  if (dest != SERVER_PORT) {
    PINF("Discard non-memcached protocol udp packets!!! (dest port %u)",dest);
    free_packets(pbuf_list, false);
    return t;
  }

  /* if the arp table does not contain the source IP, we discard it */
  int i;
  for (i = 0; i < ARP_TABLE_SIZE; i++) {
    if (ip_addr_cmp(&current_iphdr_src, &arp_table[i].ipaddr)) break;
  }
  if (i >= ARP_TABLE_SIZE) {
    /* add the new ip and eth to the arp table */
    struct eth_hdr *ethhdr = (struct eth_hdr *)(pbuf_list->pkt);
    update_arp_entry(&current_iphdr_src, &(ethhdr->src));
#if 0
    printf("New arp entry: IP: %x, MAC: %x.%x.%x.%x.%x.%x\n",current_iphdr_src.addr, 
                                                             (ethhdr->src).addr[0],
                                                             (ethhdr->src).addr[1],
                                                             (ethhdr->src).addr[2],
                                                             (ethhdr->src).addr[3], 
                                                             (ethhdr->src).addr[4],
                                                             (ethhdr->src).addr[5]);
#endif
  }

#if 0
    printf("FDIRMISS %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRMISS));
    printf("FDIRMATCH %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRMATCH));
    printf("FDIRUSTAT %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRUSTAT));
    printf("FDIRFSTAT %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRFSTAT));

#endif

#if 0
    printf("THIS UDP [%u]: TOTAL FRAGS %d and TOTAL SIZE %d\n",recv_counter[queue].ctr.pkt, frag_number,udp_size);  
    printf("[Queue %d]udp (%u.%u.%u.%u, %u) <-- (%u.%u.%u.%u, %u)\n", queue,
                   ip4_addr1_16(&iphdr->dest), ip4_addr2_16(&iphdr->dest),
                   ip4_addr3_16(&iphdr->dest), ip4_addr4_16(&iphdr->dest), ntohs(udphdr->dest),
                   ip4_addr1_16(&iphdr->src), ip4_addr2_16(&iphdr->src),
                   ip4_addr3_16(&iphdr->src), ip4_addr4_16(&iphdr->src), ntohs(udphdr->src));

    uint8_t * temp = (uint8_t *)udphdr+8+6;
    for (int i=0;i<2;i++) 
      {
        printf("[%d]%x ",i,(*temp)&0xff);  
        temp++;
      }
    printf("\n");
#endif

#if 1
  /* shortcut to test performance*/
  if (recv_counter[queue].ctr.pkt == 0)
    recv_counter[queue].ctr.timer = rdtsc(); /* timer reset */

  /* increment packet count */
  recv_counter[queue].ctr.pkt++;
  recv_counter[queue].ctr.accum_pkt++;

  unsigned num = STATS_NUM;
  if (recv_counter[queue].ctr.pkt == num) {
    cpu_time_t finish_t = rdtsc();
        PLOG("[NIC %d CH %d @ CORE %d] RX THROUGHPUT %llu PPS",
             _index,
             queue/2,
             queue/2,
             (((unsigned long long)num) * CPU_FREQ) / (finish_t - recv_counter[queue].ctr.timer));
    recv_counter[queue].ctr.pkt=0;
  }
#endif
  
  unsigned channel_id = queue/2;
//  PLOG("NIC[%d]: Before pushing to channel %d", _index, channel_id);
  while (_net_side_channel[channel_id]->produce(pbuf_list) != E_SPMC_CIRBUFF_OK) {
    cpu_relax();
  }

//  PLOG("NIC[%d]: pushed to channel %d",_index, channel_id);

//  free_packets(pbuf_list,false);
  t = KEEP_THIS_PACKET;
  return t;
}

void Exo_stack::free_packets(struct pbuf* pbuf_list, bool flag) {
  assert(pbuf_list != NULL);
  assert(pbuf_list->pkt != NULL);
  struct pbuf* curr_pbuf=pbuf_list;
  struct pbuf* next_pbuf=curr_pbuf->next;

  while (curr_pbuf->next != NULL) {
    if (flag==true) {
      _imem->free(curr_pbuf->pkt, PACKET_ALLOCATOR);
    }
    _imem->free(curr_pbuf, PBUF_ALLOCATOR);
    curr_pbuf=next_pbuf;
    next_pbuf=curr_pbuf->next;
  }
  if (flag == true) {
    _imem->free(curr_pbuf->pkt, PACKET_ALLOCATOR);
  }
  _imem->free(curr_pbuf, PBUF_ALLOCATOR);
}

pkt_status_t Exo_stack::arp_input(uint8_t* pkt) {
  struct etharp_hdr *hdr;
  struct eth_hdr *ethhdr;
  ip_addr_t sipaddr, dipaddr;
  uint8_t for_us;

  ethhdr = (struct eth_hdr *)pkt;
  hdr = (struct etharp_hdr *)((uint8_t*)ethhdr + SIZEOF_ETH_HDR);
  pkt_status_t t = REUSE_THIS_PACKET;

  if((hdr->hwtype != htons(HWTYPE_ETHERNET)) ||
    (hdr->hwlen != ETHARP_HWADDR_LEN) ||
    (hdr->protolen != sizeof(ip_addr_t)) ||
    (hdr->proto != htons(ETHTYPE_IP)))  {

    t = PACKET_ERROR_ARP_INPUT;
    printf("arp_input: packet dropped, wrong hw type, hwlen, proto, "
                   "protolen or ethernet type (%x %x %x %x)\n",
                   hdr->hwtype, hdr->hwlen, hdr->proto, hdr->protolen);
    return t;
  }

  IPADDR2_COPY(&sipaddr, &hdr->sipaddr);
  IPADDR2_COPY(&dipaddr, &hdr->dipaddr);

  /* ARP packet directed to us? */
  for_us = (uint8_t)ip_addr_cmp(&dipaddr,  &ip_addr);

  update_arp_entry(&sipaddr, &hdr->shwaddr);

  uint16_t opcode=htons(hdr->opcode);

  switch (opcode) {
    case ARP_REQUEST:
      if (for_us) {
        send_arp_reply(pkt);
      }
      break;
    case ARP_REPLY:
      break;
    default:
      t = PACKET_ERROR_WRONG_ARP_TYPE;
  };
  return t;
}

void Exo_stack::update_arp_entry(ip_addr_t *ipaddr, struct eth_addr *ethaddr) {
  /* find or create ARP entry */
  int i = find_arp_entry(ipaddr);
  /* mark it stable */
  arp_table[i].state = ETHARP_STATE_STABLE;
  /* update address */
  ETHADDR16_COPY(&arp_table[i].ethaddr, ethaddr);
}

int Exo_stack::find_arp_entry(ip_addr_t *ipaddr) {
  unsigned i;
  /* first search if there exists the same IP address in the arp_table */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    if (ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
      arp_table[i].state=ETHARP_STATE_STABLE;
      return i;
    }
  }

  /* second try to find the first empty slot */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    if (arp_table[i].state==ETHARP_STATE_EMPTY) {
      ip_addr_copy(arp_table[i].ipaddr, *ipaddr);
      arp_table[i].state=ETHARP_STATE_STABLE;
      return i;
    }
  }

  /* arp_table is full, we kick out the first entry */
  ip_addr_copy(arp_table[0].ipaddr, *ipaddr);
  arp_table[0].state=ETHARP_STATE_STABLE;
  return 0;
}

void Exo_stack::arp_output(ip_addr_t *ipdst_addr) {
  addr_t tmp_v,tmp_p;
  void * temp;
  if (_imem->alloc((addr_t *)&temp, PACKET_ALLOCATOR) != Exokernel::S_OK) {
    panic("PACKET_ALLOCATOR failed!\n");
  }
  assert(temp);

  tmp_v=(addr_t)temp;
  tmp_p=(addr_t)_imem->get_phys_addr(temp, PACKET_ALLOCATOR);
  assert(tmp_p);

  struct eth_hdr *ethhdr;
  struct etharp_hdr *hdr;
  ethhdr=(struct eth_hdr *)tmp_v;
  hdr = (struct etharp_hdr *) ((uint8_t*)ethhdr + SIZEOF_ETH_HDR);
  hdr->opcode = htons(ARP_REQUEST);

  struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};
  struct eth_addr ethzero = {{0,0,0,0,0,0}};

  struct eth_addr *ethsrc_addr = &ethaddr;
  struct eth_addr *ethdst_addr = &ethbroadcast;
  struct eth_addr *hwsrc_addr = ethsrc_addr;
  ip_addr_t *ipsrc_addr = &ip_addr;
  struct eth_addr *hwdst_addr=&ethzero;

  /* Write the ARP MAC-Addresses */
  ETHADDR16_COPY(&hdr->shwaddr, hwsrc_addr);
  ETHADDR16_COPY(&hdr->dhwaddr, hwdst_addr);
  /* Write the Ethernet MAC-Addresses */
  ETHADDR16_COPY(&ethhdr->dest, ethdst_addr);
  ETHADDR16_COPY(&ethhdr->src, ethsrc_addr);
  /* Copy struct ip_addr2 to aligned ip_addr, to support compilers without
   * structure packing. */
  IPADDR2_COPY(&hdr->sipaddr, ipsrc_addr);
  IPADDR2_COPY(&hdr->dipaddr, ipdst_addr);

  hdr->hwtype = htons(HWTYPE_ETHERNET);
  hdr->proto = htons(ETHTYPE_IP);
  /* set hwlen and protolen */
  hdr->hwlen = ETHARP_HWADDR_LEN;
  hdr->protolen = sizeof(ip_addr_t);

  ethhdr->type = htons(ETHTYPE_ARP);

  send_arp((uint8_t *)tmp_v, SIZEOF_ETHARP_PACKET, 0, false);
}

void Exo_stack::send_arp_reply(uint8_t *pkt) {
  struct etharp_hdr *hdr;
  struct eth_hdr *ethhdr;
  ethhdr = (struct eth_hdr *)pkt;
  hdr = (struct etharp_hdr *)(pkt + SIZEOF_ETH_HDR);

  hdr->opcode = htons(ARP_REPLY);
  IPADDR2_COPY(&hdr->dipaddr, &hdr->sipaddr);
  IPADDR2_COPY(&hdr->sipaddr,  &ip_addr);

  ETHADDR16_COPY(&hdr->dhwaddr, &hdr->shwaddr);
  ETHADDR16_COPY(&ethhdr->dest, &hdr->shwaddr);
  ETHADDR16_COPY(&hdr->shwaddr, &ethaddr);
  ETHADDR16_COPY(&ethhdr->src, &ethaddr);

  send_arp(pkt, SIZEOF_ETHARP_PACKET, 0, true);
}

int Exo_stack::get_entry_in_arp_table(ip_addr_t* ip_addr) {
  for (unsigned i = 0; i < ARP_TABLE_SIZE; i++) {
    if (ip_addr_cmp(ip_addr, &arp_table[i].ipaddr)) {
      return i;
    }
  }
  return -1;
}

void Exo_stack::send_arp(uint8_t * pkt, uint32_t length, unsigned tx_queue, bool is_arp_reply) {
  assert(tx_queue==0);
  void * temp;
  if (_imem->alloc((addr_t *)&temp, MBUF_ALLOCATOR) != Exokernel::S_OK) {
    panic("MBUF_ALLOCATOR failed!\n");
  }
  assert(temp);
  __builtin_memset(temp, 0, sizeof(struct exo_mbuf));
  
  addr_t pkt_p;
  pkt_p = _imem->get_phys_addr(pkt, PACKET_ALLOCATOR);
  assert(pkt_p);

  struct exo_mbuf * mbuf = (struct exo_mbuf *)temp;
  mbuf->phys_addr = pkt_p;
  mbuf->virt_addr = (addr_t)pkt;
  mbuf->len = length;
  mbuf->ref_cnt = 0;
  mbuf->id = tx_queue;
  mbuf->nb_segment = 1;

  if (is_arp_reply == true)
    mbuf->flag = 0;
  else
    mbuf->flag = 1 | PACKET_ALLOCATOR << 4;

  size_t sent_num = 1;
  if (ethernet_output_simple(&mbuf, sent_num, tx_queue) != Exokernel::S_OK) {
    printf("%s: TX ring buffer is full!\n", __func__);
  };

  _imem->free(temp, MBUF_ALLOCATOR);
}

/**
 * To send UDP packets.
 *
 * @param vaddr Virtual address of the UDP payload.
 * @param paddr Physical address of the UDP payload.
 * @param udp_payload_len The length of UDP payload.
 * @param queue The TX queue where the packets are sent.
 */

void Exo_stack::udp_send_pkt(uint8_t *vaddr, addr_t paddr, uint32_t udp_payload_len, unsigned queue) {
  uint16_t ip_iden= 0;
  struct ip_hdr *iphdr;
  struct udp_hdr *udphdr;
  struct eth_hdr *ethhdr;
  addr_t app_phys = paddr;
  uint32_t app_len;

  int total_udp_len = udp_payload_len + UDP_HLEN;
  int max_ip_payload = ETH_MTU - IP_HLEN; //1500 - 20 = 1480;
  int remaining_bytes = total_udp_len;
  bool need_fragment;
  if (total_udp_len > max_ip_payload) need_fragment = true;

  bool first_ip = true;
  uint16_t curr_offset = 0;
  size_t cnt;

  /* calculate how many IP packets are needed */
  unsigned nb_ip_needed = 0;
  if (udp_payload_len <= max_ip_payload - UDP_HLEN) 
    nb_ip_needed = 1;
  else { 
    unsigned remaining_payload = udp_payload_len - max_ip_payload - UDP_HLEN;
    if (remaining_payload % max_ip_payload == 0)
      nb_ip_needed = 1 + remaining_payload / max_ip_payload;
    else
      nb_ip_needed = 1 + (remaining_payload / max_ip_payload + 1);
  }
  assert(nb_ip_needed <= IXGBE_TX_MAX_BURST);

  /* create mbuf meta data structure */
  void * temp;
  if (_imem->alloc((addr_t *)&temp, META_ALLOCATOR) != Exokernel::S_OK) {
    panic("META_ALLOCATOR failed!\n");
  }
  assert(temp);
  __builtin_memset(temp, 0, sizeof(void *) * IXGBE_TX_MAX_BURST);

  struct exo_mbuf ** mbuf = (struct exo_mbuf **)temp;
  for (unsigned i = 0; i < nb_ip_needed; i++) {
    if (_imem->alloc((addr_t *)&temp, MBUF_ALLOCATOR) != Exokernel::S_OK) {
      panic("MBUF_ALLOCATOR failed!\n");
    }
    assert(temp);
    __builtin_memset(temp, 0, sizeof(struct exo_mbuf));

    mbuf[i] = (struct exo_mbuf *) temp;
  }

  unsigned pkt_index = 0;
  // main loop for IP fragmentation
  while (remaining_bytes > 0) {
    void * temp;
    if (_imem->alloc((addr_t *)&temp, NETHEADER_ALLOCATOR) != Exokernel::S_OK) {
      panic("NETHEADER_ALLOCATOR failed!\n");
    }
    assert(temp);
    __builtin_memset(temp, 0, NETWORK_HDR_SIZE);

    uint8_t * network_hdr=(uint8_t *)temp;

    if (first_ip) {
      udphdr = (struct udp_hdr *)(network_hdr + SIZEOF_ETH_HDR + IP_HLEN);
      // populate UDP header
      udphdr->src=htons(udp_port);
      udphdr->dest=htons(remote_port);
      udphdr->len=htons(total_udp_len);
      udphdr->chksum=0;
    }

    iphdr = (struct ip_hdr *)(network_hdr + SIZEOF_ETH_HDR);

    ip_addr_copy(iphdr->src,ip_addr);
    ip_addr_copy(iphdr->dest,remote_ip_addr);
    iphdr->_id=htons(ip_iden);
    iphdr->_ttl=0x3f; //TTL value
    iphdr->_chksum=0; //Hardware will do the IP checksum
    iphdr->_v_hl_tos=htons(0x4500); //ipv4 and ip header 20 bytes
    iphdr->_proto=0x11; //udp  

    if (remaining_bytes<=max_ip_payload) {
      if (need_fragment == false) {
        iphdr->_offset=htons(0x4000);//no fragment
      } else {
        // last fragment
        iphdr->_offset=htons(curr_offset);
      }
      if (first_ip==true)
        app_len = remaining_bytes - UDP_HLEN;
      else
        app_len = remaining_bytes;

      remaining_bytes = 0;
    }
    else {
      remaining_bytes = remaining_bytes - max_ip_payload;
      iphdr->_offset=htons(curr_offset|(1<<13)); // more fragments flag
      curr_offset += max_ip_payload/8;
      if (first_ip==true)
        app_len = max_ip_payload - UDP_HLEN;
      else
        app_len = max_ip_payload;
    }

    ethhdr = (struct eth_hdr *)network_hdr;
    ethhdr->type=htons(0x0800);// type: ip datagram

    unsigned i = get_entry_in_arp_table(&remote_ip_addr);
    if (i < 0) {
      panic("No arp entry for this outgoing IP address yet\n");
      return;
    }

    ETHADDR16_COPY(&ethhdr->dest, &(arp_table[i].ethaddr));
    ETHADDR16_COPY(&ethhdr->src, &ethaddr);

    uint32_t net_hdr_len = NETWORK_HDR_SIZE;
    if (first_ip==false)
      net_hdr_len = NETWORK_HDR_SIZE - UDP_HLEN;

    addr_t pkt_p;
    pkt_p = _imem->get_phys_addr(network_hdr, NETHEADER_ALLOCATOR);
    assert(pkt_p);

    mbuf[pkt_index]->phys_addr = pkt_p;
    mbuf[pkt_index]->virt_addr = (addr_t)network_hdr;
    mbuf[pkt_index]->len = net_hdr_len;
    mbuf[pkt_index]->ref_cnt = 0;
    mbuf[pkt_index]->id = queue;
    mbuf[pkt_index]->flag = 1 | NETHEADER_ALLOCATOR << 4;
    mbuf[pkt_index]->nb_segment = 2;
    mbuf[pkt_index]->phys_addr_seg[0] = app_phys;
    mbuf[pkt_index]->seg_len[0] = app_len; 

    app_phys += app_len;
    first_ip = false;
    pkt_index ++;
  }

  cnt = nb_ip_needed;
  if (ethernet_output(mbuf, cnt, queue) != Exokernel::S_OK) {
    printf("%s: TX ring buffer is full! No packet sent out!\n", __func__);
  };

  if (cnt != nb_ip_needed)
    printf("only sent %lu packets out of %d\n", cnt, nb_ip_needed);

  /* free memory */
  for (unsigned i = 0; i < nb_ip_needed; i++) {
    _imem->free(mbuf[i], MBUF_ALLOCATOR);
  }
  _imem->free(mbuf, META_ALLOCATOR);

}

void Exo_stack::tcp_server_test(unsigned tid)
{
	PLOG("tid=%u", tid);

	struct tcp_pcb *pcb;
	struct ip_addr local;
	IP4_ADDR(&local, 10, 0, 0, 1);

	pcb = tcp_new();
	tcp_bind(pcb, &local, 8000);
	pcb = tcp_listen(pcb);
	tcp_accept(pcb, &Exo_stack::server_accept);
}

/**
 * Creates a new TCP protocol control block but doesn't place it on
 * any of the TCP PCB lists.
 * The pcb is not put on any list until binding using tcp_bind().
 *
 * @internal: Maybe there should be a idle TCP PCB list where these
 * PCBs are put on. Port reservation using tcp_bind() is implemented but
 * allocated pcbs that are not bound can't be killed automatically if wanting
 * to allocate a pcb with higher prio (@see tcp_kill_prio())
 *
 * @return a new tcp_pcb that initially is in state CLOSED
 */
struct tcp_pcb *Exo_stack::tcp_new(void)
{
	PLOG();

	return tcp_alloc(TCP_PRIO_NORMAL);
}

/**
 * Allocate a new tcp_pcb structure.
 *
 * @param prio priority for the new pcb
 * @return a new tcp_pcb that initially is in state CLOSED
 */
struct tcp_pcb *Exo_stack::tcp_alloc(u8_t prio)
{
	PLOG();

	struct tcp_pcb *pcb;
	u32_t iss;

	pcb = new struct tcp_pcb;
	if (pcb == NULL) {
		/* Try killing oldest connection in TIME-WAIT. */
		PLOG("killing off oldest TIME-WAI connection");
		tcp_kill_timewait();
		pcb = new struct tcp_pcb;
		if (pcb == NULL) {
			PLOG("killing connection with prio lower than %d", prio);
			tcp_kill_prio(prio);
			pcb = new struct tcp_pcb;
			if (pcb != NULL) {
				/* adjust err stats */
			}
		}
		if (pcb != NULL) {
			/* adjust err stats */
		}
	}
	if (pcb != NULL) {
		memset(pcb, 0, sizeof(struct tcp_pcb));
		pcb->prio = prio;
		pcb->snd_buf = TCP_SND_BUF;
		pcb->snd_queuelen = 0;
		pcb->rcv_wnd = TCP_WND;
		pcb->rcv_ann_wnd = TCP_WND;

		pcb->tos = 0;
		pcb->ttl = TCP_TTL;

		pcb->mss = (TCP_MSS > 536) ? 536 : TCP_MSS;
		pcb->rto = 3000 / TCP_SLOW_INTERVAL;
		pcb->sa = 0;
		pcb->sv = 3000 / TCP_SLOW_INTERVAL;
		pcb->rtime = -1;
		pcb->cwnd = 1;
		iss = tcp_next_iss();
		pcb->snd_wl2 = iss;
		pcb->snd_nxt = iss;
		pcb->lastack = iss;
		pcb->snd_lbb = iss;
		pcb->tmr = _tcp_ticks;
		pcb->last_timer = _tcp_timer_ctr;

		pcb->polltmr = 0;

		pcb->recv = &Exo_stack::tcp_recv_null;

		/* Init KEEPALIVE timer */
		pcb->keep_idle = TCP_KEEPIDLE_DEFAULT;

		pcb->keep_cnt_sent = 0;

	}
	PLOG("ends.");
	return pcb;
}

/**
 * Calculates a new initial sequence number for new connections.
 *
 * @return u32_t pseudo random sequence number
 */
u32_t Exo_stack::tcp_next_iss(void)
{
	u32_t iss = 6510;

	iss += _tcp_ticks;
	return iss;
}

/**
 * Default receive callback that is called if the user didn't register
 * a recv callback for the pcb.
 */
err_t Exo_stack::tcp_recv_null(void *arg, struct tcp_pcb *pcb, struct pbuf2 *p, err_t err)
{
	if (p != NULL) {
		tcp_recved(pcb, p->tot_len);
		pbuf2_free(p);
	} else if (err == ERR_OK) {
		return tcp_close(pcb);
	}
	return ERR_OK; 
}

/**
 * Binds the connection to a local portnumber and IP address. If the
 * IP address is not given (i.e., ipaddr == NULL), the IP address of
 * the outgoing network interface is used instead.
 *
 * @param pcb the tcp_pcb to bind (no check is done whether this pcb is
 * 				already bound!)
 * @param ipaddr the local ip address to bind to (use IP_ADDR_ANY to bind
 * 				to any local address
 * @param port the local port to bind to
 * @return ERR_USE if the port is already in use
 * 				 ERR_VAL if bind failed because the PCB is not in a valid state
 * 				 ERR_OK if bound
 */
err_t Exo_stack::tcp_bind(struct tcp_pcb *pcb, ip_addr_t *ipaddr, u16_t port)
{
	PLOG();

	int i;
	int max_pcb_list = NUM_TCP_PCB_LISTS;
	struct tcp_pcb *cpcb;

	if (pcb->state != CLOSED) {
		return ERR_VAL;
	}

	if (port == 0) {
		port = tcp_new_port();
		if (port == 0) {
			return ERR_BUF;
		}
	}
	
	//TODO: check this out
#if 0
	/* Check if the address already is in use (on all lists) */
	for (i = 0; i < max_pcb_list; i++) {
		for (cpcb = *_tcp_pcb_lists[i]; cpcb != NULL; cpcb = cpcb->next) {
			if (cpcb->local_port == port) {
				if (ip_addr_isany(&cpcb->local_ip) ||
						ip_addr_cmp(&cpcb->local_ip, ip_2_ip(ipaddr))) {
					PLOG("[%d] ERR_USE");
					return ERR_USE; 
				}
			}
		}
	}
#endif
	if (!ip_addr_isany(ip_2_ip(ipaddr))) {
		ip_addr_set(&pcb->local_ip, ipaddr);
	}
	pcb->local_port = port;
	TCP_REG(&_tcp_bound_pcbs, pcb);
	PLOG("bind to port %u", port);
	PLOG("ends.");

	return ERR_OK;
}

/**
 * Called from TCP_REG when registering a new PCB:
 * the reason is to have the TCP timer only running when
 * there are active (or time-wait) PCBs.
 */
void Exo_stack::tcp_timer_needed(void)
{
	PLOG();

	/* timer is off but needed again? */
	if (!_tcpip_tcp_timer_active && (_tcp_active_pcbs || _tcp_tw_pcbs)) {
		/* enable and start timer */
		_tcpip_tcp_timer_active = 1;
		sys_timeout((u32_t)TCP_TMR_INTERVAL, &Exo_stack::tcpip_tcp_timer, NULL);
	} else {
		PLOG("else");
	}
}

/**
 * Create a one-shot timer (aka timeout). Timeouts are processed in the
 * following cases:
 * - while waiting for a message using sys_timeouts_mbox_fetch()
 * - by calling sys_check_timeouts() (NO_SYS==1 only)
 *
 * @param msecs time in milliseconds after that the timer should expire
 * @param handler callback function to call when msecs have elapsed
 * @param arg argument to pass to the callback function
 */
void Exo_stack::sys_timeout(u32_t msecs, sys_timeout_handler handler, void *arg)
{
	PLOG();

	struct sys_timeo *timeout, *t;

	timeout = new struct sys_timeo;
	if (timeout == NULL) {
		PLOG("timeout == NULL");
		return;
	}

	timeout->next = NULL;
	timeout->h = handler;
	timeout->arg = arg;
	timeout->time = msecs;

	if (_next_timeout == NULL) {
		_next_timeout = timeout;
		return;
	}

	if (_next_timeout->time > msecs) {
		_next_timeout->time -= msecs;
		timeout->next = _next_timeout;
	} else {
		for (t = _next_timeout; t != NULL; t = t->next) {
			timeout->time -= t->time;
			if (t->next == NULL || t->next->time > timeout->time) {
				if (t->next != NULL) {
					t->next->time -= timeout->time;
				}
				timeout->next = t->next;
				t->next = timeout;
				break;
			}
		}
	}
	PLOG("ends.");
}

/**
 * Timer callback function that calls tcp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
void Exo_stack::tcpip_tcp_timer(void *arg)
{
	PLOG("starts.");

	/* call TCP timer handler */
	tcp_tmr();
	/* timer still needed? */
	if (_tcp_active_pcbs || _tcp_tw_pcbs) {
		/* restart timer */
		sys_timeout(TCP_TMR_INTERVAL, &Exo_stack::tcpip_tcp_timer, NULL);
	} else {
		/* disable timer */
		_tcpip_tcp_timer_active = 0;
	}
	PLOG("ends.");
}

/**
 * Set the state of the connection to be LISTEN, which means that it
 * is able to accept incoming connections. The protocol control block
 * is reallocated in order to consume less memory. Setting the
 * connection to LISTEN is an irreversible process.
 *
 * @param pcb the original tcp_pcb
 * @param backlog the incoming connections queue limit
 * @return tcp_pcb used for listening, consumes less memory.
 *
 * @note The original tcp_pcb is freed. This function therefore has to be
 * 			 called like this:
 * 			 		tpcb = tcp_listen(tpcb);
 */
struct tcp_pcb *Exo_stack::tcp_listen_with_backlog(struct tcp_pcb *pcb, u8_t backlog)
{
	PLOG("starts.");

	struct tcp_pcb_listen *lpcb;

	if (pcb->state != CLOSED) {
		PLOG("pcb already connected");
		return NULL;
	} 
	/* already listening? */
	if (pcb->state == LISTEN) {
		return pcb;
	}

	lpcb = new struct tcp_pcb_listen;
	if (lpcb == NULL) {
		PLOG("lpcb == NULL");
		return NULL;
	}
	lpcb->callback_arg = pcb->callback_arg;
	lpcb->local_port = pcb->local_port;
	lpcb->state = LISTEN;
	lpcb->prio = pcb->so_options;
	ip_set_option(lpcb, SOF_ACCEPTCONN);
	lpcb->ttl = pcb->ttl;
	lpcb->tos = pcb->tos;
	ip_addr_copy(lpcb->local_ip, pcb->local_ip);
	if (pcb->local_port != 0) {
		TCP_RMV(&_tcp_bound_pcbs, pcb);
	}
	delete pcb;
	lpcb->accept = &Exo_stack::tcp_accept_null;
	TCP_REG(&_tcp_listen_pcbs.pcbs, (struct tcp_pcb *)lpcb);

	PLOG("ends.");

	return (struct tcp_pcb *)lpcb;
}

/**
 * Default accept callback if no accept callback is specified by the user.
 */
err_t Exo_stack::tcp_accept_null(void *arg, struct tcp_pcb *pcb, err_t err)
{
	// do nothing
	return ERR_ABRT; 
}

/**
 * Used for specifying the function that should be called when a
 * LISTENing connection has been connected to another host.
 *
 * @param pcb tcp_pcb to set the accept callback
 * @param accept callback function to call for this pcb when LISTENing
 * 				connection has been connected to another host
 */
void Exo_stack::tcp_accept(struct tcp_pcb *pcb, tcp_accept_fn accept)
{
	PLOG();

	/* This function is allowed to be called for both listen pcbs and
		 connection pcbs. */
	pcb->accept = accept;
	PLOG("ends.");
}

err_t Exo_stack::server_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	PLOG("starts.");

	tcp_setprio(pcb, TCP_PRIO_MIN);
	tcp_arg(pcb, NULL);
	tcp_recv(pcb, &Exo_stack::server_recv);
	tcp_err(pcb, &Exo_stack::server_err);
	tcp_poll(pcb, &Exo_stack::server_poll, 4); // every two seconds of inactivity of the TCP connection
	tcp_accepted(pcb);

	PLOG("Accepting incoming connection on server...");

	PLOG("ends.");
	return ERR_OK;
}

err_t Exo_stack::server_err(void *arg, err_t err)
{
	if (err != ERR_OK)
		PLOG("Fatal error, exiting...");

	return err;
}

err_t Exo_stack::server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf2 *p, err_t err)
{
	PLOG("starts.");
	char *string;
	int length;

	if (err == ERR_OK && p != NULL) {
		tcp_recved(pcb, p->tot_len);

		string = (char*)((addr_t)p->payload+20);
		length = strlen(string);

		PLOG("length %u", length);
		PLOG("string %s", string);

		PLOG("pbuf->len is %d bytes", p->len);
		PLOG("pbuf->tot_len is %d bytes", p->tot_len);
		PLOG("pbuf->next is %p", p->next);

		pbuf2_free(p);
		server_close(pcb);
	} else {
		if (err != ERR_OK) {
			PLOG("Connection is not on ERR_OK sate, but in %d state.", err);
		}
		if (p == NULL) {
			PLOG("Pbuf pointer p is a NULL pointer");
		}
		PLOG("Closing server-side connection...");
		pbuf2_free(p);
		server_close(pcb);
	}
	PLOG("ends.");
	return ERR_OK;
}

/**
 * Used to specify the function that should be called when a TCP
 * connection receives data.
 *
 * @param pcb tcp_pcb to set the recv callback
 * @param recv callback function to call for this pcb when data is received
 */
void Exo_stack::tcp_recv(struct tcp_pcb *pcb, tcp_recv_fn recv)
{
	PLOG("starts.");
	assert(pcb->state != LISTEN);

	pcb->recv = recv;
	PLOG("ends.");
}

/**
 * This function should be called by the application when it has
 * processed the data. The purpose is to advertise a larger window
 * when the data has been processed.
 *
 * @param pcb the tcp_pcb for which data is read
 * @param len the amount of bytes that have been read by the application
 */
void Exo_stack::tcp_recved(struct tcp_pcb *pcb, u16_t len)
{
	int wnd_inflation;

	/* pcb->state LISTEN not allowed here */
	assert(pcb->state != LISTEN);
	assert(len <= TCPWND_MAX - pcb->rcv_wnd);

	pcb->rcv_wnd += len;
	if (pcb->rcv_wnd > TCP_WND) {
		pcb->rcv_wnd = TCP_WND;
	}

	wnd_inflation = tcp_update_rcv_ann_wnd(pcb);

	/* If the change in the right edge of window is significant (default
	 * watermark is TCP_WND/4), then send an explicit update now.
	 * Otherwise wait for a packet to be sent in the normal course of
	 * events (or more window to be available alter) */
	if (wnd_inflation >= TCP_WND_UPDATE_THRESHOLD) {
		tcp_ack_now(pcb);
		tcp_output(pcb);
	}
	PLOG("received %u bytes, wnd %u (%u).", len, pcb->rcv_wnd, 
			TCP_WND - pcb->rcv_wnd);
}

/**
 * Update the state that tracks the available window space to advertise.
 *
 * Returns how much extra window would be advertised if we sent an
 * update now.
 */
u32_t Exo_stack::tcp_update_rcv_ann_wnd(struct tcp_pcb *pcb)
{
	u32_t new_right_edge = pcb->rcv_nxt + pcb->rcv_wnd;

	if (TCP_SEQ_GEQ(new_right_edge, pcb->rcv_ann_right_edge + EXO_MIN((TCP_WND / 2), pcb->mss))) {
		/* we can advertise more window */
		pcb->rcv_ann_wnd = pcb->rcv_wnd;
		return new_right_edge - pcb->rcv_ann_right_edge;
	} else {
		if (TCP_SEQ_GT(pcb->rcv_nxt, pcb->rcv_ann_right_edge)) {
			/* Can happen due to other end sending out of advertised window.
			 * but within actual available (but not et advertised) window */
			pcb->rcv_ann_wnd = 0;
		} else {
			/* keep the right edge of window constant */
			u32_t new_rcv_ann_wnd = pcb->rcv_ann_right_edge - pcb->rcv_nxt;
			assert(new_rcv_ann_wnd <= 0xffff);
			pcb->rcv_ann_wnd = (tcpwnd_size_t)new_rcv_ann_wnd;
		}
		return 0;
	}
}

#if 0
err_t Exo_stack::server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	// do nothing
	PLOG("Correctly ACK'ed, closing server-side connection...");
	server_close(pcb);

	return ERR_OK;
}
#endif

/**
 * Used to specifiy the function that should be called when TCP data
 * has been successfully delivered to the remote host.
 *
 * @param pcb tcp_pcb to set the sent callback
 * @param sent callback function to call for this pcb when data is successfully sent
 */
void Exo_stack::tcp_sent(struct tcp_pcb *pcb, tcp_sent_fn sent)
{
	assert(pcb->state != LISTEN);
	pcb->sent = sent;
}

/**
 * Closes the connection held by the PCB.
 *
 * Listening pcbs are freed and may not be referenced any more.
 * Connection pcbs are freed if not yet connected and may not be referenced
 * any more. If a connectino is established (at least SYN received or in
 * a closing state), the connection is closed, and put in a closing state.
 * The pcb is then automatically freed in tcp_slowtmr(). It is therefore
 * unsafe to reference it (unless an error is returned).
 *
 * @param pcb the tcp_pcb to close
 * @return ERR_OK if connection has been closed
 * 				 another err_t if closing failed and pcb is not freed
 */
err_t Exo_stack::tcp_close(struct tcp_pcb *pcb)
{
	PLOG("starts.");
	PLOG("closing in ");
	printf("%s", __func__); tcp_debug_print_state(pcb->state);

	if (pcb->state != LISTEN) {
		/*Set a flag not to receive any more data... */
		pcb->flags |= TF_RXCLOSED;
	}
	/* ... and close */
	PLOG("ends.");
	return tcp_close_shutdown(pcb, 1);
}

/**
 * Closes the TX side of a connection held by the PCB.
 * For tcp_close(), a RST is sent if the application didn't receive all data
 * (tcp_recved() not called for all data passed to recv callback).
 *
 * Listening pcbs are freed and may not be referenced any more.
 * Connection pcbs are freed if not yet connected and may not be referenced
 * any more. If a connection is established (at least SYN received or in
 * a closing state), the connection is closed, and put in a closing state.
 * The pcb is then automatically freed in tcp_slowtmr(). It is therefore
 * unsafe to reference it.
 *
 * @param pcb the tcp_pcb to close
 * @return ERR_OK if connection has been closed
 *         another err_t if closing failed and pcb is not freed
 */
err_t Exo_stack::tcp_close_shutdown(struct tcp_pcb *pcb, u8_t rst_on_unacked_data)
{
	PLOG("starts.");

	err_t err;

	if (rst_on_unacked_data && 
			((pcb->state == ESTABLISHED) || (pcb->state == CLOSE_WAIT))) {
		if ((pcb->refused_data != NULL) || (pcb->rcv_wnd != TCP_WND)) {
			/* Not all data received by application, send RST to tell the remote
			   side about this. */
			assert(pcb->flags & TF_RXCLOSED);

			/* don't call tcp_abort here: we must not deallocate the pcb since
			   that might not be expected when calling tcp_close */
			tcp_rst(pcb->snd_nxt, pcb->rcv_nxt, &pcb->local_ip, &pcb->remote_ip,
							pcb->local_port, pcb->remote_port);
			
			tcp_pcb_purge(pcb);
			TCP_RMV_ACTIVE(pcb);
			if (pcb->state == ESTABLISHED) {
				/* move to TIME_WAIT since we close actively */
				pcb->state = TIME_WAIT;
				TCP_REG(&_tcp_tw_pcbs, pcb);
			} else {
				/* CLOSE_WAIT: deallocate the pcb since we already sent a RST for it */
				//memp_free(MEMP_TCP_PCB, pcb);
				delete pcb;
			}
			return ERR_OK;
		}
	}

	switch (pcb->state) {
		case CLOSED:
			/* Closing a pcb in the CLOSED state might seem erroneous,
			 * however, it is in this state once allocated and as yet unused
			 * and the user needs some way to free it should the need arise.
			 * Calling tcp_close() with a pcb that has already been closed, (i.e. twice)
			 * or for a pcb that has been used and then entered the CLOSED state
			 * is erroneous, but this should never happen as the pcb has in those cases
			 * been freed, and so any remaining handles are bogus. */
			err = ERR_OK;
			if (pcb->local_port != 0) {
				TCP_RMV(&_tcp_bound_pcbs, pcb);
			}
			delete pcb;
			pcb = NULL;
			break;
		case LISTEN:
			err = ERR_OK;
			tcp_pcb_remove(&_tcp_listen_pcbs.pcbs, pcb);
			delete pcb;
			break;
		case SYN_SENT:
			err = ERR_OK;
			TCP_PCB_REMOVE_ACTIVE(pcb);
			delete pcb;
			break;
		case SYN_RCVD:
			err = tcp_send_fin(pcb);
			if (err == ERR_OK) {
				pcb->state = FIN_WAIT_1;
			}
			break;
		case ESTABLISHED:
			err = tcp_send_fin(pcb);
			if (err == ERR_OK) {
				pcb->state = FIN_WAIT_1;
			}
			break;
		case CLOSE_WAIT:
			err = tcp_send_fin(pcb);
			if (err == ERR_OK) {
				pcb->state = LAST_ACK;
			}
			break;
		default:
			/* Has already been closed, do nothing. */
			err = ERR_OK;
			pcb = NULL;
			break;
	}
	if (pcb != NULL && err == ERR_OK) {
		/* To ensure all data has been sent when tcp_close returns, we have
		   to make sure tcp_output doesn't fail.
			 Since we don't really have to ensure all data has been sent when tcp_close
			 returns (unsent data is sent from tcp timer functions, also), we don't care
			 for the return value of tcp_output for now. */
		/* @todo: When implementing SO_LINGER, this must be changed somehow:
		   if SOF_LINGER is set, the data should be sent and acked before close returns.
			 This can only be valid for sequential APIs, not for the raw API. */
		tcp_output(pcb);
	}
	PLOG("ends.");
	return err;
}

/**
 * Send a TCP RESET packet (empty segment with RST flag set) either to
 * abort a connection or to show that there is no matching local connection
 * for a received segment.
 *
 * Called by tcp_abort() (to abort a local connection), tcp_input() (if no
 * matching local pcb was found), tcp_listen_input() (if incoming segment
 * has ACK flag set) and tcp_process() (received segment in the wrong state)
 *
 * Since a RST segment is in most cases not sent for an active connection,
 * tcp_rst() has a number of arguments that are taken from a tcp_pcb for
 * most other segment output functions.
 *
 * @param seqno the sequence number to use for the outgoing segment
 * @param ackno the acknowledge number to use for the outgoing segment
 * @param local_ip the local IP address to send the segment from
 * @param remote_ip the remote IP address to send the segment to
 * @param local_port the local TCP port to send the segment from
 * @param remote_port the remote TCP port to send the segment to
 */
void Exo_stack::tcp_rst(u32_t seqno, u32_t ackno,
												ip_addr_t *local_ip, ip_addr_t *remote_ip,
												u16_t local_port, u16_t remote_port)
{
	struct pbuf2 *p;
	struct tcp_hdr *tcphdr;
	p = pbuf2_alloc(PBUF_IP, TCP_HLEN, PBUF_RAM);
	if (p == NULL) {
		PLOG("could not allocate memory for pbuf");
		return;
	}
	assert(p->len >= sizeof(struct tcp_hdr));
	
	tcphdr = (struct tcp_hdr *)p->payload;
	tcphdr->src = htons(local_port);
	tcphdr->dest = htons(remote_port);
	tcphdr->seqno = htonl(seqno);
	tcphdr->ackno = htonl(ackno);
	TCPH_HDRLEN_FLAGS_SET(tcphdr, TCP_HLEN/4, TCP_RST | TCP_ACK);
	tcphdr->wnd = TCP_WND; //PP_HTONS(TCP_WND);
	tcphdr->chksum = 0;
	tcphdr->urgp = 0;

	//TODO: checksum is NOT IMPLEMENTED! for now
	//tcphdr->chksum = inet_chksum_pseudo(p, IP_PROTO_TCP, p->tot_len, 
	//		local_ip, remote_ip);

	ip_output(p, local_ip, remote_ip, TCP_TTL, 0, IP_PROTO_TCP);
	pbuf2_free(p);
	PLOG("seqno %u ackno %u.\n", seqno, ackno);
}

/**
 * Purges a TCP. Removes any buffered data and frees the buffer memory
 * (pcb->ooseq, pcb->unsent and pcb->unacked are freed).
 *
 * @param pcb tcp_pcb to purge. The pcb itself is not deallocated!
 */
void Exo_stack::tcp_pcb_purge(struct tcp_pcb *pcb)
{
	if (pcb->state != CLOSED &&
			pcb->state != TIME_WAIT &&
			pcb->state != LISTEN) {
		if (pcb->refused_data != NULL) {
			PLOG("data left on ->refused_data");
			pbuf2_free(pcb->refused_data);
			pcb->refused_data = NULL;
		}
		if (pcb->unsent != NULL) {
			PLOG("not all data sent");
		}
		if (pcb->unacked != NULL) {
			PLOG("data left on ->unacked");
		}
		if (pcb->ooseq != NULL) {
			PLOG("data left on ->ooseq");
		}
		tcp_segs_free(pcb->ooseq);
		pcb->ooseq = NULL;

		/* Stop the retransmission timer as it will expect data on unacked
		   queue if it fires */
		pcb->rtime = -1;

		tcp_segs_free(pcb->unsent);
		tcp_segs_free(pcb->unacked);
		pcb->unacked = pcb->unsent = NULL;
		pcb->unsent_oversize = 0;
	}
}

/**
 * Deallocates a list of TCP segments (tcp_seg structures).
 *
 * @param seg tcp_seg list of TCP segments to free
 */
void Exo_stack::tcp_segs_free(struct tcp_seg2 *seg)
{
	while (seg != NULL) {
		struct tcp_seg2 *next = seg->next;
		tcp_seg_free(seg);
		seg = next;
	}
}

/**
 * Deallocates a TCP segment (tcp_seg structure).
 *
 * @param seg single tcp_seg to free
 */
void Exo_stack::tcp_seg_free(struct tcp_seg2 *seg)
{
	if (seg != NULL) {
		if (seg->p != NULL) {
			pbuf2_free(seg->p);
		}
		delete seg;
	}
}

/**
 * Sets the priority of a connection.
 *
 * @param pcb the tcp_pcb to manipulate
 * @param prio new priority
 */
void Exo_stack::tcp_setprio(struct tcp_pcb *pcb, u8_t prio)
{
	pcb->prio = prio;
}

void Exo_stack::server_close(struct tcp_pcb *pcb)
{
	tcp_arg(pcb, NULL);
	tcp_sent(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_close(pcb);

	PLOG("Closing...");
}

/**
 * Used to specify the argument that should be passed callback
 * functions.
 *
 * @param pcb tcp_pcb to set the callback argument
 * @param arg void pointer argument to pass to callback functions
 */
void Exo_stack::tcp_arg(struct tcp_pcb *pcb, void *arg)
{
	/* This function is allowed to be called for both listen pcbs and
	   connection pcbs. */
	pcb->callback_arg = arg;
}

/**
 * The initial input processing of TCP. It verifies the TCP header. demultiplexes
 * the segment between the PCBs and passes it on to tcp_process(). which implements
 * the TCP finite state machine. This function is called by the IP layer (in
 * ip_input()).
 *
 * @param p received TCP segment to process (p->payload pointing to the TCP header)
 * @param queue
 */
pkt_status_t Exo_stack::tcp_input(struct pbuf2 *p, unsigned queue) 
{
	PLOG();
	assert(p);
	struct ip_hdr *iphdr;
	pkt_status_t t = REUSE_THIS_PACKET;

	struct tcp_pcb *pcb, *prev;
	struct tcp_pcb_listen *lpcb;
	u8_t hdrlen;
	err_t err;

  iphdr = (struct ip_hdr *)(p->payload);
	//ip_debug_print(p);
	_tcphdr = (struct tcp_hdr *)((addr_t)p->payload + IPH_HL(iphdr)*4);

	assert(_tcphdr);
	tcp_debug_print(_tcphdr);

	/* Check that TCP header fits in payload */
	if (p->len < sizeof(struct tcp_hdr)) {
		/* drop short packets */
		PLOG("short packet (%u bytes)", p->tot_len);
		goto dropped;
	}

	/* Don't even process incoming broadcasts/multicasts. */
	if (ip_addr_isbroadcast(ip_current_dest_addr())) {
		goto dropped;
	}
	/* Move the payload pointer in the pbuf so that it points to the
	   TCP data instead of the TCP header. */
	hdrlen = TCPH_HDRLEN(_tcphdr);
	PLOG("hdrlen %u", hdrlen);
	if (pbuf2_header(p, -(hdrlen * 4))) {
		/* drop short packets */
		PLOG("short packet");
		goto dropped;
	}

	/* Convert fields in TCP header to host byte order. */
	_tcphdr->src = ntohs(_tcphdr->src);
	_tcphdr->dest = ntohs(_tcphdr->dest);
	_seqno = _tcphdr->seqno = ntohl(_tcphdr->seqno);
	_ackno = _tcphdr->ackno = ntohl(_tcphdr->ackno);
	_tcphdr->wnd = ntohs(_tcphdr->wnd);

	_flags = TCPH_FLAGS(_tcphdr);
	_tcplen = p->len + ((_flags & (TCP_FIN | TCP_SYN)) ? 1: 0);

	/* Demultiplex an incoming segment. First, we check if it is desitined
	   for an active connection. */
	prev = NULL;
	for (pcb = _tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
		assert(pcb->state != CLOSED);
		assert(pcb->state != TIME_WAIT);
		assert(pcb->state != LISTEN);
		if (pcb->remote_port == _tcphdr->src &&
				pcb->local_port == _tcphdr->dest &&
				ip_addr_cmp(&pcb->remote_ip, ip_current_src_addr()) &&
				ip_addr_cmp(&pcb->local_ip, ip_current_dest_addr())) {
			/* Move this PCB to the front of the list so that subsequent
			   lookups will be faster (we exploit locality in TCP segment
				 arrivals). */
			assert(pcb->next != pcb);
			if (prev != NULL) {
				prev->next = pcb->next;
				pcb->next = _tcp_active_pcbs;
				_tcp_active_pcbs = pcb;
			}
			assert(pcb->next != pcb);
			break;
		}
		prev = pcb;
	}

	if (pcb == NULL) {
		PLOG("[%d] PCB == NULL", __LINE__);
		/* If it did not go to an active connection, we check the connections
		   in the TIME-WAIT state. */
		for (pcb = _tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
			assert(pcb->state == TIME_WAIT);
			if (pcb->remote_port == _tcphdr->src &&
					pcb->local_port == _tcphdr->dest &&
					ip_addr_cmp(&pcb->remote_ip, ip_current_src_addr()) &&
					ip_addr_cmp(&pcb->local_ip, ip_current_dest_addr())) {
				/* We don't really care enough to move this PCB to the front
				   of the list since we are not very likely receive that
					 many segment for connections in TIME-WAIT. */
				PLOG("packed for TIME_WAITing connection.");
				tcp_timewait_input(pcb);
				pbuf2_free(p);
				return t;
			}
		}

		/* Finally, if we still did not get a match, we check all PCBs that
		   are LISTENING for incoming connections. */
		prev = NULL;
		for (lpcb = _tcp_listen_pcbs.listen_pcbs; lpcb != NULL; lpcb = lpcb->next) {
			PLOG("lpcb=%p", (void*)lpcb);
			if (lpcb->local_port == _tcphdr->dest) {
				if (ip_addr_cmp(&lpcb->local_ip, ip_current_dest_addr())) {
					/* found an exact match */
					PLOG("found an exact match");
					break;
				} else if (ip_addr_isany(&lpcb->local_ip)) {
					PLOG("found an ANY-match");
					/* found an ANY-match */
					break;
				}
			}
			prev = (struct tcp_pcb *)lpcb;
		}
		if (lpcb != NULL) {
			/* Move this PCB to the front of the list so that subsequent
			   lookups will be faster (we exploit locality in TCP segment
				 arrivals). */
			if (prev != NULL) {
				((struct tcp_pcb_listen *)prev)->next = lpcb->next;
					/* our successor is the remainder of the listening list */
				lpcb->next = _tcp_listen_pcbs.listen_pcbs;
					/* put this listening pcb at the head of the listening list */
				_tcp_listen_pcbs.listen_pcbs = lpcb;
			}
			PLOG("packed for LISTENing connection.");
			tcp_listen_input(lpcb);
			pbuf2_free(p);
			return t;
		}
	}

	if (pcb != NULL) {
		PLOG("[%d] PCB != NULL", __LINE__);
		/* The incoming segment belongs to a connection. */
		printf("%s", __func__); tcp_debug_print_state(pcb->state);

		/* Set up a tcp_seg structure. */
		_inseg.next = NULL;
		_inseg.len = p->tot_len;
		_inseg.p = p;
		_inseg.tcphdr = _tcphdr;

		_recv_data = NULL;
		_recv_flags = 0;

		if (_flags & TCP_PSH) {
			p->flags |= PBUF_FLAG_PUSH;
		}
		
		/* If there is data which was previously "refused" by upper layer */
		if (pcb->refused_data != NULL) {
			if ((tcp_process_refused_data(pcb) == ERR_ABRT) ||
					((pcb->refused_data != NULL) && (_tcplen > 0))) {
				/* pcb has been aborted or refused data is still refused and the new
				   segment contains data */
				PLOG("GOTO ABORTED: line %d", __LINE__);
				goto aborted;
			}
		}
		_tcp_input_pcb = pcb;
		err = tcp_process(pcb);
		/* A return value of ERR_ABRT means that tcp_abort() was called
		   and that the pcb has been freed. If so, we don't do anything. */
		if (err != ERR_ABRT) {
			if (_recv_flags & TF_RESET) {
				/* TF_RESET means that the connection was reset by the other
				   end. We then call the error callback to inform the
					 application that the connection is dead before we
					 deallocate the PCB. */
				TCP_EVENT_ERR(pcb->errf, pcb->callback_arg, ERR_RST);
				tcp_pcb_remove(&_tcp_active_pcbs, pcb);
				delete pcb;
			} else if (_recv_flags & TF_CLOSED) {
				/* The connection has been closed  and we will deallocate the
				 	 PCBs. */
				if (!(pcb->flags & TF_RXCLOSED)) {
					/* Connection closed although the application has only shut down the
					   tx side: call the PCB's err callback and indicate the closure to
						 ensure the application doesn't continue using the PCB. */
					TCP_EVENT_ERR(pcb->errf, pcb->callback_arg, ERR_CLSD);
				}
				tcp_pcb_remove(&_tcp_active_pcbs, pcb);
				delete pcb;
			} else {
				err = ERR_OK;
				/* If the application has registered a "sent" function to be
				   called when new send buffer space is available, we call it
					 now. */
				if (pcb->acked > 0) {
					u16_t acked;
					{
						acked = pcb->acked;
						TCP_EVENT_SENT(pcb, acked, err);
						if (err == ERR_ABRT) {
							PLOG("GOTO ABORTED: line %d", __LINE__);
							goto aborted;
						}
					}
				}
				if (_recv_data != NULL) {
					assert(pcb->refused_data == NULL);
					if (pcb->flags & TF_RXCLOSED) {
						/* received data although already closed -> abort(send RST) to
						   notify the remote host that not all data has been processed */
						pbuf2_free(_recv_data);
						tcp_abort(pcb);
						PLOG("GOTO ABORTED: line %d", __LINE__);
						goto aborted;
					}

					/* Notify application that data has been received. */
					TCP_EVENT_RECV(pcb, _recv_data, ERR_OK, err);
					if (err == ERR_ABRT) {
						PLOG("GOTO ABORTED: line %d", __LINE__);
						goto aborted;
					}

					/* If the upper layer can't receive this data, store it */
					if (err != ERR_OK) {
						pcb->refused_data = _recv_data;
						PLOG("keep incoming packet, because pcb is full");
					}
				}

				/* If a FIN segment was received, we call the callback
				   function with a NULL buffer to indicate EOF. */
				if (_recv_flags & TF_GOT_FIN) {
					if (pcb->refused_data != NULL) {
						/* Delay this if we have refused data. */
						pcb->refused_data->flags |= PBUF_FLAG_TCP_FIN;
					} else {
						/* correct rcv_wnd as the application won't call tcp_recved()
						   for the FIN's seqno */
						if (pcb->rcv_wnd != TCP_WND) {
							pcb->rcv_wnd++;
						}
						TCP_EVENT_CLOSED(pcb, err);
						if (err == ERR_ABRT) {
							PLOG("GOTO ABORTED: line %d", __LINE__);
							goto aborted;
						}
					}
				}
				_tcp_input_pcb = NULL;
				/* Try to send something out. */
				tcp_output(pcb);
				printf("%s", __func__); tcp_debug_print_state(pcb->state);
			}
		}
		/* Jump target if pcb has been aborted in a callback (by calling tcp_abort()).
		   Below this line, 'pcb' may not be dereferenced! */

aborted:
		_tcp_input_pcb = NULL;
		_recv_data = NULL;
	
		/* give up our reference to inseg.p */
		if (_inseg.p != NULL) {
			pbuf2_free(_inseg.p);
			_inseg.p = NULL;
		}
	} else {
		/* If no matching PCB was found, send a TCP RST (reset) to the
		   sender. */
		PLOG("no PCB match found, resetting");
		if (!(TCPH_FLAGS(_tcphdr) & TCP_RST)) {
			tcp_rst(_ackno, _seqno + _tcplen, ip_current_dest_addr(),
				ip_current_src_addr(), _tcphdr->dest, _tcphdr->src);
		}
		pbuf2_free(p);
	}
	assert(tcp_pcbs_sane());
	return t;

dropped:
	PLOG("dropped");
	pbuf2_free(p);

	PLOG("ends.");
}

/** Pass pcb->refused_data to the recv callback */
err_t Exo_stack::tcp_process_refused_data(struct tcp_pcb *pcb)
{
  err_t err;
  u8_t refused_flags = pcb->refused_data->flags;
  /* set pcb->refused_data to NULL in case the callback frees it and then
     closes the pcb */
  struct pbuf2 *refused_data = pcb->refused_data;
  pcb->refused_data = NULL;
  /* Notify again application with data previously received. */
  PLOG("notify kept packet");
  TCP_EVENT_RECV(pcb, refused_data, ERR_OK, err);
  if (err == ERR_OK) {
    /* did refused_data include a FIN? */
    if (refused_flags & PBUF_FLAG_TCP_FIN) {
      /* correct rcv_wnd as the application won't call tcp_recved()
         for the FIN's seqno */
      if (pcb->rcv_wnd != TCP_WND) {
        pcb->rcv_wnd++;
      }
      TCP_EVENT_CLOSED(pcb, err);
      if (err == ERR_ABRT) {
        return ERR_ABRT;
      }
    }
  } else if (err == ERR_ABRT) {
    /* if err == ERR_ABRT, 'pcb' is already deallocated */
    /* Drop incoming packets because pcb is "full" (only if the incoming
       segment contains data). */
    PLOG("drop incoming packets, because pcb is full");
    return ERR_ABRT;
  } else {
    /* data is still refused, pbuf is still valid (go on for ACK-only packets) */
    pcb->refused_data = refused_data;
  }
  return ERR_OK;
}

void Exo_stack::tcp_client_test(unsigned tid)
{
	PLOG("tid=%u", tid);

	struct tcp_pcb *pcb;
	struct ip_addr dest;
	struct ip_addr local;
	err_t ret_val;

	IP4_ADDR(&dest, 10, 0, 0, 1);
	IP4_ADDR(&local, 10, 0, 0, 2);

	pcb = tcp_new();
	tcp_bind(pcb, &local, 7000);

	ret_val = tcp_connect(pcb, &dest, 8000, &Exo_stack::client_connected);
	if (ret_val != 0)
		PLOG("ERROR: return value %d", ret_val);

}

/**
 * Connects to another host. The function given as the "connected"
 * argument will be called when the connection has been established.
 *
 * @param pcb the tcp_pcb used to establish the connection
 * @param ipaddr the remote ip address to connect to
 * @param port the remote tcp port to connect to
 * @param connected callback function to call when connected (or on error)
 * @return ERR_VAL if invalid arguments are given
 * 				 ERR_OK if connect request has been sent
 * 				 other err_t values if connect request couldn't be sent
 */
err_t Exo_stack::tcp_connect(struct tcp_pcb *pcb, ip_addr_t *ipaddr, u16_t port, tcp_connected_fn connected)
{
	PLOG();

	err_t ret;
	u16_t old_local_port;
	u32_t iss;

	if (pcb->state != CLOSED) {
		PLOG("can only connect from state CLOSED");
		return ERR_ISCONN;
	}
	PLOG("to port %u", port);
	if (ipaddr != NULL) {
		ip_addr_set(&pcb->remote_ip, ipaddr);
	} else {
		PLOG("ERR_VAL");
		return ERR_VAL;
	}
	pcb->remote_port = port;

	/* check if we have a route to the remote host */
	//TODO: implement this once ICMP testing is completed
	
	old_local_port = pcb->local_port;
	if (pcb->local_port == 0) {
		pcb->local_port = tcp_new_port();
		if (pcb->local_port == 0) {
			PLOG("ERR_BUF");
			return ERR_BUF;
		}
	}
	iss = tcp_next_iss();
	pcb->rcv_nxt = 0;
	pcb->snd_nxt = iss;
	pcb->lastack = iss - 1;
	pcb->snd_lbb = iss - 1;
	pcb->rcv_wnd = TCP_WND;
	pcb->rcv_ann_wnd = TCP_WND;
	pcb->rcv_ann_right_edge = pcb->rcv_nxt;
	pcb->snd_wnd = TCP_WND;
	/* As initial send MSS, we use TCP_MSS but limit it to 536.
	   The send MSS is updated when an MSS option is received. */
	pcb->mss = (TCP_MSS > 536) ? 536 : TCP_MSS;
	pcb->mss=  tcp_eff_send_mss(pcb->mss, &pcb->local_ip);
	pcb->cwnd = 1;
	pcb->ssthresh = pcb->mss * 10;
	pcb->connected = connected;

	/* Send a SYN together with the MSS option */
	ret = tcp_enqueue_flags(pcb, TCP_SYN);
	if (ret == ERR_OK) {
		/* SYN segment was enqueued, changed the pcbs state now */
		pcb->state = SYN_SENT;
		if (old_local_port != 0) {
			TCP_RMV(&_tcp_bound_pcbs, pcb);
		}
		TCP_REG_ACTIVE(pcb);

		tcp_output(pcb);
	}
	return ret;
}

err_t Exo_stack::client_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
	PLOG("starts.");
	char string[] = "Hello!";
	if (err != ERR_OK) {
		PLOG("err argument not set to ERR_OK(0), but the value is %d", err);
	} else {
		tcp_sent(pcb, &Exo_stack::client_sent);
		//tcp_write(pcb, string, sizeof(string), 0);
		tcp_write(pcb, string, sizeof(string), TCP_WRITE_FLAG_COPY);
	}
	PLOG("ends.");
	return err;
}

/**
 * Allocate a new local TCP port.
 *
 * @return a new (free) local TCP port number
 */
u16_t Exo_stack::tcp_new_port(void)
{
	u8_t i;
	u16_t n = 0;
	struct tcp_pcb *pcb;

again:
	if (_tcp_port++ == TCP_LOCAL_PORT_RANGE_END) {
		_tcp_port = TCP_LOCAL_PORT_RANGE_START;
	}
	/* Check all PCB lists. */
	for (i = 0; i < NUM_TCP_PCB_LISTS; i++) {
		for (pcb = *_tcp_pcb_lists[i]; pcb != NULL; pcb = pcb->next) {
			if (pcb->local_port == _tcp_port) {
				if (++n > (TCP_LOCAL_PORT_RANGE_END - TCP_LOCAL_PORT_RANGE_START)) {
					return 0;
				}
			}
			goto again;
		}
	}
	return _tcp_port;
}

/**
 * Enqueue TCP options for transmission.
 *
 * Called by tcp_connect(), tcp_listen_input(), and tcp_send_ctrl().
 *
 * @param pcb Protocol control block for the TCP connection.
 * @param flags TCP header flags to set in the outgoing segment.
 * @param optdata pointer to TCP options, or NULL.
 * @param optlen length of TCP options in bytes.
 */
err_t Exo_stack::tcp_enqueue_flags(struct tcp_pcb *pcb, u8_t flags)
{
	PLOG();

	struct pbuf2 *p;
	struct tcp_seg2 *seg;
	u8_t optflags = 0;
	u8_t optlen = 0;

	PLOG("queuelen %u", pcb->snd_queuelen);
	
	assert((flags & (TCP_SYN | TCP_FIN)) != 0);

	/* check for configured max queuelen and possible overflow */
	if ((pcb->snd_queuelen >= TCP_SND_QUEUELEN) || (pcb->snd_queuelen > TCP_SNDQUEUELEN_OVERFLOW)) {
		PLOG("too long queue %u (max %u)", pcb->snd_queuelen, TCP_SND_QUEUELEN);
		pcb->flags != TF_NAGLEMEMERR;
		return ERR_MEM;
	}

	if (flags & TCP_SYN) {
		optflags = TF_SEG_OPTS_MSS;
	}
	optlen = EXO_TCP_OPT_LENGTH(optflags);

	/* tcp_enqueue_flags is always called with either SYN or FIN in flags.
	 * We need one available snd_buf bytes to do that.
	 * This means we can't send FIN while snd_buf==0. A better fix would be to
	 * not include SYN and FIN sequenece numbers in the snd_buf count. */
	if (pcb->snd_buf == 0) {
		PLOG("no send buffer availble");
		return ERR_MEM;
	}
	
	/* Allocate pbuf with room for TCP header + options */
	PLOG("optlen=%d", optlen);
	if ((p = pbuf2_alloc(PBUF_TRANSPORT, optlen, PBUF_RAM)) == NULL) {
		pcb->flags = TF_NAGLEMEMERR;
		return ERR_MEM;
	}
	assert(p->len >= optlen);

	/* Allocate memory for tcp_seg, and fill in field. */
	if ((seg = tcp_create_segment(pcb, p, flags, pcb->snd_lbb, optflags)) == NULL) {
		pcb->flags |= TF_NAGLEMEMERR;
		PLOG("ERR_MEM");
		return ERR_MEM;
	}

	/* Now append seg to tcb->unsent queuee */
	if (pcb->unsent == NULL) {
		pcb->unsent = seg;
	} else {
		struct tcp_seg2 *useg;
		for (useg = pcb->unsent; useg->next != NULL; useg = useg->next);
		useg->next = useg;
	}
	/* The new unsent tail has no space */
	pcb->unsent_oversize = 0;

	/* SYN and FIN bump the sequence number */
	if ((flags & TCP_SYN) || (flags & TCP_FIN)) {
		pcb->snd_lbb++;
		/* optlen does not influence snd_buf */
		pcb->snd_buf--;
	}
	if (flags & TCP_FIN) {
		pcb->flags |= TF_FIN;
	}

	/* update number of segments on the queues */
	pcb->snd_queuelen += pbuf2_clen(seg->p);
	PLOG("%d (after enqueued)", pcb->snd_queuelen);
	if (pcb->snd_queuelen != 0) {
		/* invalid queue length */
		assert(pcb->unacked != NULL || pcb->unsent != NULL);
	}

	return ERR_OK;
}

/**
 * Find out what we can send and send it
 *
 * @param pcb Protocol control block for the TCP connection to send data
 * @return ERR_OK if data has been sent or nothing to send
 * 				 another err_t on error
 */
err_t Exo_stack::tcp_output(struct tcp_pcb *pcb)
{
	PLOG("starts");

	struct tcp_seg2 *seg, *useg;
	u32_t wnd, snd_nxt;


	assert(pcb->state != LISTEN);

	/* First, check if we are invoked by the TCP input processing
	   code. If so, we do not output anything. Instead, we reply on the
		 input processing code to call us when input processing is done
		 with. */
	if (_tcp_input_pcb == pcb) {
		return ERR_OK;
	}

	wnd = EXO_MIN(pcb->snd_wnd, pcb->cwnd);

	seg = pcb->unsent;

	/* If the TF_ACK_NOW flag is set and no data will be sent (either
	 * because the ->unsent queue is empty or because the window does
	 * not allow it), construct an empty ACK segment and send it.
	 *
	 * If data is to be sent, we will just piggyback the ACK (see below).
	 */
	if (pcb->flags & TF_ACK_NOW &&
			(seg == NULL ||
			ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len > wnd)) {
		return tcp_send_empty_ack(pcb);
	}

	/* useg should point to last segment on unacked queue */
	useg = pcb->unacked;
	if (useg != NULL) {
		for (; useg->next != NULL; useg = useg->next);
	}

	if (seg == NULL) {
		PLOG("nothing to send (%p)", (void *)pcb->unsent);
		PLOG("snd_wnd %u cwnd %u seg == NULL ack %u",
				pcb->snd_wnd, pcb->cwnd, wnd, pcb->lastack);
	} else {
		PLOG("snd_wnd %u cwnd %u wnd %u effwnd %u seq %u ack %u",
				pcb->snd_wnd, pcb->cwnd, wnd,
				ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len,
				ntohl(seg->tcphdr->seqno), pcb->lastack);
	}

	/* data available and window allows it to be sent? */
	while (seg != NULL &&
			ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len <= wnd) {
		/* RST not expected here! */
		assert((TCPH_FLAGS(seg->tcphdr) & TCP_RST) == 0);
		/* Stop sending if the nagle algorithm would prevent it
		 * Don't stop:
		 * - if tcp_write had a memory error before (prevent delayed ACK timeout) or
		 * - if FIN was already enqueued for this PCB (SYN is always alone in a segment -
		 *   either seg->next != NULL or pcb->unacked == NULL;
		 *   RST is no sent using tcp_write/tcp_output.
		 */
		if ((tcp_do_output_nagle(pcb) == 0) &&
				((pcb->flags & (TF_NAGLEMEMERR | TF_FIN)) == 0)) {
			break;
		}
		
		PLOG("snd_wnd %d cwnd %d wnd %u effwnd %u seq %u ack %u",
				pcb->snd_wnd, pcb->cwnd, wnd,
				ntohl(seg->tcphdr->seqno) + seg->len - pcb->lastack,
				ntohl(seg->tcphdr->seqno),
				pcb->lastack);
		
		pcb->unsent = seg->next;

		if (pcb->state != SYN_SENT) {
			TCPH_SET_FLAG(seg->tcphdr, TCP_ACK);
			pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
		}
		tcp_output_segment(seg, pcb);
		snd_nxt = ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg);
		if (TCP_SEQ_LT(pcb->snd_nxt, snd_nxt)) {
			pcb->snd_nxt = snd_nxt;
		}

		/* put segment on unacknowledged list if length > 0 */
		if (TCP_TCPLEN(seg) > 0) {
			seg->next = NULL;
			/* unacked list is empty? */
			if (pcb->unacked == NULL) {
				pcb->unacked = seg;
				useg = seg;
			/* unacked list is not empty? */
			} else {
				/* In the case of fast retransmit, the packet should not go to the tail
				 * of the unacked queue, but rather somewhere before it. We need to check for
				 * this case. -STJ Jul 27, 2004 */
				if (TCP_SEQ_LT(ntohl(seg->tcphdr->seqno), ntohl(useg->tcphdr->seqno))) {
					/* add segment to before tail of unacked list, keeping the list sorted */
					struct tcp_seg2 **cur_seg = &(pcb->unacked);
					while (*cur_seg &&
							TCP_SEQ_LT(ntohl((*cur_seg)->tcphdr->seqno), ntohl(seg->tcphdr->seqno))) {
						cur_seg = &((*cur_seg)->next);
					}
					seg->next = (*cur_seg);
					(*cur_seg) = seg;
				} else {
					/* add segment to tail of unacked list */
					useg->next =seg;
					useg = useg->next;
				}
			}
		/* do not queue empty segments on the unacked list */
		} else {
			tcp_seg_free(seg);
		}
		seg = pcb->unsent;
	}
	if (pcb->unsent == NULL) {
		/* last unsent has been removed, reset unsent_oversize */
		pcb->unsent_oversize = 0;
	}
	pcb->flags &= ~TF_NAGLEMEMERR;

	PLOG("ends.");
	return ERR_OK;
}

/**
 * Create a TCP segment with prefilled header.
 *
 * @param pcb Protocol control block for the TCP connection.
 * @param p pbuf that is used to hold the TCP header.
 * @param flags TCP flags for header.
 * @param seqno TCP sequence number of this packet
 * @param optflags options to include in TCP header
 * @return a new tcp_seg pointing to p, or NULL.
 * The TCP header is filled in except ackno and wnd.
 * p is freed on failure.
 */
struct tcp_seg2 *Exo_stack::tcp_create_segment(struct tcp_pcb *pcb, struct pbuf2 *p, u8_t flags, u32_t seqno, u8_t optflags)
{
	PLOG();

	struct tcp_seg2 *seg;
	u8_t optlen = EXO_TCP_OPT_LENGTH(optflags);

	//if ((seg = new struct tcp_seg) == NULL) {
	if ((seg = (struct tcp_seg2 *)malloc(sizeof(struct tcp_seg2))) == NULL) {
		PLOG("no memory.");
		pbuf2_free(p);
		return NULL;
	}
	
	seg->flags = optflags;
	seg->next = NULL;
	seg->p = p;
	seg->len = p->tot_len - optlen;

	/* build TCP header */
	if (pbuf2_header(p, TCP_HLEN)) {
		PLOG("no room for TCP header in pbuf.");
		tcp_seg_free(seg);
		return NULL;
	}
	seg->tcphdr = (struct tcp_hdr *)seg->p->payload;
	seg->tcphdr->src = htons(pcb->local_port);
	seg->tcphdr->dest = htons(pcb->remote_port);
	seg->tcphdr->seqno = htonl(seqno);
	/* ackno is set in tcp_output */
	TCPH_HDRLEN_FLAGS_SET(seg->tcphdr, (5 + optlen / 4), flags);
	/* wnd and chksum are set in tcp_output */
	seg->tcphdr->urgp = 0;	
	PLOG("ends.");
	return seg;
}

/** Send an ACK without data.
 *
 * @param pcb Protocol control block for the TCP connection to send the ACK
 */
err_t Exo_stack::tcp_send_empty_ack(struct tcp_pcb *pcb)
{
	PLOG("starts.");
	struct pbuf2 *p;
	u8_t optlen = 0;

	p = tcp_output_alloc_header(pcb, optlen, 0, htonl(pcb->snd_nxt));
	if (p == NULL) {
		PLOG("(ACK) could not allocate pbuf");
		return ERR_BUF;
	}
	PLOG("sending ACK for %u", pcb->rcv_nxt);
	/* remove ACK flags from the PCB, as we send an empty ACK now */
	pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);

	/* NB, MSS and window scale options are only sent on SYNs, so ignore them here */
	ip_output(p, &pcb->local_ip, &pcb->remote_ip, pcb->ttl, pcb->tos, IP_PROTO_TCP);
	delete p;

	PLOG("ends.");
	return ERR_OK;
}

/**
 * Called by tcp_output() to actually send a TCP segment over IP.
 *
 * @param seg the tcp_seg to send
 * @param pcb the tcp_pcb for the TCP connection used to send the segment
 */
void Exo_stack::tcp_output_segment(struct tcp_seg2 *seg, struct tcp_pcb *pcb)
{
	PLOG();

	u16_t len;
	u32_t *opts;

	/* The TCP header has already been constructed, but the ackno and
	 wnd field remain. */
	seg->tcphdr->ackno = htonl(pcb->rcv_nxt);

	/* advertise our receive window size in this TCP segment */
	seg->tcphdr->wnd = htons(pcb->rcv_ann_wnd);

	pcb->rcv_ann_right_edge = pcb->rcv_nxt + pcb->rcv_ann_wnd;

	/* Add any requested options. NB MSS option is only set on SYN
	   packets, so ignore it here */
	opts = (u32_t *)(void *)(seg->tcphdr + 1);
	if (seg->flags & TF_SEG_OPTS_MSS) {
		u16_t mss;
		mss = tcp_eff_send_mss(TCP_MSS, &pcb->local_ip);
		*opts = TCP_BUILD_MSS_OPTION(mss);
		opts += 1;
	}

	/* Set retransmission timer running if it is not currently enabled
	   This must be set before checking the route. */
	if (pcb->rtime == -1) {
		pcb->rtime = 0;
	}

	/* If we don't have a local IP address, we get one by
	   calling ip_route(). */
	//TODO: implement this when ICMP testing is completed
	
	if (pcb->rttest == 0) {
		pcb->rttest = _tcp_ticks;
		pcb->rtseq = ntohl(seg->tcphdr->seqno);
	}
	PLOG("tcp_output_segment: %u:%u", 
			htonl(seg->tcphdr->seqno), htonl(seg->tcphdr->seqno) + seg->len);

	len = (u16_t)((u8_t *)seg->tcphdr - (u8_t *)seg->p->payload);

	seg->p->len -= len;
	seg->p->tot_len -= len;

	seg->p->payload = seg->tcphdr;

	seg->tcphdr->chksum = 0;

	//seg->tcphdr->chksum = inet_chksum_pseudo(seg->p, IP_PROTO_TCP,
	//		seg->p->tot_len, &pcb->local_ip, &pcb->remote_ip);

	PLOG("src=%d dest=%d", ntohs(seg->tcphdr->src), ntohs(seg->tcphdr->dest));
	ip_output(seg->p, &pcb->local_ip, &pcb->remote_ip, pcb->ttl,
			pcb->tos, IP_PROTO_TCP);
	
}

/**
 * Calculates the effective send mss that can be used for a specific IP address
 * by using ip_route to determine the netif used to send to the address and
 * calculating the minimum of TCP_MSS and that netif's mtu (if set).
 */
u16_t Exo_stack::tcp_eff_send_mss(u16_t sendmss, ip_addr_t *dest)
{
	PLOG();

	u16_t mss_s;
	s16_t mtu;


	//TODO mtu should be set from underlying NIC
	
	if (mtu != 0) {
		mss_s = mtu - IP_HLEN - TCP_HLEN;
		sendmss = EXO_MIN(sendmss, mss_s);
	}
	return sendmss;
}

/**
 * Called periodically to dispatch TCP timers.
 */
void Exo_stack::tcp_tmr(void)
{
	PLOG();

	/* Call tcp_fasttmr() every 250ms */
	tcp_fasttmr();

	if (++_tcp_timer & 1) {
		/* Call tcp_tmr() every 500 ms, i.e., every other timer
		   tcp_tmr() is called. */
		tcp_slowtmr();
	}
}

/**
 * 	Is called every TCP_FAST_INTERVAL (250 ms) and process data previously
 * 	"refused" by upper layer (application) and sends delayed ACKs.
 *
 * 	Automatically called from tcp_tmr().
 */
void Exo_stack::tcp_fasttmr(void)
{
	PLOG();

	struct tcp_pcb *pcb;

	++_tcp_timer_ctr;

tcp_fasttmr_start:
	pcb = _tcp_active_pcbs;

	while (pcb != NULL) {
		if (pcb->last_timer != _tcp_timer_ctr) {
			struct tcp_pcb *next;
			pcb->last_timer = _tcp_timer_ctr;
			/* send delayed ACKs */
			if (pcb->flags & TF_ACK_DELAY) {
				PLOG("tcp_fasttmr: delayed ACK");
				tcp_ack_now(pcb);
				tcp_output(pcb);
				pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
			}

			next = pcb->next;

			/* If there is data which was previously "refused" by upper layer */
			if (pcb->refused_data != NULL) {
				_tcp_active_pcbs_changed = 0;
				tcp_process_refused_data(pcb);
				if (_tcp_active_pcbs_changed) {
					/* application callback has changed the pcb list: restart the loop */
					goto tcp_fasttmr_start;
				}
			}
			pcb = next;
		} else {
			pcb = pcb->next;
		}
	}
}

/**
 * Called every 500 ms and implements the retransmission timer and the timer that
 * removes PCBs that have been in TIME-WAIT for enough time. It also increments
 * various timers such as the inactivity timer in each PCB.
 *
 * Automatically called from tcp_tmr().
 */
void Exo_stack::tcp_slowtmr(void)
{
	PLOG("starts.");

	struct tcp_pcb *pcb, *prev;
	tcpwnd_size_t eff_wnd;
	u8_t pcb_remove;		/* flag if a PCB should be removed  */
	u8_t pcb_reset;		/* flag if a RST should be sent when removing */
	err_t err;

	err = ERR_OK;

	++_tcp_ticks;
	++_tcp_timer_ctr;

tcp_slowtmr_start:
	/* Steps through all of the active PCBs. */
	prev = NULL;
	pcb = _tcp_active_pcbs;
	if (pcb == NULL) {
		PLOG("no active pcbs");
	}
	while (pcb != NULL) {
		PLOG("processing active pcb");
		assert(pcb->state != CLOSED);
		assert(pcb->state != LISTEN);
		assert(pcb->state != TIME_WAIT);
		if (pcb->last_timer == _tcp_timer_ctr) {
			/* skip this pcb, we have already processed it */
			pcb = pcb->next;
			continue;
		}
		pcb->last_timer = _tcp_timer_ctr;

		pcb_remove = 0;
		pcb_reset = 0;

		if (pcb->state == SYN_SENT && pcb->nrtx == TCP_SYNMAXRTX) {
			++pcb_remove;
			PLOG("max SYN retries reached");
		} else if (pcb->nrtx == TCP_MAXRTX) {
			++pcb_remove;
			PLOG("max DATA retries reached");
		} else {
			if (pcb->persist_backoff > 0) {
				/* If snd_wnd is zero, use persist timer to send 1 byte probes
				 * instead of using the standard retransmission mechanism. */
				pcb->persist_cnt++;
				if (pcb->persist_cnt >= _tcp_persist_backoff[pcb->persist_backoff-1]) {
					pcb->persist_cnt = 0;
					if (pcb->persist_backoff < sizeof(_tcp_persist_backoff)) {
						pcb->persist_backoff++;
					}
					tcp_zero_window_probe(pcb);
				}
			} else {
				/* Increase the retransmission timer if it is running */
				if (pcb->rtime >= 0) {
					++pcb->rtime;
				}

				if (pcb->unacked != NULL && pcb->rtime >= pcb->rto) {
					/* Time for a retransmission. */
					PLOG("rtime %d pcb->rto %d", pcb->rtime, pcb->rto);

					/* Double retransmission time-out unless we are trying to
					 * connect to somebody (i.e., we are in SYN_SENT). */
					if (pcb->state != SYN_SENT) {
						pcb->rto = ((pcb->sa >> 3) + pcb->sv) << _tcp_backoff[pcb->nrtx];
					}

					/* Reset the retransmission timer. */
					pcb->rtime = 0;

					/* Reduce congestion window and ssthresh. */
					eff_wnd = EXO_MIN(pcb->cwnd, pcb->snd_wnd);
					pcb->ssthresh = eff_wnd >> 1;
					if (pcb->ssthresh < (tcpwnd_size_t)(pcb->mss << 1)) {
						pcb->ssthresh = (pcb->mss << 1);
					}
					pcb->cwnd = pcb->mss;
					PLOG("cwnd %u ssthresh %u", pcb->cwnd, pcb->ssthresh);

					/* The following needs to be called AFTER cwnd is set to one
					   mss - STJ */
					tcp_rexmit_rto(pcb);
				}
			}
		}
		/* Check if this PCB has stayed too long in FIN-WAIT-2 */
		if (pcb->state == FIN_WAIT_2) {
			/* If this PCB is in FIN_WAIT_2 because of SHUT_WR don't let it timeout. */
			if (pcb->flags & TF_RXCLOSED) {
				/* PCB was fully closed (either through close() or SHUT_RDWR):
				   normal FIN-WAIT timeout handling. */
				if ((u32_t)(_tcp_ticks - pcb->tmr) >
						TCP_FIN_WAIT_TIMEOUT / TCP_SLOW_INTERVAL) {
					++pcb_remove;
					PLOG("removing pcb stuck in FIN-WAIT-2");
				}
			}
		}
		
		/* Check if KEEPALIVE should be sent */
		if (ip_get_option(pcb, SOF_KEEPALIVE) &&
				((pcb->state == ESTABLISHED)) ||
				((pcb->state == CLOSE_WAIT))) {
			if ((u32_t)(_tcp_ticks - pcb->tmr) >
					(pcb->keep_idle + TCP_KEEP_DUR(pcb)) / TCP_SLOW_INTERVAL)
			{
				PLOG("KEEPALIVE timeout. Aborting connection to");
				++pcb_remove;
				++pcb_reset;
			} else if ((u32_t)(_tcp_ticks - pcb->tmr) >
					(pcb->keep_idle + pcb->keep_cnt_sent * TCP_KEEP_INTVL(pcb))
					/ TCP_SLOW_INTERVAL) {
				tcp_keepalive(pcb);
				pcb->keep_cnt_sent++;
			}
		}

		/* If this PCB has queued out of sequence data, but has been
		   inactive for too long, will drop the data (it will eventually
			 be retransmitted). */
		if (pcb->ooseq != NULL &&
				(u32_t)_tcp_ticks - pcb->tmr >= pcb->rto * TCP_OOSEQ_TIMEOUT) {
			tcp_segs_free(pcb->ooseq);
			pcb->ooseq = NULL;
			PLOG("dropping OOSEQ queued data");
		}
		
		/* Check if this PCB has stayed too long in SYN-RCVD */
		if (pcb->state == SYN_RCVD) {
			if ((u32_t)(_tcp_ticks - pcb->tmr) >
					TCP_SYN_RCVD_TIMEOUT / TCP_SLOW_INTERVAL) {
				++pcb_remove;
				PLOG("removing pcb stuck in SYN-RCVD");
			}
		}
		
		/* Check if this PCB has stayed too long in LAST-ACK */
		if (pcb->state == LAST_ACK) {
			if ((u32_t)(_tcp_ticks - pcb->tmr) > 2 * TCP_MSL 
					/ TCP_SLOW_INTERVAL) {
				++pcb_remove;
				PLOG("removing pcb stuck in LAST-ACK");
			}
		}

		/* If the PCB should be removed, do it */
		if (pcb_remove) {
			struct tcp_pcb *pcb2;
			tcp_err_fn err_fn;
			void *err_arg;
			tcp_pcb_purge(pcb);
			/*Remove PCB from tcp_active_pcbs list. */
			if (prev != NULL) {
				assert(pcb != _tcp_active_pcbs);
				prev->next = pcb->next;
			} else {
				/* This PCB was the first. */
				assert(_tcp_active_pcbs == pcb);
				_tcp_active_pcbs = pcb->next;
			}

			if (pcb_reset) {
				tcp_rst(pcb->snd_nxt, pcb->rcv_nxt, &pcb->local_ip, 
						&pcb->remote_ip, pcb->local_port, pcb->remote_port);
			}

			err_fn = pcb->errf;
			err_arg = pcb->callback_arg;
			pcb2 = pcb;
			delete pcb2;

			_tcp_active_pcbs_changed = 0;
			TCP_EVENT_ERR(err_fn, err_arg, ERR_ABRT);
			if (_tcp_active_pcbs_changed) {
				goto tcp_slowtmr_start;
			}
		} else {
		/* get the 'next' element now and work with 'prev' below (in case of abort) */
			prev = pcb;
			pcb = pcb->next;

			/* We check if we should poll the connection. */
			++prev->polltmr;
			if (prev->polltmr >= prev->pollinterval) {
				prev->polltmr = 0;
				PLOG("polling application");
				_tcp_active_pcbs_changed = 0;
				TCP_EVENT_POLL(prev, err);
				if (_tcp_active_pcbs_changed) {
					goto tcp_slowtmr_start;
				}
				/* if err == ERR_ABRT, 'prev' is already deallocated */
				if (err == ERR_OK) {
					tcp_output(prev);
				}
			}
		}
	}

	/* Steps through all of the TIME-WAIT PCBs. */
	prev = NULL;
	pcb = _tcp_tw_pcbs;
	while (pcb != NULL) {
		assert(pcb->state == TIME_WAIT);
		pcb_remove = 0;

		/* Check if this PCB has stayed long enough in TIME-WAIT */
		if ((u32_t)(_tcp_ticks - pcb->tmr) > 2 * TCP_MSL / TCP_SLOW_INTERVAL) {
			++pcb_remove;
		}

		/* If the PCB should be removed, do it. */
		if (pcb_remove) {
			struct tcp_pcb *pcb2;
			tcp_pcb_purge(pcb);
			/* Remove PCB from tcp_tw_pcbs list. */
			if (prev != NULL) {
				assert(pcb != _tcp_tw_pcbs);
				prev->next = pcb->next;
			} else {
				/* This PCB was the first. */
				assert(_tcp_tw_pcbs == pcb);
				_tcp_tw_pcbs = pcb->next;
			}
			pcb2 = pcb;
			pcb = pcb->next;
			delete pcb2;
		} else {
			prev = pcb;
			pcb = pcb->next;
		}
	}
	PLOG("ends.");
}

/**
 * Used to specify the function that should be called when a fatal error
 * has occured on the connection.
 *
 * @param pcb tcp_pcb to set the err callback
 * @param err callback function to call for this pcb when a fatal error
 * 				has occured on the connection
 */
void Exo_stack::tcp_err(struct tcp_pcb *pcb, tcp_err_fn err)
{
	PLOG("starts.");
	assert(pcb->state != LISTEN);
	pcb->errf = err;
	PLOG("ends.");
}

err_t Exo_stack::server_poll(void *arg, struct tcp_pcb *pcb)
{
	PLOG("starts.");
	static int counter = 1;
	PLOG("Call number %d", counter++);

	PLOG("ends.");
	return ERR_OK;
}

/**
 * Used to specify the function that should be called periodically
 * from TCP. The interval is specified in terms of the TCP coarse
 * timer interval, which is called twice a second.
 *
 */
void Exo_stack::tcp_poll(struct tcp_pcb *pcb, tcp_poll_fn poll, u8_t interval)
{
	assert(pcb->state != LISTEN);
	pcb->poll = poll;
	pcb->pollinterval = interval;
}

/**
 * Purges the PCB and removes it from a PCB list. Any delayed ACKs are sent first.
 *
 * @param pcblist PCB list to purge.
 * @param pcb tcp_pcb to purge. The pcb itself is NOT deallocated!
 */
void Exo_stack::tcp_pcb_remove(struct tcp_pcb **pcblist, struct tcp_pcb *pcb)
{
	TCP_RMV(pcblist, pcb);

	tcp_pcb_purge(pcb);

	/* If there is an outstanding delayed ACKs, send it */
	if (pcb->state != TIME_WAIT &&
			pcb->state != LISTEN &&
			pcb->flags & TF_ACK_DELAY) {
		pcb->flags |= TF_ACK_NOW;
		tcp_output(pcb);
	}

	if (pcb->state != LISTEN) {
		assert(pcb->unsent == NULL);
		assert(pcb->unacked == NULL);
		assert(pcb->ooseq == NULL);
	}

	pcb->state = CLOSED;

	assert(tcp_pcbs_sane());
}

/**
 * Called by tcp_close() to send a segment including FIN flag but not data.
 *
 * @param pcb the tcp_pcb over which to send a segment
 * @return ERR_OK if sent, another err_t otherwise
 */
err_t Exo_stack::tcp_send_fin(struct tcp_pcb *pcb)
{
	PLOG("starts.");

	/* first, try to add the fin to the last unsent segment */
	if (pcb->unsent != NULL) {
		struct tcp_seg2 *last_unsent;
		for (last_unsent = pcb->unsent; last_unsent->next != NULL;
				last_unsent = last_unsent->next);

		if ((TCPH_FLAGS(last_unsent->tcphdr) & (TCP_SYN | TCP_FIN | TCP_RST)) == 0) {
			/* no SYN/FIN/RST flag in the header, we can add the FIN flag */
			TCPH_SET_FLAG(last_unsent->tcphdr, TCP_FIN);
			pcb->flags |= TF_FIN;
			return ERR_OK;
		}
	}
	PLOG("ends.");
	/* no data, no length, flags, copy=1, no optdata */
	return tcp_enqueue_flags(pcb, TCP_FIN);
}

/**
 * Send persist timer zero-window probes to keep a connection active
 * when a window update is lost.
 *
 * Called by tcp_slowtmr()
 *
 * @param pcb the tcp_pcb for which to send a zero-window probe packet
 */
void Exo_stack::tcp_zero_window_probe(struct tcp_pcb *pcb)
{
	struct pbuf2 *p;
	struct tcp_hdr *tcphdr;
	struct tcp_seg2 *seg;
	u16_t len;
	u8_t is_fin;

	PLOG("sending ZERO WINDOW probe to");
	PLOG("tcp_ticks %u pcb->tmr %u pcb->keep_cnt_sent %u",
			_tcp_ticks, pcb->tmr, pcb->keep_cnt_sent);
	
	seg = pcb->unacked;

	if (seg == NULL) {
		seg = pcb->unsent;
	}
	if (seg == NULL) {
		return;
	}

	is_fin = ((TCPH_FLAGS(seg->tcphdr) & TCP_FIN) != 0) && (seg->len == 0);
	/* we want to send one seqno; either FIN or data (no options) */
	len = is_fin ? 0 : 1;

	p = tcp_output_alloc_header(pcb, 0, len, seg->tcphdr->seqno);
	if (p == NULL) {
		PLOG("no memory for pbuf");
		return;
	}
	tcphdr = (struct tcp_hdr *)p->payload;

	if (is_fin) {
		/* FIN segment, no data */
		TCPH_FLAGS_SET(tcphdr, TCP_ACK | TCP_FIN);
	} else {
		/* Data segment, copy in one byte from the head of the unacked queue */
		char *d = ((char *)p->payload + TCP_HLEN);
		/* Depending on whether the segment has laready been sent (unacked) or not
		   (unsent), seg->p->payload points to the IP header or TCP header.
			 Ensure we copy the first TCP data byte: */
		pbuf2_copy_partial(seg->p, d, 1, seg->p->tot_len - seg->len);
	}
	
	ip_output(p, &pcb->local_ip, &pcb->remote_ip, pcb->ttl, 0, IP_PROTO_TCP);

	pbuf2_free(p);

	PLOG("seqno %u ackno %u.", pcb->snd_nxt - 1, pcb->rcv_nxt);
}

/**
 * Requeue all unacked segments for retransmission
 *
 * Called by tcp_slowtmr() for slow retransmission.
 *
 * @param pcb the tcp_pcb for which to re-enqueue all unacked segments
 */
void Exo_stack::tcp_rexmit_rto(struct tcp_pcb *pcb)
{
	struct tcp_seg2 *seg;

	if (pcb->unacked == NULL) {
		return;
	}

	/* Move all unacked segments to the head of the unsent queue */
	for (seg = pcb->unacked; seg->next != NULL; seg = seg->next);
	/* concatenate unsent queue after unacked queue */
	seg->next = pcb->unsent;
	/*unsent queue is the concatenated queue (of unacked, unsent) */
	pcb->unsent = pcb->unacked;
	/* unacked queue is now empty */
	pcb->unacked = NULL;

	/* increment number of retransmissions */
	++pcb->nrtx;
	
	/* Don't take any RTT measurements after retransmitting. */
	pcb->rttest = 0;

	/* Do the actual retransmission */
	tcp_output(pcb);
}

/**
 * Send keepalive packets to keep a connection active although
 * no data is sent over it.
 *
 * Called by tcp_slowtmr()
 *
 * @param pcb the tcp_pcb for which to send a keepalive packet
 */
void Exo_stack::tcp_keepalive(struct tcp_pcb *pcb)
{
	//TODO
	panic("NOT IMPLEMENTED!");
}

/**
 * Check state consistency of the tcp_pcb lists.
 */
s16_t Exo_stack::tcp_pcbs_sane(void)
{
	struct tcp_pcb *pcb;
	for (pcb = _tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
		assert(pcb->state != CLOSED);
		assert(pcb->state != LISTEN);
		assert(pcb->state != TIME_WAIT);
	}
	for (pcb = _tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
		assert(pcb->state == TIME_WAIT);
	}
	return 1;
}

/**
 * Kills the oldest connection that is in TIME_WAIT state.
 * Called from tcp_alloc() if no more connections are available.
 */
void Exo_stack::tcp_kill_timewait(void)
{
	struct tcp_pcb *pcb, *inactive;
	u32_t inactivity;

	inactivity = 0;
	inactive = NULL;

	for (pcb = _tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
		if ((u32_t)(_tcp_ticks - pcb->tmr) >= inactivity) {
			inactivity = _tcp_ticks - pcb->tmr;
			inactive = pcb;
			}
	}
	if (inactive != NULL) {
		PLOG("killing oldest TIME-WAIT PCB %p (%d)", (void *)inactive,
				inactivity);
		tcp_abort(inactive);
	}
}

/**
 * Aborts the connection by sending a RST (reset) segment to the remote
 * host. The pcb is deallocated. This function never fails.
 *
 * ATTENTION: When calling this from one of the TCP callbacks, make
 * sure you always return ERR_ABRT (and never return ERR_ABRT otherwise
 * or you will risk accessing deallocated memory or memory leaks!
 *
 * @param pcb the tcpc pcb to abort
 */
void Exo_stack::tcp_abort(struct tcp_pcb *pcb)
{
	tcp_abandon(pcb, 1);
}

/**
 * Abandons a connection and optionally sends a RST to the remote
 * host. Deletes the local protocol control block. This is done when
 * a connection is killed because of shortage of memory.
 *
 * @param pcb the tcp_pcb to abort
 * @param reset boolean to indicate whether a reset should be sent
 */
void Exo_stack::tcp_abandon(struct tcp_pcb *pcb, int reset)
{
	u32_t seqno, ackno;
	tcp_err_fn errf;
	void *errf_arg;

	/* pcb->state LISTEN not allowed here */
	assert(pcb->state != LISTEN);
	/* Figure out on which TCP PCB list we are, and remove us. If we
	 * are in an active state, call the receive function associated with
	 * the PCB with a NULL argument, and send an RST to the remote end. */
	if (pcb->state == TIME_WAIT) {
		tcp_pcb_remove(&_tcp_tw_pcbs, pcb);
		delete pcb;
	} else {
		int send_rst = reset && (pcb->state != CLOSED);
		seqno = pcb->snd_nxt;
		ackno = pcb->rcv_nxt;
		errf = pcb->errf;
		errf_arg = pcb->callback_arg;
		TCP_PCB_REMOVE_ACTIVE(pcb);
		if (pcb->unacked != NULL) {
			tcp_segs_free(pcb->unacked);
		}
		if (pcb->unsent != NULL) {
			tcp_segs_free(pcb->unsent);
		}
		if (pcb->ooseq != NULL) {
			tcp_segs_free(pcb->ooseq);
		}
		if (send_rst) {
			PLOG("sending RST");
			tcp_rst(seqno, ackno, &pcb->local_ip, &pcb->remote_ip, pcb->local_port, pcb->remote_port);
		}
		delete pcb;
		TCP_EVENT_ERR(errf, errf_arg, ERR_ABRT);
	}
}

/**
 * Kills the oldest active connection that has the same or lower priority than
 * 'prio'.
 *
 * @param prio minimum priority
 */
void Exo_stack::tcp_kill_prio(u8_t prio)
{
	struct tcp_pcb *pcb, *inactive;
	u32_t inactivity;
	u8_t mprio;

	mprio = TCP_PRIO_MAX;

	/* We kill the oldest active connection that has lower priority than prio. */
	inactivity = 0;
	inactive = NULL;
	for (pcb = _tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
		if (pcb->prio <= prio &&
				pcb->prio <= mprio &&
				(u32_t)(_tcp_ticks - pcb->tmr) >= inactivity) {
			inactivity = _tcp_ticks - pcb->tmr;
			inactive = pcb;
			mprio = pcb->prio;
		}
	}
	if (inactive != NULL) {
		PLOG("killing oldest PCB %p (%d)", (void *)inactive, inactivity);
		tcp_abort(inactive);
	}
}

/**
 * Allocates a pbuf of the given type (possibly a chain for PBUF_POOL type).
 *
 * The actual memory allocated for the pbuf is determined by the
 * layer at which the pbuf is allocated and the requested size
 * (from the size parameter).
 *
 * @param layer flag to define header size
 * @param length size of the pbuf's payload
 * @param type this parameter decides how and where the pbuf
 * should be allocated as follows:
 *
 * - PBUF_RAM: buffer memory for pbuf is allocated as one large
 *             chunk. This includes protocol headers as well.
 * - PBUF_ROM: no buffer memory is allocated for the pbuf, even for
 *             protocol headers. Additional headers must be prepended
 *             by allocating another pbuf and chain in to the front of
 *             the ROM pbuf. It is assumed that the memory used is really
 *             similar to ROM in that it is immutable and will not be
 *             changed. Memory which is dynamic should generally not
 *             be attached to PBUF_ROM pbufs. Use PBUF_REF instead.
 * - PBUF_REF: no buffer memory is allocated for the pbuf, even for
 *             protocol headers. It is assumed that the pbuf is only
 *             being used in a single thread. If the pbuf gets queued,
 *             then pbuf_take should be called to copy the buffer.
 * - PBUF_POOL: the pbuf is allocated as a pbuf chain, with pbufs from
 *              the pbuf pool that is allocated during pbuf_init().
 *
 * @return the allocated pbuf. If multiple pbufs where allocated, this
 * is the first pbuf of a pbuf chain.
 */
pbuf2 *Exo_stack::pbuf2_alloc(pbuf_layer layer, u16_t length, pbuf_type type)
{
	struct pbuf2 *p, *q, *r;
	u16_t offset;
	s32_t rem_len;
	PLOG("length=%u", length);

	switch (layer) {
		case PBUF_TRANSPORT:
			offset = PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN;
			break;
		case PBUF_IP:
			offset = PBUF_LINK_HLEN + PBUF_IP_HLEN;
			break;
		case PBUF_LINK:
			panic("NOT IMPLEMENTED! yet: line %d", __LINE__);
			break;
		case PBUF_RAW:
			offset = 0;
			break;
		default:
			PLOG("bad pbuf layer");
			return NULL;
	}

	switch (type) {
		case PBUF_POOL:
			panic("PBUF_POOL: NOT IMPLEMENTED!");
		case PBUF_RAM:
			p = (struct pbuf2 *)malloc(SIZEOF_STRUCT_PBUF2 + offset + length);
			if (p == NULL) {
				return NULL;
			}
			p->payload = (void *)(p + SIZEOF_STRUCT_PBUF2 + offset);
			p->len = p->tot_len = length;
			p->next = NULL;
			p->type = type;

			break;
		case PBUF_ROM:
			panic("PBUF_ROM: NOT IMPLEMENTED!");
		case PBUF_REF:
			panic("PBUF_REF: NOT IMPLEMENTED!");
		default:
			PLOG("erroneous type");
			return NULL;
	}
	/* set reference count */
	p->ref = 1;
	/* set flags */
	p->flags = 0;

	return p;
}

/**
 * Dereference a pbuf chain or queue and deallocate any no-longer-used
 * pbufs at the head of this chain or queue.
 *
 * Decrements the pbuf reference count. If it reaches zero, the pbuf is
 * deallocated.
 *
 * For a pbuf chain, this is repeated for each pbuf in the chain,
 * up to the first pbuf which has a non-zero reference count after
 * decrementing. So, when all reference counts are one, the whole
 * chain is free'd.
 *
 * @param p The pbuf (chain) to be dereferenced.
 *
 * @return the number of pbufs that were de-allocated
 * from the head of the chain.
 *
 * @note MUST NOT be called on a packet queue (Not verified to work yet).
 * @note the reference counter of a pbuf equals the number of pointers
 * that refer to the pbuf (or into the pbuf).
 *
 * @internal examples:
 *
 * Assuming existing chains a->b->c with the following reference
 * counts, calling pbuf_free(a) results in:
 * 
 * 1->2->3 becomes ...1->3
 * 3->3->3 becomes 2->3->3
 * 1->1->2 becomes ......1
 * 2->1->1 becomes 1->1->1
 * 1->1->1 becomes .......
 *
 */
u8_t Exo_stack::pbuf2_free(struct pbuf2 *p)
{
	u16_t type;
	struct pbuf2 *q;
	u8_t count;

	if (p == NULL) {
		assert(p != NULL);
		/* if assertions are disabled, proceed with debug output */
		PLOG("pbuf_free(p == NULL) was called.");
		return 0;
	}
	PLOG("%p", (void*)p);
	
	assert(p->type == PBUF_RAM || p->type == PBUF_ROM ||
					p->type == PBUF_REF || p->type == PBUF_POOL);
	
	count = 0;
	/* de-allocate all consecutive pbufs from the head of the chain that
	 * obtain a zero reference count after decrementing */
	while (p != NULL) {
		u16_t ref;
		sys_prot_t old_level;
		/* Since decrementing ref cannot be guaranteed to be a single machine operation
		 * we must protect it. We put the new ref into a local variable to prevent
		 * further protection. */
		old_level = sys_arch_protect();
		/* all pbufs in a chain are referenced at least once */
		assert(p->ref > 0);
		/* decrease reference count (number of pointers to pbuf) */
		ref = --(p->ref);
		sys_arch_unprotect(old_level);
		/* this pbuf is no longer referenced to? */
		if (ref == 0) {
			/* remember next pbuf in chain for next iteration */
			q = p->next;
			PLOG("deallocating %p", (void *)p);
			type = p->type;
			/* is this a custom pbuf? */
			if ((p->flags & PBUF_FLAG_IS_CUSTOM) != 0) {
				panic("[%d] NOT IMPLMENTED!", __LINE__);
			} else {
				/* is this a pbuf from the pool? */
				if (type == PBUF_POOL) {
					panic("[%d] NOT IMPLEMENTED!", __LINE__);
				/* is this a ROM or RAM referencing pbuf? */
				} else if (type == PBUF_ROM || type == PBUF_REF) {
					panic("[%d] NOT IMPLEMENTED!", __LINE__);
				/* type == PBUF_RAM */
				} else {
					free(p);
				}
			}
			count++;
			/* proceed to next pbuf */
			p = q;
		/* p->ref > 0, this pbuf is still referenced to */
		/* (and so the remaining pbufs in chain as well) */
		} else {
			PLOG("%p has ref %u ending here.", (void *)p, ref);
			p = NULL;
		}
	}
	return count;
}

/**
 * Adjusts the payload pointer to hide or reveal headers in the payload.
 *
 * Adjusts the ->payload pointer so that space for a header
 * (dis)appears in the pbuf payload.
 *
 * The ->payload, ->tot_len and ->len fields are adjusted.
 *
 * @param p pbuf to change the header size.
 * @param header_size_increment Number of bytes to increment header size which
 * increases the size of the pbuf. New space is on the front.
 * (Using a negative value decreases the header size.)
 * If hdr_size_inc is 0, this function does nothing and returns succesful.
 *
 * PBUF_ROM and PBUF_REF type buffers cannot have their sizes increased, so
 * the call will fail. A check is made that the increase in header size does
 * not move the payload pointer in front of the start of the buffer.
 * @return non-zero on failure, zero on success.
 *
 */
u8_t Exo_stack::pbuf2_header(struct pbuf2 *p, s16_t header_size_increment)
{
	u16_t type;
	void *payload;
	u16_t increment_magnitude;

	assert(p != NULL);
	if ((header_size_increment == 0) || (p == NULL)) {
		return 0;
	}

	if (header_size_increment < 0) {
		increment_magnitude = -header_size_increment;
		if (increment_magnitude > p->len) {
			PLOG("[%d] increment_magnitude (%u) > p->len (%d)", __LINE__,
				increment_magnitude, p->len);
			return 1;
		}
	} else {
		increment_magnitude = header_size_increment;
	}

	type = p->type;
	payload = p->payload;

	if (type == PBUF_RAM || type == PBUF_POOL) {
		p->payload = (u8_t *)p->payload - header_size_increment;
		if ((u8_t *)p->payload < (u8_t *)p + SIZEOF_STRUCT_PBUF2) {
			PLOG("failed as %p < %p (not enough space for new header size)",
					(void *)p->payload, (void *)(p + 1));
			p->payload = payload;
			return 1;
		}
	} else if (type == PBUF_REF || type == PBUF_ROM) {
		//TODO
		panic("NOT IMPLEMENTED! yet: line %d", __LINE__);
	} else {
		assert("bad pbuf type");
		return 1;
	}
	p->len += header_size_increment;
	p->tot_len += header_size_increment;

	PLOG("old %p new %p (%d)", (void *)payload, (void *)p->payload,
			header_size_increment);

	PLOG("ends.");
	return 0;
}

/**
 * Count number of pbufs in a chain
 *
 * @param p first pbuf of chain
 * @return the number of pbufs in a chain
 */
u8_t Exo_stack::pbuf2_clen(struct pbuf2 *p)
{
	u8_t len;

	len = 0;
	while (p != NULL) {
		++len;
		p = p->next;
	}
	return len;
}

/**
 * Sends an IP packet on a network interface. This function constructs
 * the IP header and calculates the IP header checksum. If the source
 * IP address is NULL, the IP address of the outgoing network
 * interface is filled in as source address.
 * If the destination IP address is IP_HDRINCL, p is assumed to already
 * include an IP header and p->payload points to it instead of the data.
 *
 * @param p the packet to send (p->payload points to the data, e.g. next
            protocol header; if dest == IP_HDRINCL, p already includes an IP
            header and p->payload points to that IP header)
 * @param src the source IP address to send from (if src == IP_ADDR_ANY, the
 *         IP  address of the netif used to send is used as source address)
 * @param dest the destination IP address to send the packet to
 * @param ttl the TTL value to be set in the IP header
 * @param tos the TOS value to be set in the IP header
 * @param proto the PROTOCOL to be set in the IP header
 * @return ERR_OK if the packet was sent OK
 *         ERR_BUF if p doesn't have enough space for IP/LINK headers
 *         returns errors returned by netif->output
 *
 * @note ip_id: RFC791 "some host may be able to simply use
 *  unique identifiers independent of destination"
 */
err_t Exo_stack::ip_output(struct pbuf2 *p, ip_addr_t *src, ip_addr_t *dest, u8_t ttl, u8_t tos, u8_t proto)
{
	PLOG();
	struct ip_hdr *iphdr;
	ip_addr_t dest_addr;

	assert(p->ref == 1);

	/* should the IP header be generated or is it already included in p? */
	if (dest != NULL) {
		PLOG("dest != NULL");
		u16_t ip_hlen = IP_HLEN;

		// testing
		tcp_debug_print((tcp_hdr*)(p->payload));

		/* generate IP header */
		if (pbuf2_header(p, IP_HLEN)) {
			PLOG("not enough room for IP header in pbuf");
			return ERR_BUF;
		}

		iphdr = (struct ip_hdr *)p->payload;
		assert(p->len >= sizeof(struct ip_hdr));

		IPH_TTL_SET(iphdr, ttl);
		IPH_PROTO_SET(iphdr, proto);

		/* dest cannot be NULL here */
		ip_addr_copy(iphdr->dest, *dest);
		// test
		PLOG("| %u | %u | %u | %u | (dest)",
			ip4_addr1_16(&iphdr->dest),
			ip4_addr2_16(&iphdr->dest),
			ip4_addr3_16(&iphdr->dest),
			ip4_addr4_16(&iphdr->dest));

		//IPH_VHL_SET(iphdr, 4, ip_hlen / 4);
		//IPH_TOS_SET(iphdr, tos);
		IPH_VHLTOS_SET(iphdr, 4, ip_hlen / 4, tos); 

		IPH_LEN_SET(iphdr, htons(p->tot_len));

		IPH_OFFSET_SET(iphdr, 0);
		IPH_ID_SET(iphdr, htons(_ip_id));
		++_ip_id;
		
		//if (ip_addr_isany(src) ...
		{
			ip_addr_copy(iphdr->src, *src);
			// test
			PLOG("| %u | %u | %u | %u | (src)",
				ip4_addr1_16(&iphdr->src),
				ip4_addr2_16(&iphdr->src),
				ip4_addr3_16(&iphdr->src),
				ip4_addr4_16(&iphdr->src));
			
		}	

		//TODO
		//currently, check sum ignored
		IPH_CHKSUM_SET(iphdr, 0);
	} else {
		PLOG("dest == NULL");
		/* IP header already included in p */
		iphdr = (struct ip_hdr *)p->payload;
		ip_addr_copy(dest_addr, iphdr->dest);
		dest = &dest_addr;
	}
	PLOG("ends.");

	ip_debug_print(p);
	return etharp_output(p, dest);
}

/*
 * Print an IP header by using LWIP_DEBUGF
 * @param p an IP packet, p->payload pointing to the IP header
 */
void Exo_stack::ip_debug_print(struct pbuf2 *p)
{
	struct ip_hdr *iphdr = (struct ip_hdr *)p->payload;

	PLOG("IP header:");
	PLOG("+-------------------------------+");
	PLOG("| %d | %d | %u | %u | (v, hl, tos, len)",
		IPH_V(iphdr),
		IPH_HL(iphdr),
		IPH_TOS(iphdr),
		ntohs(IPH_LEN(iphdr)));
	PLOG("+-------------------------------+");
	PLOG("| %u | %u %u %u | %u", 
		ntohs(IPH_ID(iphdr)),
		ntohs(IPH_OFFSET(iphdr)) >> 15 & 1,
		ntohs(IPH_OFFSET(iphdr)) >> 14 & 1,
		ntohs(IPH_OFFSET(iphdr)) >> 13 & 1,
		ntohs(IPH_OFFSET(iphdr)) & IP_OFFMASK);
	PLOG("+-------------------------------+");
	PLOG("| %u | %u | %d | (ttl, proto, chksum)",
		IPH_TTL(iphdr),
		IPH_PROTO(iphdr),
		ntohs(IPH_CHKSUM(iphdr)));
	PLOG("+-------------------------------+");
	PLOG("| %u | %u | %u | %u | (src)",
		ip4_addr1_16(&iphdr->src),
		ip4_addr2_16(&iphdr->src),
		ip4_addr3_16(&iphdr->src),
		ip4_addr4_16(&iphdr->src));
	PLOG("+-------------------------------+");
	PLOG("| %u | %u | %u | %u | (dest)",
		ip4_addr1_16(&iphdr->dest),
		ip4_addr2_16(&iphdr->dest),
		ip4_addr3_16(&iphdr->dest),
		ip4_addr4_16(&iphdr->dest));
	PLOG("+-------------------------------+");
}

void Exo_stack::tcp_debug_print(struct tcp_hdr *tcphdr)
{
	PLOG("+-------------------------------+");
	PLOG("TCP header:");
	PLOG("+-------------------------------+");
	PLOG("| %u | %u | (src port, dest port)",
			ntohs(tcphdr->src), ntohs(tcphdr->dest));
	PLOG("+-------------------------------+");
	PLOG("| %u | (seq no)", ntohl(tcphdr->seqno));
	PLOG("+-------------------------------+");
	PLOG("| %u | (ack no)", ntohl(tcphdr->ackno));
	PLOG("+-------------------------------+");
	PLOG("| %u | | %u %u %u %u %u %u | %u | (hdrlen, flags, win)",
			TCPH_HDRLEN(tcphdr),
			TCPH_FLAGS(tcphdr) >> 5 & 1,
			TCPH_FLAGS(tcphdr) >> 4 & 1,
			TCPH_FLAGS(tcphdr) >> 3 & 1,
			TCPH_FLAGS(tcphdr) >> 2 & 1,
			TCPH_FLAGS(tcphdr) >> 1 & 1,
			TCPH_FLAGS(tcphdr) & 1,
			ntohs(tcphdr->wnd));
	tcp_debug_print_flags(TCPH_FLAGS(tcphdr));

}

/**
 * Resolve and fill-in Ethernet address header for outgoing IP packet.
 *
 * For IP multicast and broadcast, corresponding Ethernet addresses
 * are selected and the packet is transmitted on the link.
 *
 * For unicast addresses, the packet is submitted to etharp_query(). In
 * case the IP address is outside the local network, the IP address of
 * the gateway is used.
 *
 * @param q The pbuf(s) containing the IP packet to be sent.
 * @param ipaddr The IP address of the packet destination.
 *
 */
err_t Exo_stack::etharp_output(struct pbuf2 *q, ip_addr_t *ipaddr)
{
	PLOG();
	//TODO: remove hard-coded addresses
	struct eth_addr src = { 0xa0, 0x36, 0x9f, 0x25, 0x65, 0xd0 };
	struct eth_addr dst = { 0xa0, 0x36, 0x9f, 0x1a, 0x56, 0xa0 };
	//struct eth_addr dst = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	ip_addr_t *dst_addr = ipaddr;

	assert(q != NULL);
	assert(ipaddr != NULL);

	/* make room for Ethernet header - should not fail */
	if (pbuf2_header(q, sizeof(struct eth_hdr)) != 0) {
		PLOG("could not allocate room for header");
		return ERR_BUF;
	}

	/* Determine on destination hardware address. Broadcast and multicasts
	 * are special, other IP addresses are looked up in the ARP table. */
	//TODO
	return etharp_send_ip(q, &src, &dst);

	PLOG("ends.");
}

/**
 * Send an IP packet on the network using netif->linkoutput
 * The ethernet header is filled in before sending.
 *
 * @params p the packet to send, p->payload pointing to the (uninitialized) ethernet header
 * @params src the source MAC address to be copied into the ethernet header
 * @params dst the destination MAC address to be copied into the ethernet header
 * @return ERR_OK if the packet was sent, any other err_t on failure
 */
err_t Exo_stack::etharp_send_ip(struct pbuf2 *p, struct eth_addr *src, struct eth_addr *dst)
{
	PLOG();

	struct eth_hdr *ethhdr = (struct eth_hdr *)p->payload;
	ethhdr->type = htons(ETHTYPE_IP); //PP_HTONS(ETHTYPE_IP);
	//ETHADDR32_COPY(&ethhdr->dest, dst);
	//ETHADDR32_COPY(&ethhdr->src, src);
	ETHADDR16_COPY(&ethhdr->dest, dst);
	ETHADDR16_COPY(&ethhdr->src, src);
	

	return linkoutput(p);

	PLOG("ends.");
}

err_t Exo_stack::linkoutput(struct pbuf2 *p)
{
	PLOG();

	addr_t tmp_v, tmp_p;
	uint32_t length = p->tot_len;
	void* temp1;
	if (_imem->alloc((addr_t *)&temp1, PACKET_ALLOCATOR) != Exokernel::S_OK) {
		panic("PACKET_ALLOCATOR failed!");
	}
	assert(temp1);

	tmp_v = (addr_t)temp1;
	tmp_p = (addr_t)_imem->get_phys_addr(temp1, PACKET_ALLOCATOR);
	assert(tmp_p);
	__builtin_memcpy((void *)tmp_v, (void *)p->payload, length);

	void* temp2;
	if (_imem->alloc((addr_t *)&temp2, MBUF_ALLOCATOR) != Exokernel::S_OK) {
		panic("MBUF_ALLOCATOR failed!");
	}
	assert(temp2);
	__builtin_memset(temp2, 0, sizeof(struct exo_mbuf));

	addr_t pkt_p;
	pkt_p = (addr_t)_imem->get_phys_addr((void *)tmp_v, PACKET_ALLOCATOR);
	assert(pkt_p);

	struct exo_mbuf *mbuf = (struct exo_mbuf *)temp2;
	mbuf->phys_addr = pkt_p;
	mbuf->virt_addr = (addr_t)tmp_v;
	mbuf->len = length;
	mbuf->ref_cnt = 0;
	mbuf->id = 0;
	mbuf->nb_segment = 1;
	mbuf->flag = 0;

	size_t sent_num = 1;
	if (ethernet_output_simple(&mbuf, sent_num, 0) != Exokernel::S_OK) {
		PLOG("TX ring buffer is full!");
	}
	_imem->free(temp2, MBUF_ALLOCATOR);
	_imem->free(temp1, PACKET_ALLOCATOR);

	PLOG("ends.");
}

/**
 * Determine if an address is a broadcast address on a network interface
 *
 * @param addr address to be checked
 * @return returns non-zero if the address is a broadcast address
 */
u8_t Exo_stack::ip4_addr_isbroadcast(u32_t addr)
{
	ip_addr_t ipaddr;
	ip4_addr_set_u32(&ipaddr, addr);

	/* all ones (broadcast) or all zeros (old skool broadcast) */
	//TODO: checking broadcast support on the network interface
	if ((~addr == IPADDR_ANY) ||
			(addr == IPADDR_ANY)) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * Called by tcp_input() when a segment arrives for a listening
 * connection (from tcp_input()).
 *
 * @param pcb the tcp_pcb_listen for which a segment arrived
 * @return ERR_OK if the segment was processed
 * 				 another err_t on error
 *
 * @note the return value is not (yet?) used in tcp_input()
 * @note the segment which arrived is saved in global variables, therefore only the pcb
 *       involved is passed as a parameter to this function
 */
err_t Exo_stack::tcp_listen_input(struct tcp_pcb_listen *pcb)
{
	PLOG();

	struct tcp_pcb *npcb;
	err_t rc;

	if (_flags & TCP_RST) {
		/* An incoming RST should be ignored. Return. */
		return ERR_OK;
	}

	/* In the LISTEN state, we check for incoming SYN segments,
	   creates a new PCB, and responds with a SYN|ACK. */
	if (_flags & TCP_ACK) {
		/* For incoming segments with the ACK flag set, respond with a 
		 	 RST. */
		PLOG("ACK in LISTEN, sending reset");
		tcp_rst(_ackno, _seqno + _tcplen, ip_current_dest_addr(),
				ip_current_src_addr(), _tcphdr->dest, _tcphdr->src);
	} else if (_flags & TCP_SYN) {
		PLOG("TCP connection request %u -> %u.", _tcphdr->src, _tcphdr->dest);
		npcb = tcp_alloc(pcb->prio);
		/* If a new PCB could not be created (probably due to lack of memory),
		   we don't do anything, but reply on the sender will retransmit the
			 SYN at a time when we have more memory available. */
		if (npcb == NULL) {
			PLOG("could not allocate PCB");
			return ERR_MEM;
		}
		ip_addr_copy(npcb->local_ip, *ip_current_dest_addr());
		PLOG("| %u | %u | %u | %u |", ip4_addr1_16(&npcb->local_ip), ip4_addr2_16(&npcb->local_ip), ip4_addr3_16(&npcb->local_ip), ip4_addr4_16(&npcb->local_ip));
		ip_addr_copy(npcb->remote_ip, *ip_current_src_addr());
		PLOG("| %u | %u | %u | %u |", ip4_addr1_16(&npcb->remote_ip), ip4_addr2_16(&npcb->remote_ip), ip4_addr3_16(&npcb->remote_ip), ip4_addr4_16(&npcb->remote_ip));
		npcb->local_port = pcb->local_port;
		npcb->remote_port = _tcphdr->src;
		npcb->state = SYN_RCVD;
		npcb->rcv_nxt = _seqno + 1;
		npcb->rcv_ann_right_edge = npcb->rcv_nxt;
		npcb->snd_wl1 = _seqno - 1;	/* initialise to seqno-1 to force window update */
		npcb->callback_arg = pcb->callback_arg;
		npcb->accept = pcb->accept;
		/* inherit socket options */
		npcb->so_options = pcb->so_options & SOF_INHERITED;
		/* Register the new PCB so that we can begin receiving segments
		   for it. */
		TCP_REG_ACTIVE(npcb);

		/* Parse any options in the SYN. */
		tcp_parseopt(npcb);
		npcb->snd_wnd = SND_WND_SCALE(npcb, _tcphdr->wnd);
		npcb->snd_wnd_max = npcb->snd_wnd;
		npcb->mss = tcp_eff_send_mss(npcb->mss, &npcb->local_ip);

		/* Send a SYN/ACK together with the MSS option. */
		rc = tcp_enqueue_flags(npcb, TCP_SYN | TCP_ACK);
		if (rc != ERR_OK) {
			tcp_abandon(npcb, 0);
			return rc;
		}
		return tcp_output(npcb);
	}
	PLOG("ends.");
	return ERR_OK;
}

/**
 * Implements the TCP state machine. Called by tcp_input. In some
 * states tcp_receive() is called to receive data. The tcp_seg
 * argument will be freed by the caller (tcp_input()) unless the
 * recv_data pointer in the pcb is set.
 *
 * @param pcb the tcp_pcb for which a segment arrived
 *
 * @note the segment which arrived is saved in global variables, therefore only the pcb
 *       involved is passed as a parameter to this function
 */
err_t Exo_stack::tcp_process(struct tcp_pcb *pcb)
{
	PLOG("starts.");
	
	struct tcp_seg2 *rseg;
	u8_t acceptable = 0;
	err_t err;

	err = ERR_OK;

	/* Process incoming RST segments. */
	if (_flags & TCP_RST) {
		/* First, determine if the reset is acceptable. */
		if (pcb->state == SYN_SENT) {
			if (_ackno == pcb->snd_nxt) {
				acceptable = 1;
			}
		} else {
			if (TCP_SEQ_BETWEEN(_seqno, pcb->rcv_nxt, pcb->rcv_nxt+pcb->rcv_wnd)) {
				acceptable = 1;
			}
		}
		if (acceptable) {
			PLOG("Connection RESET");
			assert(pcb->state != CLOSED);
			_recv_flags |= TF_RESET;
			pcb->flags &= ~TF_ACK_DELAY;
			return ERR_RST;
		} else {
			PLOG("unacceptable reset seqno %u rcv_nxt %u", _seqno, pcb->rcv_nxt);
			return ERR_OK;
		}
	}

	if ((_flags & TCP_SYN) && (pcb->state != SYN_SENT && pcb->state != SYN_RCVD))
	{
		/* Cope with new connection attempt after remote end crashed */
		tcp_ack_now(pcb);
		return ERR_OK;
	}

	if ((pcb->flags & TF_RXCLOSED) == 0) {
		/* Update the PCB (in)activity timer unless rx is closed (see tcp_shutdown) */
		pcb->tmr = _tcp_ticks;
	}
	pcb->keep_cnt_sent = 0;

	tcp_parseopt(pcb);

	/* Do different things depending on the TCP state. */
	switch (pcb->state) {
	case SYN_SENT:
			PLOG("SYN-SENT: ackno %u pcb->snd_nxt %u unacked %u",
				_ackno, pcb->snd_nxt, ntohl(pcb->unacked->tcphdr->seqno));
			/* received SYN ACK with expected sequence number? */
			if ((_flags & TCP_ACK) && (_flags & TCP_SYN)
					&& _ackno == ntohl(pcb->unacked->tcphdr->seqno) + 1) {
				pcb->snd_buf++;
				pcb->rcv_nxt = _seqno + 1;
				pcb->rcv_ann_right_edge = pcb->rcv_nxt;
				pcb->lastack = _ackno;
				pcb->snd_wnd = _tcphdr->wnd;
				pcb->snd_wnd_max = _tcphdr->wnd;
				pcb->snd_wl1 = _seqno - 1; /* initialise to seqno - 1 to force window update */
				pcb->state = ESTABLISHED;
				pcb->mss = tcp_eff_send_mss(pcb->mss, &pcb->local_ip);

				/* Set ssthresh again after changing pcb->mss (already set in tcp_connect
				 * but for the default value of pcb->mss) */
				pcb->ssthresh = pcb->mss * 10;

				pcb->cwnd = ((pcb->cwnd == 1)? (pcb->mss * 2) : pcb->mss);
				assert(pcb->snd_queuelen > 0);
				--pcb->snd_queuelen;
				PLOG("SYN-SENT -- queuelen %u", (u16_t)pcb->snd_queuelen);
				rseg = pcb->unacked;
				pcb->unacked = rseg->next;
				tcp_seg_free(rseg);

				/* If there's nothing left to acknowledge, stop the retransmit
				   timer, otherwise reset it to start again */
				if (pcb->unacked == NULL)
					pcb->rtime = -1;
				else {
					pcb->rtime = 0;
					pcb->nrtx = 0;
				}

				/* Call the user specified function to call when successfully
				 * connected. */
				TCP_EVENT_CONNECTED(pcb, ERR_OK, err);
				if (err == ERR_ABRT) {
					return ERR_ABRT;
				}
				tcp_ack_now(pcb);
			}
			/* received ACK? possibly a half-open connection */
			else if (_flags & TCP_ACK) {
				/* send a RST to bring the other side in a non-synchronized state. */
				tcp_rst(_ackno, _seqno + _tcplen, ip_current_dest_addr(),
						ip_current_src_addr(), _tcphdr->dest, _tcphdr->src);
			}
			break;
		case SYN_RCVD:
			if (_flags & TCP_ACK) {
				/* expected ACK number? */
				if (TCP_SEQ_BETWEEN(_ackno, pcb->lastack+1, pcb->snd_nxt)) {
					u16_t old_cwnd;
					pcb->state = ESTABLISHED;
					PLOG("TCP connecton established %u -> %u",
							_inseg.tcphdr->src, _inseg.tcphdr->dest);
					assert(pcb->accept != NULL);

					/* Call the accept function. */
					TCP_EVENT_ACCEPT(pcb, ERR_OK, err);
					if (err != ERR_OK) {
						/* IF the accept function returns with an error, we abort
						 * the connection. */
						/* already aborted? */
						if (err != ERR_ABRT) {
							tcp_abort(pcb);
						}
						return ERR_ABRT;
					}
					old_cwnd = pcb->cwnd;
					/* If there was any data contained within this ACK.
					 * we'd better pass it on to the application as wekk. */
					tcp_receive(pcb);

					/* Prevent ACK for SYN to generate a sent event */
					if (pcb->acked != 0) {
						pcb->acked--;
					}

					pcb->cwnd = ((old_cwnd == 1)? (pcb->mss * 2) : pcb->mss);

					if (_recv_flags & TF_GOT_FIN) {
						tcp_ack_now(pcb);
						pcb->state = CLOSE_WAIT;
					}
				} else {
					/* incorrect ACK number, send RST */
					tcp_rst(_ackno, _seqno + _tcplen, ip_current_dest_addr(),
							ip_current_src_addr(), _tcphdr->dest, _tcphdr->src);
				}
			} else if ((_flags & TCP_SYN) && (_seqno == pcb->rcv_nxt - 1)) {
				/* Looks like another copy of the SYN - retransmit our SYN-ACK */
				tcp_rexmit(pcb);
			}
			break;
		case CLOSE_WAIT:
			/* FALL THROUGH */
		case ESTABLISHED:
			tcp_receive(pcb);
			if (_recv_flags & TF_GOT_FIN) { /* passive close */
				tcp_ack_now(pcb);
				pcb->state = CLOSE_WAIT;
			}
			break;
		case FIN_WAIT_1:
			tcp_receive(pcb);
			if (_recv_flags & TF_GOT_FIN) {
				if ((_flags & TCP_ACK) && (_ackno == pcb->snd_nxt)) {
					PLOG("TCP connection closed: FIN_WAIT_1 %u -> %u.",
							_inseg.tcphdr->src, _inseg.tcphdr->dest);
					tcp_ack_now(pcb);
					tcp_pcb_purge(pcb);
					pcb->state = TIME_WAIT;
					TCP_REG(&_tcp_tw_pcbs, pcb);
				} else {
					tcp_ack_now(pcb);
					pcb->state = CLOSING;
				}
			} else if ((_flags & TCP_ACK) && (_ackno == pcb->snd_nxt)) {
				pcb->state = FIN_WAIT_2;
			}
			break;
		case FIN_WAIT_2:
			tcp_receive(pcb);
			if (_recv_flags & TF_GOT_FIN) {
				PLOG("TCP connection closed: FIN_WAIT_2 %u -> %u.",
						_inseg.tcphdr->src, _inseg.tcphdr->dest);
				tcp_ack_now(pcb);
				tcp_pcb_purge(pcb);
				TCP_RMV_ACTIVE(pcb);
				pcb->state = TIME_WAIT;
				TCP_REG(&_tcp_tw_pcbs, pcb);
			}
			break;
		case CLOSING:
			tcp_receive(pcb);
			if (_flags & TCP_ACK && _ackno == pcb->snd_nxt) {
				PLOG("TCP connection closed: CLOSING %u -> %u.",
						_inseg.tcphdr->src, _inseg.tcphdr->dest);
				tcp_pcb_purge(pcb);
				TCP_RMV_ACTIVE(pcb);
				pcb->state = TIME_WAIT;
				TCP_REG(&_tcp_tw_pcbs, pcb);
			}
			break;
		case LAST_ACK:
			tcp_receive(pcb);
			if (_flags & TCP_ACK && _ackno == pcb->snd_nxt) {
				PLOG("TCP connection closed: LAST_ACK %u -> %u.",
						_inseg.tcphdr->src, _inseg.tcphdr->dest);
				/* bugfix #21699: don't see pcb->state to CLOSED here or we risk leaking segments */
				_recv_flags |= TF_CLOSED;
			}
			break;
		default:
			break;
	}
	PLOG("ends.");
	return ERR_OK;
}

/**
 * Print a tcp flags for debugging purposes.
 *
 * @param flags tcp flags, all active flags are printed
 */
void Exo_stack::tcp_debug_print_flags(u8_t flags)
{
	printf("[EXO-LIB]: tcp_debug_print_flags: ");

	if (flags & TCP_FIN) {
		printf("FIN ");
	}
	if (flags & TCP_SYN) {
		printf("SYN ");
	}
	if (flags & TCP_RST) {
		printf("RST ");
	}
	if (flags & TCP_PSH) {
		printf("PSH ");
	}
	if (flags & TCP_ACK) {
		printf("ACK ");
	}
	if (flags & TCP_URG) {
		printf("URG ");
	}
	if (flags & TCP_ECE) {
		printf("ECE ");
	}
	if (flags & TCP_CWR) {
		printf("CWR ");
	}
	printf("\n");
}

/**
 * Parses the options contained in the incoming segment.
 *
 * Called from tcp_listen_input() and tcp_process).
 * Currently, only the MSS option is supported!
 *
 * @param pcb the tcp_pcb for which a segment arrived
 */
void Exo_stack::tcp_parseopt(struct tcp_pcb *pcb)
{
	PLOG();
	u16_t c, max_c;
	u16_t mss;
	u8_t *opts, opt;

	opts = (u8_t *)_tcphdr + TCP_HLEN;

	/* Parse the TCP MSS option, if present. */
	if (TCPH_HDRLEN(_tcphdr) > 0x5) {
		max_c = (TCPH_HDRLEN(_tcphdr) - 5) << 2;
		for (c = 0; c < max_c; ) {
			opt = opts[c];
			switch (opt) {
				case 0x00:
					/* End of options. */
					PLOG("EOL");
					return;
				case 0x01:
					/* NOP option. */
					++c;
					PLOG("NOP");
					return;
				case 0x02:
					/* MSS */
					if (opts[c + 1] != 0x04 || c + 0x04 > max_c) {
						/* Bad length */
						PLOG("bad length");
						return;
					}
					/* An MSS option iwth the right option length. */
					mss = (opts[c + 2] << 8) | opts[c + 3];
					/* Limit the mss to the configured TCP_MSS and prevent division by zero */
					pcb->mss = ((mss > TCP_MSS) || (mss == 0)) ? TCP_MSS : mss;
					/* Advance to next option */
					c += 0x04;
					break;
				default:
					PLOG("other");
					if (opts[c + 1] == 0) {
						PLOG("bad length");
						/* If the length field is zero, the options are malformed
						   and we don't process them further. */
						return;
					}
					/* All other options have a length field, so that we easily
					   can skip past them. */
					c += opts[c + 1];
			}
		}
	}
	PLOG("ends.");
}

/**
 * Print a tcp state for debugging purposes.
 *
 * @param s enum tcp_state to print
 */
void Exo_stack::tcp_debug_print_state(enum tcp_state s)
{
	printf(" State: %s\n", _tcp_state_str[s]);
}

err_t Exo_stack::client_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	PLOG("Number of bytes ACK'ed is %d", len);
	client_close(pcb);
	return ERR_OK;
}

void Exo_stack::client_close(struct tcp_pcb *pcb)
{
	tcp_sent(pcb, NULL);
	tcp_close(pcb);

	PLOG("Closing...");
}

/**
 * Write data for sending (but does not send it immediately).
 *
 * It waits in the expectation of more data being sent soon (as
 * it can send them more efficiently by combining them together).
 * To prompt the system to send data now, call tcp_output() after
 * calling tcp_write().
 *
 * @param pcb Protocol control block for the TCP connection to enqueue data for.
 * @param arg Pointer to the data to be enqueued for sending.
 * @param len Data length in bytes
 * @param apiflags combination of following flags :
 * - TCP_WRITE_FLAG_COPY (0x01) data will be copied into memory belonging to the stack
 * - TCP_WRITE_FLAG_MORE (0x02) for TCP connection, PSH flag will be set on last segment sent,
 * @return ERR_OK if enqueued, another err_t on error
 */
err_t Exo_stack::tcp_write(struct tcp_pcb *pcb, const void *arg, u16_t len, u8_t apiflags)
{
	PLOG("starts.");
	struct pbuf2 *concat_p = NULL;
	struct tcp_seg2 *last_unsent = NULL, *seg = NULL, *prev_seg = NULL, *queue = NULL;
	u16_t pos = 0; /* position in 'arg' data */
	u16_t queuelen;
	u8_t optlen = 0;
	u8_t optflags = 0;
	u16_t oversize = 0;
	u16_t oversize_used = 0;
	err_t err;
	/* don't allocate segment bigger than half the maximum window we ever received */
	u16_t mss_local = EXO_MIN(pcb->mss, pcb->snd_wnd_max/2);

	PLOG("pcb=%p data=%p len=%u apiflags=%u", (void *)pcb, (char*)arg, len, (u16_t)apiflags);
	if (arg == NULL) {
		PLOG("arg == NULL (programmer violates API))");
		return ERR_ARG;
	}

	err = tcp_write_checks(pcb, len);
	if (err != ERR_OK) {
		return err;
	}
	queuelen = pcb->snd_queuelen;

  /*
   * TCP segmentation is done in three phases with increasing complexity:
   *
   * 1. Copy data directly into an oversized pbuf.
   * 2. Chain a new pbuf to the end of pcb->unsent.
   * 3. Create new segments.
   *
   * We may run out of memory at any point. In that case we must
   * return ERR_MEM and not change anything in pcb. Therefore, all
   * changes are recorded in local variables and committed at the end
   * of the function. Some pcb fields are maintained in local copies:
   *
   * queuelen = pcb->snd_queuelen
   * oversize = pcb->unsent_oversize
   *
   * These variables are set consistently by the phases:
   *
   * seg points to the last segment tampered with.
   *
   * pos records progress as data is segmented.
   */
   
	/* Find the tail of the unsent queue. */
	if (pcb->unsent != NULL) {
		PLOG("HELLO? %d", __LINE__);
		u16_t space;
		u16_t unsent_optlen;

		/* @todo: this could be sped up by keeping last_unsent in the pcb */
		for (last_unsent = pcb->unsent; last_unsent->next != NULL;
				last_unsent = last_unsent->next);

		/* Usable space at the end of the last unsent segment */
		unsent_optlen = EXO_TCP_OPT_LENGTH(last_unsent->flags);
		space = mss_local - (last_unsent->len + unsent_optlen);

		/*
		 * Phase 1: Copy data directly into an oversize pbuf.
     *
     * The number of bytes copied is recorded in the oversize_used
     * variable. The actual copying is done at the bottom of the
     * function.		 
		 */
		assert(pcb->unsent_oversize == last_unsent->oversize_left);
		oversize = pcb->unsent_oversize;
		if (oversize > 0) {
			assert(oversize_used <= space);
			seg = last_unsent;
			oversize_used = oversize < len ? oversize : len;
			pos += oversize_used;
			oversize -= oversize_used;
			space -= oversize_used;
		}
		/* now we are either finished or oversize is zero */
		assert((oversize ==0) || (pos == len));

		/*
		 * Phase 2: Chain a new pbuf to the end of pcb->unsent.
     *
     * We don't extend segments containing SYN/FIN flags or options
     * (len==0). The new pbuf is kept in concat_p and pbuf_cat'ed at
     * the end.		 
		 */
		if ((pos < len) && (space > 0) && (last_unsent->len > 0)) {
			u16_t seglen = space < len - pos ? space : len - pos;
			seg = last_unsent;

			/* Create a pbuf with a copy or reference to seglen bytes. We
			 * can use PBUF_RAW here since the data appears in the middle of
			 * a segment. A header will never by prepended. */
			if (apiflags & TCP_WRITE_FLAG_COPY) {
				/* Data is copied */
				if ((concat_p = tcp_pbuf2_prealloc(PBUF_RAW, seglen, space, &oversize, pcb, apiflags, 1)) == NULL) {
					PLOG("could not allocate memory for pbuf copy side %u", seglen);
					goto memerr;
				}
				last_unsent->oversize_left += oversize;
				TCP_DATA_COPY2(concat_p->payload, (u8_t*)arg + pos, seglen, &concat_chksum, &concat_chksum_swapped);
			} else {
				/* Data is not copied */
				if ((concat_p = pbuf2_alloc(PBUF_RAW, seglen, PBUF_ROM)) == NULL) {
					PLOG("could not allocate memory for zero-copy pbuf");
					goto memerr;
				}
				concat_p->payload = (u8_t*)arg + pos;
			}
			pos += seglen;
			queuelen += pbuf2_clen(concat_p);
		}
	} else {
		assert(pcb->unsent_oversize == 0);
	}

	/*
	 * Phase 3: Create new segments.
	 *
	 * The new segments are chained together in the local 'queue'
	 * variable, ready to be appended to pcb->unsent.
	 */
	while (pos < len) {
		struct pbuf2 *p;
		u16_t left = len - pos;
		u16_t max_len = mss_local - optlen;
		u16_t seglen = left > max_len ? max_len : left;

		if (apiflags & TCP_WRITE_FLAG_COPY) {
			/* If copy is set, memory should be allocated and data copied
			 * into pbuf */
			PLOG("seglen %u", seglen);
			if ((p = tcp_pbuf2_prealloc(PBUF_TRANSPORT, seglen + optlen, mss_local, &oversize, pcb, apiflags, queue == NULL)) == NULL) {
				PLOG("could not allocate memory for pbuf copy size %u", seglen);
				goto memerr;
			}
			assert(p->len >= seglen);
			TCP_DATA_COPY2((char *)p->payload + optlen, (u8_t *)arg + pos, seglen, &chksum, &chksum_swapped);
		} else {
			/* Copy is not set: First allocate a pbuf for holding the data.
			 * Since the referenced data is available at least until it is
			 * sent out on the link (as it has to be ACKed by the remote
			 * party) we can safely use PBUF_ROM instead of PBUF_REF here.
			 */
			struct pbuf2 *p2;
			assert(oversize == 0);
			// Check PBUF_ROM --> PBUF_RAM
			if ((p2 = pbuf2_alloc(PBUF_TRANSPORT, seglen, PBUF_ROM)) == NULL) {
				PLOG("could not allocate memor for zero-copy pbuf");
				goto memerr;
			}
			/* reference the non-volatile payload data */
			p2->payload = (u8_t *)arg + pos;

			/* Second, allocate a pbuf for the headers. */
			if ((p = pbuf2_alloc(PBUF_TRANSPORT, optlen, PBUF_RAM)) == NULL) {
				/* If allocation fails, we have to deallocate the data pbuf as
				 * well */
				pbuf2_free(p2);
				PLOG("could not allocate memory for header pbuf");
				goto memerr;
			}
			/* Concatenate the headers and data pbufs together. */
			pbuf2_cat(p, p2);
		}

		queuelen += pbuf2_clen(p);

		/* Now that there are more segments queued, we check again if the
		 * length of the queue exceeds the configured maximum or
		 * overflows. */
		if ((queuelen > TCP_SND_QUEUELEN) || (queuelen > TCP_SNDQUEUELEN_OVERFLOW)) {
			PLOG("queue too long %u (%u)", queuelen, TCP_SND_QUEUELEN);
			pbuf2_free(p);
			goto memerr;
		}

		if ((seg = tcp_create_segment(pcb, p, 0, pcb->snd_lbb + pos, optflags)) == NULL) {
			PLOG("seg == NULL: line %d", __LINE__);
			goto memerr;
		}

		seg->oversize_left = oversize;
		/* first segment of to-be-queued data? */
		if (queue == NULL) {
			queue = seg;
		} else {
			/* Attach the segment to the end of the queue segments */
			assert (prev_seg != NULL);
			prev_seg->next = seg;
		}
		/* remember last segment of to-be-queued data for next iteration */
		prev_seg = seg;

		PLOG("queueing %u: %u", 
				ntohl(seg->tcphdr->seqno), ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg));

		pos += seglen;
	}

	/* 
	 * All three segmentation phases were successful. We can commit the
	 * transaction.
	 */

	/*
	 * Phase 1: If data has been added to the preallocated tail of
	 * last_sent, we update the length fields of the pbuf chain.
	 */
	if (oversize_used > 0) {
		struct pbuf2 *p;
		/* Bump tot_len of whole chain, len of tail */
		for (p = last_unsent->p; p; p = p->next) {
			p->tot_len += oversize_used;
			if (p->next == NULL) {
				TCP_DATA_COPY((char *)p->payload + p->len, arg, oversize_used, last_unsent);
				p->len += oversize_used;
			}
		}
		last_unsent->len += oversize_used;
		assert (last_unsent->oversize_left >= oversize_used);
		last_unsent->oversize_left -= oversize_used;
	}
	pcb->unsent_oversize = oversize;

	/* 
	 * Phase 2: concat_p can be concatenated onto last_unsent->p
	 */
	if (concat_p != NULL) {
		assert (last_unsent != NULL);
		pbuf2_cat(last_unsent->p, concat_p);
		last_unsent->len += concat_p->tot_len;
	}

	/*
	 * Phase 3: Append queue to pcb->unsent. Queue may be NULL, but that
	 * is harmless
	 */
	if (last_unsent == NULL) {
		pcb->unsent = queue;
	} else {
		last_unsent->next = queue;
	}

	/*
	 * Finally update the pcb state.
	 */
	pcb->snd_lbb += len;
	pcb->snd_buf -= len;
	pcb->snd_queuelen = queuelen;

	PLOG("pcb->snd_queuelen %d (after enqueued)", pcb->snd_queuelen);
	if (pcb->snd_queuelen != 0) {
		assert(pcb->unacked != NULL || pcb->unsent != NULL);
	}

	/* Set the PSH flag in the last segment that we enqueued. */
	if (seg != NULL && seg->tcphdr != NULL && ((apiflags & TCP_WRITE_FLAG_MORE) == 0)) {
		TCPH_SET_FLAG(seg->tcphdr, TCP_PSH);
	}
	PLOG("seg->p->tot_len %u", seg->p->tot_len);
	PLOG("ends.");
	return ERR_OK;

memerr:
	pcb->flags |= TF_NAGLEMEMERR;
	if (concat_p != NULL) {
		pbuf2_free(concat_p);
	}
	if (queue != NULL) {
		tcp_segs_free(queue);
	}
	if (pcb->snd_queuelen != 0) {
		assert(pcb->unacked != NULL || pcb->unsent != NULL);
	}
	PLOG("pcb->snd_queuelen %d (with mem err)", pcb->snd_queuelen);
	return ERR_MEM;
}

/** Allocate a pbuf and create a tcphdr at p->payload, used for output
 * functions other than the default tcp_output -> tcp_output_segment
 * (e.g. tcp_send_empty_ack, etc.)
 *
 * @param pcb tcp pcb for which to send a packet (used to initialize tcp_hdr)
 * @param optlen length of header-options
 * @param datalen length of tcp data to reserve in pbuf
 * @param seqno_be seqno in network byte order (big-endian)
 * @return pbuf with p->payload being the tcp_hdr
 */
pbuf2* Exo_stack::tcp_output_alloc_header(struct tcp_pcb *pcb, u16_t optlen,
		u16_t datalen, u32_t seqno_be /* already in network byte order */)
{
	struct tcp_hdr *tcphdr;
	struct pbuf2 *p = pbuf2_alloc(PBUF_IP, TCP_HLEN + optlen + datalen, PBUF_RAM);
	if (p != NULL) {
		assert(p->len >= TCP_HLEN + optlen);
		tcphdr = (struct tcp_hdr *)p->payload;
		tcphdr->src = htons(pcb->local_port);
		tcphdr->dest = htons(pcb->remote_port);
		tcphdr->seqno = seqno_be;
		tcphdr->ackno = htonl(pcb->rcv_nxt);
		TCPH_HDRLEN_FLAGS_SET(tcphdr, (5 + optlen / 4), TCP_ACK);
		tcphdr->wnd = htons(pcb->rcv_ann_wnd);
		tcphdr->chksum = 0;
		tcphdr->urgp = 0;

		/* If we're sending a packet, update the announced right window edge */
		pcb->rcv_ann_right_edge = pcb->rcv_nxt + pcb->rcv_ann_wnd;
	}
	return p;
}

/** Checks if tcp_write is allowed or not (checks state, snd_buf and snd_queuelen).
 *
 * @param pcb the tcp pcb to check for
 * @param len length of data to send (checked agains snd_buf)
 * @return ERR_OK if tcp_write is allowed to proceed, another err_t otherwise
 */
err_t Exo_stack::tcp_write_checks(struct tcp_pcb *pcb, u16_t len)
{
	/* connection is in invalid state for data transmission? */
	if ((pcb->state != ESTABLISHED) &&
			(pcb->state != CLOSE_WAIT) &&
			(pcb->state != SYN_SENT) &&
			(pcb->state != SYN_RCVD)) {
		PLOG("called in invalid state");
		return ERR_CONN;
	} else if (len == 0) {
		return ERR_OK;
	}

	/* fail on too much data */
	if (len > pcb->snd_buf) {
		PLOG("too much data (len=%u > snd_buf=%u)", len, pcb->snd_buf);
		pcb->flags |= TF_NAGLEMEMERR;
		return ERR_MEM;
	}

	PLOG("queuelen: %u", pcb->snd_queuelen);

	/* If total number of pbufs on the unsent/unacked queues exceeds the
	 * configured maximum, return an error */
	/* check for configured max queuelen and possible overflow */
	if ((pcb->snd_queuelen >= TCP_SND_QUEUELEN) || (pcb->snd_queuelen > TCP_SNDQUEUELEN_OVERFLOW)) {
		PLOG("too long queue %u (max %u)", pcb->snd_queuelen, TCP_SND_QUEUELEN);
		pcb->flags |= TF_NAGLEMEMERR;
		return ERR_MEM;
	}
	if (pcb->snd_queuelen != 0) {
		/* pbufs on queue => at least one queue non-empty */
		assert(pcb->unacked != NULL || pcb->unsent != NULL);
	} else {	
		/* no pbufs on queue => both queues empty */
		assert(pcb->unacked == NULL && pcb->unsent == NULL);
	}
	return ERR_OK;
}

/**
 * Called by tcp_process. Checks if the given segment is an ACK for outstanding
 * data, and if so frees the memory of the buffered data. Next, is places the
 * segment on any of the receive queues (pcb->recved or pcb->ooseq). If the segment
 * is buffered, the pbuf is referenced by pbuf_ref so that it will not be freed until
 * it has been removed from the buffer.
 *
 * If the incoming segment constitutes an ACK for a segment that was used for RTT
 * estimation, the RTT is estimated here as well.
 *
 * Called from tcp_process().
 */
void Exo_stack::tcp_receive(struct tcp_pcb *pcb)
{
	struct tcp_seg2 *next;
	struct tcp_seg2 *prev, *cseg;
	struct pbuf2 *p;
	s32_t off;
	s16_t m;
	u32_t right_wnd_edge;
	u16_t new_tot_len;
	int found_dupack = 0;

	assert(pcb->state >= ESTABLISHED);

	if (_flags & TCP_ACK) {
		right_wnd_edge = pcb->snd_wnd + pcb->snd_wl2;

		/* Update window. */
		if (TCP_SEQ_LT(pcb->snd_wl1, _seqno) ||
				(pcb->snd_wl1 == _seqno && TCP_SEQ_LT(pcb->snd_wl2, _ackno)) ||
				(pcb->snd_wl2 == _ackno && _tcphdr->wnd > pcb->snd_wnd)) {
			pcb->snd_wnd = _tcphdr->wnd;
			/* keep track of the biggest window announced by the remote host to calculate
			   maximum segment size */
			if (pcb->snd_wnd_max < pcb->snd_wnd) {
				pcb->snd_wnd_max = pcb->snd_wnd;
			}
			pcb->snd_wl1 = _seqno;
			pcb->snd_wl2 = _ackno;
			if (pcb->snd_wnd == 0) {
				if (pcb->persist_backoff == 0) {
					/* start persist timer */
					pcb->persist_cnt = 0;
					pcb->persist_backoff = 1;
				}
			} else if (pcb->persist_backoff > 0) {
				/* stop persist timer */
				pcb->persist_backoff = 0;
			}
			PLOG("window update %u", pcb->snd_wnd);
		} else {
			if (pcb->snd_wnd != _tcphdr->wnd) {
				PLOG("no window update lastack %u ackno %u wl1 %u seqno %u wl2 %u",
						pcb->lastack, _ackno, pcb->snd_wl1, _seqno, pcb->snd_wl2);
			}
		}

		/* (From STevens TCP/IP Illustrated Vol II, p970.) Its only a
		 * duplicate ack if:
		 * 1) It doesn't ACK new data
		 * 2) length of received packet is zero (i.e. no payload)
		 * 3) the advertised window hasn't changed
		 * 4) There is outstanding unacknowledged data (retransmission timer running)
		 * 5) The ACK is == biggest ACK sequence number so far seen (snd_una)
		 * 
		 * If it passes all five, should process as a dupack:
		 * a) dupacks < 3: do nothing
		 * b) dupacks == 3: fast retransmit
		 * c) dupacks > 3: increase cwnd
		 *
		 * If it only passes 1-3, should reset dupack counter (and add to
		 * stats, which we don't do in lwIP)
		 *
		 * It it only passes 1, should reset dupack counter
		 *
		 */
		
		/* Clause 1 */
		if (TCP_SEQ_LEQ(_ackno, pcb->lastack)) {
			pcb->acked = 0;
			/* Clause 2 */
			if (_tcplen == 0) {
				/* Clause 3 */
				if (pcb->snd_wl2 + pcb->snd_wnd == right_wnd_edge) {
					/* Clause 4 */
					if (pcb->rtime >= 0) {
						/* Clause 5 */
						if (pcb->lastack == _ackno) {
							found_dupack = 1;
							if ((u8_t)(pcb->dupacks + 1) > pcb->dupacks) {
								++pcb->dupacks;
							}
							if (pcb->dupacks > 3) {
								/* Inflate the congestion window, but not if it means that
								   the value overflows. */
								if ((tcpwnd_size_t)(pcb->cwnd + pcb->mss) > pcb->cwnd) {
									pcb->cwnd += pcb->mss;
								}
							} else if (pcb->dupacks == 3) {
								/* Do fast retransmit */
								tcp_rexmit_fast(pcb);
							}
						}
					}
				}
			}
			/* If Clause (1) or more is true, but not a duplicate ack, reset
			 * count of consecutive duplicate acks */
			if (!found_dupack) {
				pcb->dupacks = 0;
			}
		} else if (TCP_SEQ_BETWEEN(_ackno, pcb->lastack+1, pcb->snd_nxt)) {
			/* We come here when the ACK acknowledges new data. */

			/* Reset the "IN Fast Retransmit" flag, since we are no longer
			   in fast retransmit. Also reset the congestion window to the
				 slow start threshold. */
			if (pcb->flags & TF_INFR) {
				pcb->flags &= ~TF_INFR;
				pcb->cwnd = pcb->ssthresh;
			}

			/* Reset the number of retransmissions. */
			pcb->nrtx = 0;

			/* Reset the retransmission time-out. */
			pcb->rto = (pcb->sa >> 3) + pcb->sv;

			/* Update the send buffer space. Diff between the two can nver exceed 64K
			   unless window scaling is used. */
			pcb->acked = (tcpwnd_size_t)(_ackno - pcb->lastack);

			pcb->snd_buf += pcb->acked;

			/* Reset the fast retransmit variables. */
			pcb->dupacks = 0;
			pcb->lastack = _ackno;

			/* Update the congestion control variables (cwnd and
		     ssthresh). */
			if (pcb->state >= ESTABLISHED) {
				if (pcb->cwnd < pcb->ssthresh) {
					if ((tcpwnd_size_t)(pcb->cwnd + pcb->mss) > pcb->cwnd) {
						pcb->cwnd += pcb->mss;
					}
					PLOG("slow start cwnd %u", pcb->cwnd);
				} else {
					tcpwnd_size_t new_cwnd = (pcb->cwnd + pcb->mss * pcb->mss / pcb->cwnd);
					if (new_cwnd > pcb->cwnd) {
						pcb->cwnd = new_cwnd;
					}
					PLOG("congestion avoidance cwnd %u", pcb->cwnd);
				}
			}
			PLOG("ACK For %u, unacked->seqno %u:%u", _ackno,
					pcb->unacked != NULL? ntohl(pcb->unacked->tcphdr->seqno): 0,
					pcb->unacked != NULL? ntohl(pcb->unacked->tcphdr->seqno) +
							TCP_TCPLEN(pcb->unacked): 0);

			/* Remove segment from the unackowledged list if the incoming
			   ACK acknowledges them. */
			while (pcb->unacked != NULL &&
					TCP_SEQ_LEQ(ntohl(pcb->unacked->tcphdr->seqno) +
					TCP_TCPLEN(pcb->unacked), _ackno)) {
				PLOG("removing %u:%u from pcb->unacked",
						ntohl(pcb->unacked->tcphdr->seqno),
						ntohl(pcb->unacked->tcphdr->seqno) + TCP_TCPLEN(pcb->unacked));
				next = pcb->unacked;
				pcb->unacked = pcb->unacked->next;

				PLOG("queeulen %u...", pcb->snd_queuelen);
				assert(pcb->snd_queuelen >= pbuf2_clen(next->p));
				/* Prevent ACK for FIN to generate a sent event */
				if ((pcb->acked != 0) && ((TCPH_FLAGS(next->tcphdr) & TCP_FIN) != 0)) {
					pcb->acked--;
				}

				pcb->snd_queuelen -= pbuf2_clen(next->p);
				tcp_seg_free(next);

				PLOG("queuelen %u (after freeing unacked)", pcb->snd_queuelen);
				if (pcb->snd_queuelen != 0) {
					assert(pcb->unacked != NULL || pcb->unsent != NULL);
				}
			}
			
			/* If there's nothing left to acknowledge, stop the retransmit
			   timer, otherwise reset it ot start again */
			if (pcb->unacked == NULL) {
				pcb->rtime = -1;
			} else {
				pcb->rtime = 0;
			}

			pcb->polltmr = 0;
		} else {
			pcb->acked = 0;
		}

		/* We go through the ->unsent list to see if any of the segments
		   on the list are acknowledged by the ACK. This may seem
			 strange since an "unsent" segment shouldn't be acked. The
			 rationale is that lwIP puts all outstanding segments on the
			 ->unsent list after a retransmission, so these segments may
			 in fact have been sent once. */
		while (pcb->unsent != NULL &&
				TCP_SEQ_BETWEEN(_ackno, ntohl(pcb->unsent->tcphdr->seqno) +
				TCP_TCPLEN(pcb->unsent), pcb->snd_nxt)) {
			PLOG("removing %u: %u from pcb->unsent",
					ntohl(pcb->unsent->tcphdr->seqno),
					ntohl(pcb->unsent->tcphdr->seqno) + TCP_TCPLEN(pcb->unsent));
			next = pcb->unsent;
			pcb->unsent = pcb->unsent->next;
			if (pcb->unsent == NULL) {
				pcb->unsent_oversize = 0;
			}
			PLOG("queuelen %u", pcb->snd_queuelen);
			assert(pcb->snd_queuelen >= pbuf2_clen(next->p));
			/* Prevent ACK for FIN to generate a sent event */
			if ((pcb->acked != 0) && ((TCPH_FLAGS(next->tcphdr) & TCP_FIN) != 0)) { 
				pcb->acked--;
			}
			pcb->snd_queuelen -= pbuf2_clen(next->p);
			tcp_seg_free(next);
			PLOG("(after freeing unsent) %u", pcb->snd_queuelen);
			if (pcb->snd_queuelen != 0) {
				assert(pcb->unacked != NULL || pcb->unsent != NULL);
			}
		}
		/* End of ACK for new data processing */
		PLOG("pcb->rttest %u rtseq %u ackno %u", pcb->rttest, pcb->rtseq, _ackno);

		/* RTT estimation calculations. This is done by checking if the
		   incoming segment acknowledges the segment we use to take a
			 round-trip time measurement. */
		if (pcb->rttest && TCP_SEQ_LT(pcb->rtseq, _ackno)) {
			panic("NOT IMPLEMENTED!: line %d", __LINE__);
		}
	}
			
	/* IF the incoming segment contains data, we must process it
	   further unless the pcb already received a FIN.
		 (RFC 793, chapter 3.9, "SEGMENT ARRIVES" in states CLOSE-WAIT, CLOSING
		 LAST-ACK and TIME-WAIT: "Ignore the segment text.") */
	if ((_tcplen > 0) && (pcb->state < CLOSE_WAIT)) {
		if (TCP_SEQ_BETWEEN(pcb->rcv_nxt, _seqno + 1, _seqno + _tcplen - 1)) {
			off = pcb->rcv_nxt - _seqno;
			p = _inseg.p;
			assert(_inseg.p);
			assert(off < 0x7fff);
			if (_inseg.p->len < off) {
				assert(((s32_t)_inseg.p->tot_len) >= off);
				new_tot_len = (u16_t)(_inseg.p->tot_len - off);
				while (p->len < off) {
					off -= p->len;
					/* KJM following line changed (with addition of new_tot_len var)
					   to fix bug #9076
						 inseg.p->tot_len -= p->len; */
					p->tot_len = new_tot_len;
					p->len = 0;
					p = p->next;
				}
				if (pbuf2_header(p, (s16_t)-off)) {
					/*Do we need to cope with this failing? Assert for now */
					assert(0);
				}
			} else {
				if (pbuf2_header(p, (s16_t)-off)) {
					/* Do we need to cope with this failing? Assert for now */
					assert(0);
				}
			}
			_inseg.len -= (u16_t)(pcb->rcv_nxt - _seqno);
			_inseg.tcphdr->seqno = _seqno = pcb->rcv_nxt;
		}
		else {
			if (TCP_SEQ_LT(_seqno, pcb->rcv_nxt)) {
				/* the whole segment is < rcv_nxt */
				/* must be a duplicate of a packet that has already been correctly handled */
				PLOG("duplicate seqno %u", _seqno);
				tcp_ack_now(pcb);
			}
		}
		/* The sequence number must be within the window (above rcv_nxt
		   and below rcv_nxt + rcv_wnd) in order to be further
			 processed. */
		if (TCP_SEQ_BETWEEN(_seqno, pcb->rcv_nxt,
				pcb->rcv_nxt + pcb->rcv_wnd - 1)) {
			if (pcb->rcv_nxt == _seqno) {
				/* The incoming segment is the next in sequence. We check if
				   we have to trim the end of the segment and update rcv_nxt
					 and pass the data to the application. */
				_tcplen = TCP_TCPLEN(&_inseg);

				if (_tcplen > pcb->rcv_wnd) {
					PLOG("other end overran receive window: seqno %u len %u right edge %u",
							_seqno, _tcplen, pcb->rcv_nxt + pcb->rcv_wnd);
					if (TCPH_FLAGS(_inseg.tcphdr) & TCP_FIN) {
						/* Must remove the FIN from the header as we're trimming
						 * that byte of sequence-space from the packet */
						TCPH_FLAGS_SET(_inseg.tcphdr, TCPH_FLAGS(_inseg.tcphdr) &~ TCP_FIN);
					}
					/* Adjust length of segment to fit in the window. */
					_inseg.len = pcb->rcv_wnd;
					if (TCPH_FLAGS(_inseg.tcphdr) & TCP_SYN) {
						_inseg.len -= 1;
					}
					pbuf2_realloc(_inseg.p, _inseg.len);
					_tcplen = TCP_TCPLEN(&_inseg);
					assert((_seqno + _tcplen) == (pcb->rcv_nxt + pcb->rcv_wnd));
				}
				/* Received in-sequence data, adjust ooseq data if:
				   - FIN has been received or
					 - inseq overlaps with ooseq */
				if (pcb->ooseq != NULL) {
					if (TCPH_FLAGS(_inseg.tcphdr) & TCP_FIN) {
						PLOG("received in-order FIN, binning ooseq queue");
						panic("NOT IMPLEMENTED!: %d", __LINE__);
					} else {
						panic("NOT IMPLEMENTED!: %d", __LINE__);
						next = pcb->ooseq;
						/* Remove all segment on ooseq that are covered by inseg already.
						 * FIN is copied from ooseq to inseg if present. */
					}
				}

				pcb->rcv_nxt = _seqno + _tcplen;

				/* Update the receiver's (our) window. */
				assert(pcb->rcv_wnd >= _tcplen);
				pcb->rcv_wnd -= _tcplen;

				tcp_update_rcv_ann_wnd(pcb);

				/* If there is data in the segment, we make preparations to
				   pass this up to the application. The ->recv_data variable
					 is used for holding the pbuf that goes to the
					 application. The code for reassembling out-of-sequence data
					 chains its data on this pbuf as well.

					 If the segment was FIN, we set the TF_GOT_FIN flag that will
					 be used to indicate to the application that the remote side has
					 closed its end of the connection. */
				if (_inseg.p->tot_len > 0) {
					_recv_data = _inseg.p;
					/* Since this pbuf now is the responsibility of the
					   application, we delete our reference to it so that we won't
						 (mistakingly) deallocate it. */
					_inseg.p = NULL;
				}
				if (TCPH_FLAGS(_inseg.tcphdr) & TCP_FIN) {
					PLOG("received FIN.");
					_recv_flags |= TF_GOT_FIN;
				}
				while (pcb->ooseq != NULL &&
						pcb->ooseq->tcphdr->seqno == pcb->rcv_nxt) {
					cseg = pcb->ooseq;
					_seqno = pcb->ooseq->tcphdr->seqno;

					pcb->rcv_nxt += TCP_TCPLEN(cseg);
					assert(pcb->rcv_wnd >= TCP_TCPLEN(cseg));
					pcb->rcv_wnd -= TCP_TCPLEN(cseg);

					tcp_update_rcv_ann_wnd(pcb);

					if (cseg->p->tot_len > 0) {
						/* Chain this pbuf onto the pbuf that we will pass to
						   the application. */
						if (_recv_data) {
							pbuf2_cat(_recv_data, cseg->p);
						} else {
							_recv_data = cseg->p;
						}
						cseg->p = NULL;
					}
					if (TCPH_FLAGS(cseg->tcphdr) & TCP_FIN) {
						PLOG("dequeued FIN.");
						_recv_flags |= TF_GOT_FIN;
						if (pcb->state == ESTABLISHED) { /* force passive close or we can move to active close */
							pcb->state = CLOSE_WAIT;
						}
					}

					pcb->ooseq = cseg->next;
					tcp_seg_free(cseg);

				}
				
				/* Acknowledge the segment(s). */
				tcp_ack(pcb);

			} else {
				/* We get here if the incoming segment is out-of-sequence. */
				tcp_send_empty_ack(pcb);
				/* We queue the seguemtn on the ->ooseq queue. */
				if (pcb->ooseq == NULL) {
					panic("NOT IMPLEMENTED!: %d", __LINE__);
					//pcb->ooseq = tcp_seg_copy(&_inseg);
				} else {
					/* If the queue is not empty, we walk through the queue and
					   try to find a place where the sequence number of the 
						 incoming segment is between the sequence numbers of the 
						 previous and the next segment on the ->ooseq queue. That is
						 the place where we put the incoming segment. IF needed, we
						 trim the second edges of the previous and the incoming
						 segment so that it will fit into the sequence.

						 If the incoming segment has the same sequence number as a
						 segment on the ->ooseq queue, we discard the segment that
						 contains less data. */

					panic("NOT IMPLEMENTED!: %d", __LINE__);
				}
			}
		} else {
			/* The incoming segment is not within the window. */
			tcp_send_empty_ack(pcb);
		}
	} else {
		/* segments with length 0 is taken care of here. Segments that
		   fall out of the window are ACKed. */
		if (!TCP_SEQ_BETWEEN(_seqno, pcb->rcv_nxt, pcb->rcv_nxt + pcb->rcv_wnd-1)) {
			tcp_ack_now(pcb);
		}
	}
	PLOG("ends.");
}

void Exo_stack::tcp_rexmit(struct tcp_pcb *pcb)
{
	panic("NOT IMPLEMENTED!");
}

void Exo_stack::tcp_rexmit_fast(struct tcp_pcb *pcb)
{
	panic("NOT IMPLEMENTED!");
}

err_t Exo_stack::tcp_timewait_input(struct tcp_pcb *pcb)
{
	panic("NOT IMPLEMENTED!");
}

/**
 * Allocate a PBUF_RAM pbuf, perhaps with extra space at the end.
 *
 * This function is like pbuf_alloc(layer, length, PBUF_RAM) except
 * there may be extra bytes available at the end.
 *
 * @param layer flag to define header size.
 * @param length size of the pbuf's payload.
 * @param max_length maximum usable size of payload+oversize.
 * @param oversize pointer to a u16_t that will receive the number of usable tail bytes.
 * @param pcb The TCP connection that willo enqueue the pbuf.
 * @param apiflags API flags given to tcp_write.
 * @param first_seg true when this pbuf will be used in the first enqueued segment.
 * @param 
 */
struct pbuf2* Exo_stack::tcp_pbuf2_prealloc(pbuf_layer layer, u16_t length, 
			u16_t max_length, u16_t *oversize, struct tcp_pcb *pcb, u8_t apiflags,
			u8_t first_seg)
{
	PLOG("starts.");
	struct pbuf2 *p;
	u16_t alloc = length;

	if (length < max_length) {
		if ((apiflags & TCP_WRITE_FLAG_MORE) ||
				(!(pcb->flags & TF_NODELAY) &&
				(!first_seg ||
				pcb->unsent != NULL ||
				pcb->unacked != NULL))) {
			alloc = EXO_MIN(max_length, length + TCP_OVERSIZE);
		}
	}
	p = pbuf2_alloc(layer, alloc, PBUF_RAM);
	if (p == NULL) {
		return NULL;
	}
	assert (p->next == NULL);
	*oversize = p->len - length;
	p->len = p->tot_len = length;
	PLOG("ends.");
	return p;
}

void Exo_stack::pbuf2_realloc(struct pbuf2 *p, u16_t new_len)
{
	panic("NOT IMPLEMENTED!");
}

/**
 * Concatenate two pbufs (each may be a pbuf chain) and take over
 * the caller's reference of the tail pbuf.
 * 
 * @note The caller MAY NOT reference the tail pbuf afterwards.
 * Use pbuf_chain() for that purpose.
 * 
 * @see pbuf_chain()
 */
void pbuf2_cat(struct pbuf2 *h, struct pbuf2 *t)
{
	struct pbuf2 *p;

	if (!((h != NULL) && (t != NULL))) {
		PLOG("(h != NULL) && (t != NULL) (programmer viloates API)");
		return;
	}

	/* proceed to last pbuf of chain */
	for (p = h; p->next != NULL; p = p->next) {
		/* add total length of second chain to all totals of first chain */
		p->tot_len += t->tot_len;
	}
	/* { p is last pbuf of first h chain, p->next == NULL } */
	assert(p->tot_len == p->len);
	assert(p->next == NULL);
	/* add total length of second chain to last pbuf total of first chain */
	p->tot_len += t->tot_len;
	/* chain last pbuf of head (p) with first of tail (t) */
	p->next = t;
	/* p->next now references t, but the caller will drop its reference to t,
	 * so netto there is no change to the reference count of it.
	 */
}

/**
 * Copy (part of) the contents of a packet buffer
 * to an application supplied buffer.
 *
 * @param buf the pbuf from which to copy data
 * @param dataptr the application supplied buffer
 * @param len length of data to copy (dataptr must be big enough). No more 
 * than buf->tot_len will be copied, irrespective of len
 * @param offset offset into the packet buffer from where to begin copying len bytes
 * @return the number of bytes copied, or 0 on failure
 */
u16_t Exo_stack::pbuf2_copy_partial(struct pbuf2 *buf, void *dataptr, u16_t len, u16_t offset)
{
	panic("NOT IMPLEMENTED!");
}

/** sys_prot_t sys_arch_protect(void)

This optional function does a "fast" critical region protection and returns
the previous protection level. This function is only called during very short
critical regions. An embedded system which supports ISR-based drivers might
want to implement this function by disabling interrupts. Task-based systems
might want to implement this by using a mutex or disabling tasking. This
function should support recursive calls from the same task or interrupt. In
other words, sys_arch_protect() could be called while already protected. In
that case the return value indicates that it is already protected.

sys_arch_protect() is only required if your port is supporting an operating
system.
*/
sys_prot_t Exo_stack::sys_arch_protect(void)
{
	/* Note that for the UNIX port, we are using lightweight mutex, and our
	 * own counter (which is locked by the mutex). The return code is not actually
	 * used. */
	if (_exoprot_thread != pthread_self())
	{
		/* We are locking the mutex where it has not been locked before *
		 * or is being locked by another thread */
		pthread_mutex_lock(&_exoprot_mutex);
		_exoprot_thread = pthread_self();
		_exoprot_count = 1;
	} else
		/* It is already locked by THIS thread */
		_exoprot_count++;
	return 0;
}

/** void sys_arch_unprotect(sys_prot_t pval)

This optional function does a "fast" set of critical region protection to the
value specified by pval. See the documentation for sys_arch_protect() for
more information. This function is only required if your port is supporting
an operating system.
*/
void Exo_stack::sys_arch_unprotect(sys_prot_t pval)
{
	if (_exoprot_thread == pthread_self())
	{
		if (--_exoprot_count == 0)
		{
			_exoprot_thread = (pthread_t) 0xDEAD;
			pthread_mutex_unlock(&_exoprot_mutex);
		}
	}
}



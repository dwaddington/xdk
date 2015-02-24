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

#include "exo_stack.h"
#include <x540/x540_types.h>
#include <x540/x540_device.h>
#include <arpa/inet.h>
#include <common/cycles.h>

void Exo_stack::send_udp_pkt_test(unsigned tid, unsigned stream) {

  enum { PACKET_LEN = 64 };

#if 0
  unsigned bytes_to_alloc = PACKET_LEN;

  /* allocate pkt memory */
  size_t BLOCK_SIZE = bytes_to_alloc;
  size_t NUM_BLOCKS = 1;
  size_t actual_size = round_up_page(Memory::Fast_slab_allocator::actual_block_size(BLOCK_SIZE) 
                                                   * NUM_BLOCKS);

  /* allocate some memory */
  addr_t space_p = 0;

  //allocate memory in normal pages
#ifndef RAW_NIC_TEMP
  void * space_v = ((Intel_x540_uddk_device *)_inic->driver(_index))->
                                        alloc_dma_pages(actual_size/PAGE_SIZE+1, &space_p);
#else
	void * space_v = Exokernel::Memory::huge_malloc(actual_size/PAGE_SIZE+1, &space_p);
#endif

  assert(space_v);
  assert(space_p);

  Memory::Fast_slab_allocator * xmit_pkt_allocator =
      new Memory::Fast_slab_allocator(space_v,
                                      actual_size,
                                      BLOCK_SIZE,
                                      64, // alignment
                                      0, // core_id
                                      true,// needs phys
                                      90); // debug id

  PLOG("Fast_slab_allocator ctor OK for xmit_pkt_allocator.");

  addr_t pkt_v, pkt_p;
  pkt_v = (addr_t)xmit_pkt_allocator->alloc();
  assert(pkt_v!=0);
  __builtin_memset((void *)pkt_v, 0xff, bytes_to_alloc);
  pkt_p = (addr_t)xmit_pkt_allocator->get_block_phy_addr((void *)pkt_v);
#endif
 
  void * temp;
  if (_imem->alloc((addr_t *)&temp, PACKET_ALLOCATOR, _index, rx_core[tid]) != Exokernel::S_OK) {
    panic("PACKET_ALLOCATOR failed!\n");
  }
  assert(temp);

  addr_t pkt_p;
  pkt_p = _imem->get_phys_addr(temp, PACKET_ALLOCATOR, _index);
  assert(pkt_p);

  void * pkt_v = temp;

  unsigned queue = 2*tid+1;

  unsigned counter = 0;
  cpu_time_t start_time=0;
  cpu_time_t finish_time=0;

  //struct timespec interval;
  //interval.tv_sec = 0;
  //interval.tv_nsec = 1L;

  /* setting flow director signature */
  uint8_t * tmp = (uint8_t *)pkt_v;
  tmp[6] = 0;
  tmp[7] = 0xf & stream;

  while (1) {
    //nanosleep(&interval, NULL);
    sleep(1);
    if (counter==0)
      start_time=rdtsc();

    udp_send_pkt((uint8_t *)pkt_v, pkt_p, PACKET_LEN, queue, false, PACKET_ALLOCATOR);
    printf("%u sends a packet\n",tid);
    counter++;

    if (counter >= stats_num) {
      finish_time=rdtsc();
      printf("[NIC %u TX %u] TX THROUGHPUT (UDP payload %u bytes) %llu PPS\n",
             _index,
             tid,
             PACKET_LEN,
             (((unsigned long long)counter) * cpu_freq) / (finish_time - start_time));
      counter = 0;
    }
  }
}

void Exo_stack::send_pkt_test(unsigned tid) {

  enum { PACKET_LEN = 64 };

  unsigned char packet[PACKET_LEN] = {
        0xa0, 0x36, 0x9f, 0x16, 0xde, 0x84,
        0xa0, 0x36, 0x9f, 0x1a, 0x55, 0xea,
        0x08, 0x00, // type: ip datagram
        0x45, 0x00, // version
        0x00, 50, // length
        //0x00, 0x05, 0x40, 0x00, 0x3f, 0x11, 0x27, 0xb4,//ip header with checksum
        0x00, 0x05, 0x40, 0x00, 0x3f, 0x11, 0x00, 0x00,//ip header without checksum
        0x0a, 0x00, 0x00, 0x02, // src: 10.0.0.2
        0x0a, 0x00, 0x00, 0x01, // dst: 10.0.0.1
        0xdd, 0xd5, 0x2b, 0xcb, 0x00, 30, 0x00, 0x00, // UDP header
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21};

  size_t pkt_burst_size = 32;
  unsigned bytes_to_alloc = 1514;

#if 0
  /* allocate pkt memory */
  size_t BLOCK_SIZE = bytes_to_alloc;
  size_t NUM_BLOCKS = 1024;
  size_t actual_size = round_up_page(Memory::Fast_slab_allocator::actual_block_size(BLOCK_SIZE)  
						* NUM_BLOCKS);

  /* allocate some memory */
  addr_t space_p = 0;

  //allocate memory in normal pages
#ifndef RAW_NIC_TEMP
  void * space_v = ((Intel_x540_uddk_device *)_inic->driver(_index))->
                                    alloc_dma_pages(actual_size/PAGE_SIZE+1, &space_p);
#else
	void * space_v = Exokernel::Memory::huge_malloc(actual_size/PAGE_SIZE+1, &space_p);
#endif

  assert(space_v);
  assert(space_p);

  Memory::Fast_slab_allocator * xmit_pkt_allocator =
      new Memory::Fast_slab_allocator(space_v,
                                      actual_size,
                                      BLOCK_SIZE,
                                      64, // alignment
                                      0, // core_id
                                      true, //needs phys addr
                                      91); // debug id

  PLOG("Fast_slab_allocator ctor OK for xmit_pkt_allocator.");
#endif

  struct exo_mbuf * xmit_addr_pool[1024];
  unsigned i;
  addr_t pkt_v, pkt_p;

  /* test of 3 segments packet sending (total len 64 = 42 + 10 + 12) */
  for (i = 0; i < 1024; i++) {
    void * temp;
    if (_imem->alloc((addr_t *)&temp, PACKET_ALLOCATOR, _index, rx_core[tid]) != Exokernel::S_OK) {
      panic("PACKET_ALLOCATOR failed!\n");
    }
    assert(temp);
    pkt_v = (addr_t)temp;
    __builtin_memset((void *)pkt_v, 0, bytes_to_alloc);

    pkt_p = _imem->get_phys_addr((void *)pkt_v, PACKET_ALLOCATOR, _index);

    __builtin_memcpy((void *)pkt_v, (void *)packet, PACKET_LEN);

    xmit_addr_pool[i] = (struct exo_mbuf *)malloc(sizeof(struct exo_mbuf));
    __builtin_memset(xmit_addr_pool[i], 0, sizeof(struct exo_mbuf));
    xmit_addr_pool[i]->phys_addr = pkt_p;
    xmit_addr_pool[i]->virt_addr = pkt_v;
    xmit_addr_pool[i]->len = bytes_to_alloc;
    xmit_addr_pool[i]->nb_segment = 1;
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
      ethernet_output_simple(&xmit_addr_pool[counter % 512], n, queue);
      //ethernet_output(&xmit_addr_pool[counter % 512], n, queue);
      sent_num = n;
    }
    else {
      /* transmit more than the max burst, in chunks of TX_MAX_BURST */
      unsigned nb_pkts = pkt_burst_size;
      while(nb_pkts) {
        size_t ret, n;
        n = (uint16_t)EXO_MIN(nb_pkts, (unsigned)IXGBE_TX_MAX_BURST);
        ret = n;
        ethernet_output_simple(&xmit_addr_pool[counter % 512], (size_t&)ret, queue);
        //ethernet_output(&xmit_addr_pool[counter % 512], (size_t&)ret, queue);
        sent_num += ret;
        nb_pkts -= ret;
        if (ret < n)
          break;
      }
    }

    counter += sent_num;

    if (counter >= stats_num) {
      finish_time=rdtsc();
      printf("[TID %d] TX THROUGHPUT %llu PPS\n",
             tid,
             (((unsigned long long)counter) * cpu_freq) / (finish_time - start_time));
      counter = 0;
    }
  }
}

void Exo_stack::add_ip(char* myip) {
  int len = strlen(myip);
  char s[len+1];
  __builtin_memcpy(s,myip,len);
  s[len]='\0';
  parse_ip_format(s,ip);
  printf("CLIENT IP: %d.%d.%d.%d is added to STACK[%u]\n",ip[0],ip[1],ip[2],ip[3],_index);
  uint32_t temp=0;
  temp=(ip[0]<<24)+(ip[1]<<16)+(ip[2]<<8)+ip[3];
  remote_ip_addr.addr=ntohl(temp);
}

void Exo_stack::parse_ip_format(char *s, uint8_t * ip) {
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

void Exo_stack::init_param() {
  unsigned i,j;
#ifndef RAW_NIC_TEMP
  for (i = 0; i < 6; i++) {
    mac[i] = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->mac[i];
  }

  for (i = 0; i < 4; i++) {
    ip[i] = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->ip[i];
  }

  ethaddr = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->ethaddr;
  ip_addr = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->ip_addr;
#else
  for (i = 0; i < 6; i++) {
    mac[i] = ((Raw_device *)(_inic->driver(_index)))->_mac[i];
  }
  
  for (i = 0; i < 4; i++) {
    ip[i] = ((Raw_device *)(_inic->driver(_index)))->_ip[i];
  }
  
  ethaddr = ((Raw_device *)(_inic->driver(_index)))->_ethaddr;
  ip_addr = ((Raw_device *)(_inic->driver(_index)))->_ip_addr;
#endif

  for (i = 0; i < NUM_RX_QUEUES; i++) {
    __builtin_memset(&recv_counter[i], 0, sizeof(recv_counter[i]));
  }

  for (i = 0; i < ARP_TABLE_SIZE; i++) {
    __builtin_memset(&arp_table[i], 0, sizeof(struct etharp_entry));
  }

  /* protocol related init*/
  for (i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
    reassdatagrams[i] = NULL;
    udp_pcbs[i] = NULL;
#ifndef RAW_NIC_TEMP
    rx_core[i] = ((Intel_x540_uddk_device *)(_inic->driver(_index)))->rx_core[i];
#else
    rx_core[i] = ((Raw_device *)(_inic->driver(_index)))->rx_core[i];
#endif
  }

  udp_bind(&ip_addr, server_port);
  cpu_freq = (uint64_t)(get_tsc_frequency_in_mhz() * 1000000);

  /* create mbuf meta data structure */
  for (i = 0; i < NUM_TX_THREADS_PER_NIC; i++) {
    void * temp;
    if (_imem->alloc((addr_t *)&temp, META_DATA_ALLOCATOR, _index, rx_core[i]) != Exokernel::S_OK) {
      panic("META_DATA_ALLOCATOR failed!\n");
    }
    assert(temp);
    __builtin_memset(temp, 0, sizeof(void *) * IXGBE_TX_MAX_BURST);

    mbuf[i] = (struct exo_mbuf **)temp;

    for (j = 0; j < IXGBE_TX_MAX_BURST; j++) {
      if (_imem->alloc((addr_t *)&temp, MBUF_ALLOCATOR, _index, rx_core[i]) != Exokernel::S_OK) {
        panic("MBUF_ALLOCATOR failed!\n");
      }
      assert(temp);
      __builtin_memset(temp, 0, sizeof(struct exo_mbuf));

      mbuf[i][j] = (struct exo_mbuf *) temp;
    }
  }

  /* partition ip_iden space for different queues */
  ip_iden_space_per_queue = (0xffff)/NUM_TX_QUEUES;
  for (i = 0; i < NUM_TX_QUEUES; i++) {
    __builtin_memset(&ip_iden_counter[i],0,sizeof(ip_iden_counter[i]));
    ip_iden_base[i] = ip_iden_space_per_queue * i;
  }

}

status_t Exo_stack::ethernet_output(struct exo_mbuf ** pkt, size_t& cnt, unsigned queue) {
  status_t s = _inic->send_packets((pkt_buffer_t *)pkt, cnt, _index, queue);
  return s;
}

status_t Exo_stack::ethernet_output_simple(struct exo_mbuf ** pkt, size_t& cnt, unsigned queue) {
  status_t s = _inic->send_packets_simple((pkt_buffer_t *)pkt, cnt, _index, queue);
  return s;
}

pkt_status_t Exo_stack::ethernet_input(uint8_t * pkt, size_t len, unsigned queue) {
  struct eth_hdr* ethhdr;
  uint16_t type;
  pkt_status_t t;
  //unsigned tid = queue >> 1;
  //unsigned core = rx_core[tid];

  if (len <= SIZEOF_ETH_HDR) {
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

  unsigned num = stats_num;
  if (recv_counter[queue].ctr.pkt==num) {
    cpu_time_t finish_t=rdtsc();
        PLOG("[NIC %d CH %d @ CORE %d] RX THROUGHPUT %llu PPS (RAW ETH)",
             _index,
             tid,
             core,
             (((unsigned long long)num) * cpu_freq) / (finish_t - recv_counter[queue].ctr.timer));
    recv_counter[queue].ctr.pkt=0;
  }
#endif

  //return REUSE_THIS_PACKET;
  // _imem->free((void *)pkt, PACKET_ALLOCATOR, _index);
  //return KEEP_THIS_PACKET;

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
  return t;
}

pkt_status_t Exo_stack::ip_input(uint8_t *pkt, uint16_t len, unsigned queue) {
  struct ip_hdr *iphdr;
  uint16_t iphdr_hlen;
  uint16_t iphdr_len;
  bool packet_completed = true;
  pbuf_t* pbuf_list = NULL;
  pkt_status_t t = REUSE_THIS_PACKET;
  unsigned tid = queue >> 1;
  unsigned core = rx_core[tid];

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

  /* allocate pbuf */
  void * p;
  if (_imem->alloc((addr_t *)&p, PBUF_ALLOCATOR, _index, core) != Exokernel::S_OK) {
    panic("PBUF_ALLOCATOR failed!\n");
  }
  assert(p != NULL);

  __builtin_memset(p, 0, sizeof(pbuf_t));

  /* populate pbuf struct */
  pbuf_list = (pbuf_t *)p;
  pbuf_list->ippayload_len = iphdr_len-iphdr_hlen;
  pbuf_list->iphdr_len = iphdr_hlen;
  pbuf_list->pkt = pkt;
  if ((IPH_OFFSET(iphdr) & htons(IP_OFFMASK | IP_MF)) != 0) {
#if 0
    printf("IP packet is a fragment (id=0x%04x len=%d MF=%x offset=%d)\n",
                   ntohs(IPH_ID(iphdr)), ntohs(IPH_LEN(iphdr)), 
                   !!(IPH_OFFSET(iphdr) & htons(IP_MF)), 
                   (ntohs(IPH_OFFSET(iphdr)) & IP_OFFMASK)*8);
#endif
    /* reassemble the packet*/
    pbuf_list = ip_reass(pbuf_list, &t, queue);

    /* packet not fully reassembled yet? */
    if (pbuf_list == NULL) {
      packet_completed=false;
    }
    else {
      packet_completed=true;
    }
  }

  if (is_sent_to_me(pkt)) {
    switch (IPH_PROTO(iphdr)) {
      case IP_PROTO_ICMP:
        t = icmp_input(pkt);
        _imem->free(pbuf_list, PBUF_ALLOCATOR, _index);
        break;

      case IP_PROTO_UDP:
        if (packet_completed == true) {
          t = udp_input(pbuf_list, queue);
        }
        else {
          if (t != KEEP_THIS_PACKET) {
            printf("Unwanted IP frag received...discard!!\n");
            t = REUSE_THIS_PACKET;
          }
        }
        break;

      default:
        t = PACKET_ERROR_UNKNOWN_IP_TYPE;
    };
  }

  return t;
}

struct ip_reassdata* Exo_stack::ip_reass_enqueue_new_datagram(struct ip_hdr *fraghdr, unsigned queue) {
  struct ip_reassdata* ipr;
  unsigned tid = queue >> 1;
  unsigned core = rx_core[tid];

  /* No matching previous fragment found, allocate a new reassdata struct */
  void * p;
  if (_imem->alloc((addr_t *)&p, IP_REASS_ALLOCATOR, _index, core) != Exokernel::S_OK) {
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

bool Exo_stack::ip_reass_chain_frag_into_datagram_and_validate(struct ip_reassdata *ipr, pbuf_t *new_p, pkt_status_t * t) {
  struct ip_reass_helper *iprh, *iprh_tmp, *iprh_prev=NULL;
  pbuf_t *q;
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
          _imem->free(new_p, PBUF_ALLOCATOR, _index);
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
      _imem->free(new_p, PBUF_ALLOCATOR, _index);
    } else if(iprh->start < iprh_tmp->end) {
      /* overlap: no need to keep the new datagram */
      printf("[Drop!]fragment overlaps!!!\n");
       *t=PACKET_ERROR_BAD_IP_FRAG_OVERLAP2;
      _imem->free(new_p, PBUF_ALLOCATOR, _index);
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
  unsigned tid = queue >> 1;
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
  _imem->free(ipr, IP_REASS_ALLOCATOR, _index);
}

pbuf_t * Exo_stack::ip_reass(pbuf_t *pbuf_pkt, pkt_status_t * t, unsigned queue) {
  pbuf_t *r;
  struct ip_hdr *fraghdr;
  struct ip_reassdata *ipr;
  struct ip_reass_helper *iprh;
  struct ip_reassdata *ipr_prev = NULL;
  uint16_t offset, len, pkt_len;
  int tid = queue >> 1;

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

    pbuf_t * temp=pbuf_pkt;
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

inline
uint16_t ip_checksum(void* vdata, size_t length) {
    // Cast the data pointer to one that can be indexed.
    char* data=(char*)vdata;

    // Initialise the accumulator.
    uint32_t acc=0xffff;

    // Handle complete 16-bit blocks.
    for (size_t i=0;i+1<length;i+=2) {
        uint16_t word;
        __builtin_memcpy(&word,data+i,2);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Handle any partial block at the end of the data.
    if (length&1) {
        uint16_t word=0;
        __builtin_memcpy(&word,data+length-1,1);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Return the checksum in network byte order.
    return htons(~acc);
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

  iphdr->_chksum = ip_checksum((void *)iphdr, len);
  /* The actual sending routine */
  unsigned tx_queue = 0;
  void * temp;
  if (_imem->alloc((addr_t *)&temp, MBUF_ALLOCATOR, _index, rx_core[0]) != Exokernel::S_OK) {
    panic("MBUF_ALLOCATOR failed!\n");
  }
  assert(temp);
  __builtin_memset(temp, 0, sizeof(struct exo_mbuf));

  addr_t pkt_p;
  pkt_p = _imem->get_phys_addr(pkt, PACKET_ALLOCATOR, _index);
  assert(pkt_p);

  struct exo_mbuf * mbuf = (struct exo_mbuf *)temp;
  mbuf->phys_addr = pkt_p;
  mbuf->virt_addr = (addr_t)pkt;
  mbuf->len = pkt_len;
  mbuf->flag = 0;
  mbuf->nb_segment = 1;

  size_t sent_num = 1;
  while (ethernet_output_simple(&mbuf, sent_num, tx_queue) != Exokernel::S_OK) {
    printf("%s: TX ring buffer is full!\n", __func__);
  };

  _imem->free(temp, MBUF_ALLOCATOR, _index);
}

void Exo_stack::udp_bind(ip_addr_t *ipaddr, uint16_t port) {
  struct udp_pcb *pcb;
  for (int i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
     void * temp;
     if (_imem->alloc((addr_t *)&temp, UDP_PCB_ALLOCATOR, _index, rx_core[i]) != Exokernel::S_OK) {
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

pkt_status_t Exo_stack::udp_input(pbuf_t *pbuf_list, unsigned queue) {

  struct ip_hdr *iphdr;
  struct udp_hdr *udphdr;
  pbuf_t * tmp;
  unsigned frag_number=0;
  //unsigned udp_size;
  unsigned tid = queue >> 1;
  unsigned core = rx_core[tid];

  for (tmp = pbuf_list; tmp != NULL; tmp = tmp->next) {
    frag_number++;
  }

  pbuf_list->total_frames = frag_number;

  iphdr = (struct ip_hdr *)(pbuf_list->pkt + SIZEOF_ETH_HDR);

  /** Source IP address of current_header */
  ip_addr_t current_iphdr_src;

  /* copy IP addresses to aligned ip_addr_t */
  ip_addr_copy(current_iphdr_src, iphdr->src);

  udphdr = (struct udp_hdr *)(pbuf_list->pkt + SIZEOF_ETH_HDR + IPH_HL(iphdr) * 4);
  //udp_size = ntohs(udphdr->len);

  uint16_t src;
  //uint16_t dest;

  /* convert src and dest ports to host byte order */
  src = ntohs(udphdr->src);
  //dest = ntohs(udphdr->dest);

  remote_port = src;

#if 0
  /* if this udp is not kvcache request, we discard it */
  if (dest != server_port) {
    PINF("Discard non-memcached protocol udp packets!!! (dest port %u)",dest);
    free_packets(pbuf_list, false);
    return t;
  }
#endif

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
#ifndef RAW_NIC_TEMP
    printf("FDIRMISS %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRMISS));
    printf("FDIRMATCH %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRMATCH));
    printf("FDIRUSTAT %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRUSTAT));
    printf("FDIRFSTAT %x\n",((Intel_x540_uddk_device *)_inic->driver(_index))->_mmio->mmio_read32(IXGBE_FDIRFSTAT));
#endif
#endif

#if 0
    printf("THIS UDP [%u]: TOTAL FRAGS %d and TOTAL SIZE %d\n",recv_counter[queue].ctr.pkt, frag_number,udp_size);  
    printf("[Queue %d]udp (%u.%u.%u.%u, %u) <-- (%u.%u.%u.%u, %u)\n", queue,
                   ip4_addr1_16(&iphdr->dest), ip4_addr2_16(&iphdr->dest),
                   ip4_addr3_16(&iphdr->dest), ip4_addr4_16(&iphdr->dest), ntohs(udphdr->dest),
                   ip4_addr1_16(&iphdr->src), ip4_addr2_16(&iphdr->src),
                   ip4_addr3_16(&iphdr->src), ip4_addr4_16(&iphdr->src), ntohs(udphdr->src));

    uint8_t * temp = (uint8_t *)udphdr+8;
    for (int i=0;i<44;i++) 
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

  if (recv_counter[queue].ctr.pkt == stats_num) {
    cpu_time_t finish_t = rdtsc();
        PLOG("[NIC %d CH %d @ CORE %d] RX THROUGHPUT %llu PPS (UDP)",
             _index,
             tid,
             core,
             (((unsigned long long)stats_num) * cpu_freq) / (finish_t - recv_counter[queue].ctr.timer));
    recv_counter[queue].ctr.pkt=0;
  }
#endif

  bool pkt_reuse;
  if (_msg_processor->process(pbuf_list, _index, tid, core, pkt_reuse) == S_OK) {
    if (pkt_reuse == true) {
      free_packets(pbuf_list, false);
      return REUSE_THIS_PACKET;
    }
    else {
      // packet has been pushed into the channel
      // free_packets(pbuf_list, true);
      return KEEP_THIS_PACKET;
    }
  }
  else {
    //printf("msg_processor returned error!!!\n");
    free_packets(pbuf_list, false);
    return REUSE_THIS_PACKET;
  }

  //free_packets(pbuf_list, false);
  //return KEEP_THIS_PACKET;
  //return REUSE_THIS_PACKET;
}

void Exo_stack::free_packets(pbuf_t* pbuf_list, bool flag) {
  assert(pbuf_list != NULL);
  assert(pbuf_list->pkt != NULL);
  pbuf_t* curr_pbuf=pbuf_list;
  pbuf_t* next_pbuf=curr_pbuf->next;

  while (curr_pbuf->next != NULL) {
    if (flag==true) {
      _imem->free(curr_pbuf->pkt, PACKET_ALLOCATOR, _index);
    }
    _imem->free(curr_pbuf, PBUF_ALLOCATOR, _index);
    curr_pbuf=next_pbuf;
    next_pbuf=curr_pbuf->next;
  }
  if (flag == true) {
    _imem->free(curr_pbuf->pkt, PACKET_ALLOCATOR, _index);
  }
  _imem->free(curr_pbuf, PBUF_ALLOCATOR, _index);
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
  if (_imem->alloc((addr_t *)&temp, PACKET_ALLOCATOR, _index, rx_core[0]) != Exokernel::S_OK) {
    panic("PACKET_ALLOCATOR failed!\n");
  }
  assert(temp);

  tmp_v=(addr_t)temp;
  tmp_p=(addr_t)_imem->get_phys_addr(temp, PACKET_ALLOCATOR, _index);
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
  if (_imem->alloc((addr_t *)&temp, MBUF_ALLOCATOR, _index, rx_core[0]) != Exokernel::S_OK) {
    panic("MBUF_ALLOCATOR failed!\n");
  }
  assert(temp);
  __builtin_memset(temp, 0, sizeof(struct exo_mbuf));
  
  addr_t pkt_p;
  pkt_p = _imem->get_phys_addr(pkt, PACKET_ALLOCATOR, _index);
  assert(pkt_p);

  struct exo_mbuf * mbuf = (struct exo_mbuf *)temp;
  mbuf->phys_addr = pkt_p;
  mbuf->virt_addr = (addr_t)pkt;
  mbuf->len = length;
  mbuf->nb_segment = 1;

  if (is_arp_reply == true)
    mbuf->flag = 0;
  else
    mbuf->flag = 1 | PACKET_ALLOCATOR << 4;

  size_t sent_num = 1;
  while(ethernet_output_simple(&mbuf, sent_num, tx_queue) != Exokernel::S_OK) {
    printf("%s: TX ring buffer is full!\n", __func__);
  };

  _imem->free(temp, MBUF_ALLOCATOR, _index);
}

void Exo_stack::udp_send_pkt(uint8_t *vaddr, 
                             addr_t paddr, 
                             unsigned udp_payload_len, 
                             unsigned queue,
                             bool recycle,
                             unsigned allocator_id) {
  struct ip_hdr *iphdr;
  struct udp_hdr *udphdr;
  struct eth_hdr *ethhdr;
  addr_t app_phys = paddr;
  uint8_t* app_virt = vaddr;
  uint32_t app_len;
  unsigned tid = queue >> 1;
  unsigned core = rx_core[tid];

  /* obtain IP identification number in a partitioned space */
  uint16_t ip_iden = ip_iden_counter[queue].ctr + ip_iden_base[queue];
  ip_iden_counter[queue].ctr++;
  ip_iden_counter[queue].ctr = ip_iden_counter[queue].ctr % ip_iden_space_per_queue;

  unsigned total_udp_len = udp_payload_len + UDP_HLEN;
  int remaining_bytes = total_udp_len;

  bool need_fragment;
  if (total_udp_len > MAX_IP_PAYLOAD) 
    need_fragment = true;

  bool first_ip = true;
  uint16_t curr_offset = 0;
  size_t cnt;

  /* calculate how many IP packets are needed */
  unsigned nb_ip_needed = 0;
  if (udp_payload_len <= MAX_IP_PAYLOAD - UDP_HLEN) 
    nb_ip_needed = 1;
  else { 
    unsigned remaining_payload = udp_payload_len - MAX_IP_PAYLOAD - UDP_HLEN;
    if (remaining_payload % MAX_IP_PAYLOAD == 0)
      nb_ip_needed = 1 + remaining_payload / MAX_IP_PAYLOAD;
    else
      nb_ip_needed = 1 + (remaining_payload / MAX_IP_PAYLOAD + 1);
  }
  assert(nb_ip_needed <= IXGBE_TX_MAX_BURST);

  /* driver recycle does not support IP fragment. Large object recycle should be handled by application itself */
  if (recycle == true)
    assert(nb_ip_needed == 1);

  unsigned pkt_index = 0;
  // main loop for IP fragmentation
  while (remaining_bytes > 0) {
    void * temp;
    if (_imem->alloc((addr_t *)&temp, NET_HEADER_ALLOCATOR, _index, core) != Exokernel::S_OK) {
      panic("NET_HEADER_ALLOCATOR failed!\n");
    }
    assert(temp);

    uint8_t * network_hdr=(uint8_t *)temp;

    if (first_ip) {
      udphdr = (struct udp_hdr *)(network_hdr + SIZEOF_ETH_IP_HLEN);
      // populate UDP header
      udphdr->src=htons(udp_port);
      udphdr->dest=htons(remote_port);
      udphdr->len=htons(total_udp_len);
      // UDP checksum is used for flow direction on client side.
      udphdr->chksum = htons(ip_iden % client_rx_flow_num);
    }

    iphdr = (struct ip_hdr *)(network_hdr + SIZEOF_ETH_HDR);

    ip_addr_copy(iphdr->src,ip_addr);
    ip_addr_copy(iphdr->dest,remote_ip_addr);
    iphdr->_id=htons(ip_iden);
    iphdr->_ttl=0x3f; //TTL value
    iphdr->_chksum=0; //Hardware will do the IP checksum
    iphdr->_v_hl_tos=htons(0x4500); //ipv4 and ip header 20 bytes
    iphdr->_proto=0x11; //udp 

    if (remaining_bytes <= MAX_IP_PAYLOAD) {
      if (need_fragment == false) {
        iphdr->_offset=htons(0x4000);//no fragment
      } else {
        // last fragment
        iphdr->_offset=htons(curr_offset);
      }
      if (first_ip==true) {
        app_len = remaining_bytes - UDP_HLEN;
      }
      else {
        app_len = remaining_bytes;  
      }
      iphdr->_len=htons((uint16_t)(remaining_bytes + 20));
      remaining_bytes = 0;
    }
    else {
      remaining_bytes = remaining_bytes - MAX_IP_PAYLOAD;
      iphdr->_offset=htons(curr_offset|(1<<13)); // more fragments flag
      curr_offset += MAX_IP_PAYLOAD/8;
      if (first_ip==true) {
        app_len = MAX_IP_PAYLOAD - UDP_HLEN;
      }
      else {
        app_len = MAX_IP_PAYLOAD;
      }
      iphdr->_len=htons((uint16_t)(MAX_IP_PAYLOAD + 20));
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
    if (first_ip == false)
      net_hdr_len = NETWORK_HDR_SIZE - UDP_HLEN;

    addr_t pkt_p;
    pkt_p = _imem->get_phys_addr(network_hdr, NET_HEADER_ALLOCATOR, _index);
    assert(pkt_p);

    // preparing the packet in two segments
    __builtin_memset(mbuf[tid][pkt_index], 0, sizeof(struct exo_mbuf));

    //first segment is network header
    mbuf[tid][pkt_index]->phys_addr = pkt_p;
    mbuf[tid][pkt_index]->virt_addr = (addr_t)network_hdr;
    mbuf[tid][pkt_index]->len = net_hdr_len;
    mbuf[tid][pkt_index]->flag = 1 | NET_HEADER_ALLOCATOR << 4;
    mbuf[tid][pkt_index]->nb_segment = 2;

    //second segment is UDP paylaod
    mbuf[tid][pkt_index]->phys_addr_seg[0] = app_phys;
    mbuf[tid][pkt_index]->virt_addr_seg[0] = (addr_t)app_virt;
    mbuf[tid][pkt_index]->seg_len[0] = app_len; 

    if (recycle == true)
      mbuf[tid][pkt_index]->seg_flag[0] = 1 | allocator_id << 4;
    else
      mbuf[tid][pkt_index]->seg_flag[0] = 0;

    // preparing the packet in one segment
#if 0
    __builtin_memcpy(app_virt, network_hdr, 42);
    _imem->free(network_hdr, NET_HEADER_ALLOCATOR, _index);

    mbuf[pkt_index]->phys_addr = app_phys;
    mbuf[pkt_index]->virt_addr = (addr_t)app_virt;
    mbuf[pkt_index]->len = 42 + app_len;
    mbuf[pkt_index]->ref_cnt = 0;
    mbuf[pkt_index]->id = queue;
    mbuf[pkt_index]->nb_segment = 1;

    if (recycle == true)
      mbuf[pkt_index]->flag = 1 | allocator_id << 4;
    else
      mbuf[pkt_index]->seg_flag[0] = 0;
#endif

    app_phys += app_len;
    app_virt += app_len;
    first_ip = false;
    pkt_index ++;
  }

  unsigned sent_num = 0;
  while (sent_num < nb_ip_needed) {
    cnt = nb_ip_needed - sent_num;
    ethernet_output(&mbuf[tid][sent_num], cnt, queue);
    sent_num += cnt;
  }
}

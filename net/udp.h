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

#ifndef __MICRO_UDP_PROTOCOL_H__
#define __MICRO_UDP_PROTOCOL_H__

#include <arpa/inet.h>

#define NETWORK_HDR_SIZE     (42)

// Ethernet header
#define ETH_MTU             (1500)
#define ETHARP_HWADDR_LEN   (6)
#define SIZEOF_ETH_HDR      (14)
struct eth_addr {
  uint8_t addr[ETHARP_HWADDR_LEN];
};

struct eth_hdr {
  struct eth_addr dest;
  struct eth_addr src;
  uint16_t type;
} __attribute__((packed, aligned(8)));

#define ETHTYPE_ARP       (0x0806U)
#define ETHTYPE_IP        (0x0800U)

#define HWTYPE_ETHERNET       (1)
#define SIZEOF_ETHARP_HDR     (28)
#define SIZEOF_ETHARP_PACKET (SIZEOF_ETH_HDR + SIZEOF_ETHARP_HDR)

// ARP message types (opcodes)
#define ARP_REQUEST (1)
#define ARP_REPLY   (2)

struct ip_addr2 {
  uint16_t addrw[2];
};

struct ip_addr {
  uint32_t addr;
};

typedef struct ip_addr ip_addr_t;

/** the ARP message */
struct etharp_hdr {
  uint16_t hwtype;
  uint16_t proto;
  uint8_t  hwlen;
  uint8_t  protolen;
  uint16_t opcode;
  struct eth_addr shwaddr;
  struct ip_addr2 sipaddr;
  struct eth_addr dhwaddr;
  struct ip_addr2 dipaddr;
} __attribute__((packed, aligned(8)));

#define ARP_TABLE_SIZE  (10)

enum etharp_state {
  ETHARP_STATE_EMPTY = 0,
  ETHARP_STATE_PENDING,
  ETHARP_STATE_STABLE
};

struct etharp_entry {
  ip_addr_t ipaddr;
  struct eth_addr ethaddr;
  uint8_t state;
};

#define ETHADDR16_COPY(dst, src)  SMEMCPY(dst, src, ETHARP_HWADDR_LEN)
#define IPADDR2_COPY(dest, src) SMEMCPY(dest, src, sizeof(ip_addr_t))
#define SMEMCPY(dst,src,len)            __builtin_memcpy(dst,src,len)
#define ip_addr_cmp(addr1, addr2) ((addr1)->addr == (addr2)->addr)

/** Copies IP address - faster than ip_addr_set: no NULL check */
#define ip_addr_copy(dest, src) ((dest).addr = (src).addr)

/** Safely copies one IP address to another (src may be NULL). */
#define ip_addr_set(dest, src) ((dest)->addr = ((src) == NULL ? 0 : (src)->addr))


#define IP_RF 0x8000U       //>! Reserved fragment flag. 
#define IP_DF 0x4000U       //>! Do not fragment flag.
#define IP_MF 0x2000U       //>! More fragments flag.
#define IP_OFFMASK 0x1fffU  //>! Mask for fragmenting bits.

struct ip_hdr {
  uint16_t _v_hl_tos; //>! Version, header length, and type of service.
  uint16_t _len;      //>! Total length.
  uint16_t _id;       //>! Identification.
  uint16_t _offset;   //>! Fragment offset field.
  uint8_t _ttl;       //>! Time to live.
  uint8_t _proto;     //>! Protocol.
  uint16_t _chksum;   //>! Checksum. 
  ip_addr_t src;      //>! Source and destination IP addresses.
  ip_addr_t dest;
} __attribute__((packed, aligned(8))); 


#define IPH_V(hdr)  (ntohs((hdr)->_v_hl_tos) >> 12)
#define IPH_HL(hdr) ((ntohs((hdr)->_v_hl_tos) >> 8) & 0x0f)
#define IPH_TOS(hdr) (ntohs((hdr)->_v_hl_tos) & 0xff)
#define IPH_LEN(hdr) ((hdr)->_len)
#define IPH_ID(hdr) ((hdr)->_id)
#define IPH_OFFSET(hdr) ((hdr)->_offset)
#define IPH_TTL(hdr) ((hdr)->_ttl)
#define IPH_PROTO(hdr) ((hdr)->_proto)
#define IPH_CHKSUM(hdr) ((hdr)->_chksum)

#define IPH_VHLTOS_SET(hdr, v, hl, tos) (hdr)->_v_hl_tos = (htons(((v) << 12) | ((hl) << 8) | (tos)))
#define IPH_LEN_SET(hdr, len) (hdr)->_len = (len)
#define IPH_ID_SET(hdr, id) (hdr)->_id = (id)
#define IPH_OFFSET_SET(hdr, off) (hdr)->_offset = (off)
#define IPH_TTL_SET(hdr, ttl) (hdr)->_ttl = (u8_t)(ttl)
#define IPH_PROTO_SET(hdr, proto) (hdr)->_proto = (u8_t)(proto)
#define IPH_CHKSUM_SET(hdr, chksum) (hdr)->_chksum = (chksum)

#define IP_HLEN (20)

#define IP_PROTO_ICMP    1
#define IP_PROTO_IGMP    2
#define IP_PROTO_UDP     17
#define IP_PROTO_TCP     6

#define ICMP_ECHO 8    /* echo */
#define ICMP_ER   0    /* echo reply */

struct icmp_echo_hdr {
  uint8_t type;
  uint8_t code;
  uint16_t chksum;
  uint16_t id;
  uint16_t seqno;
} __attribute__((packed, aligned(8)));

#define ICMPH_TYPE(hdr) ((hdr)->type)
#define ICMPH_CODE(hdr) ((hdr)->code)

/** Combines type and code to an u16_t */
#define ICMPH_TYPE_SET(hdr, t) ((hdr)->type = (t))
#define ICMPH_CODE_SET(hdr, c) ((hdr)->code = (c))

struct ip_reass_helper {
  struct pbuf *next_pbuf;
  uint16_t start;
  uint16_t end;
} __attribute__((packed, aligned(8)));

/** Node of a list of network frames. */
struct pbuf {
  struct pbuf* next;
  uint8_t* pkt; 
  int32_t ippayload_len; 
  int32_t iphdr_len;
  struct ip_reass_helper irh;

  /**
   * Pointer to the last frame of in the list (AKA the tail), part of an object.
   * This is needed by the eviction logic to concatenate and disassable chains of 
   * lists of frames (AKA pbufs) that need to be discarded.
   * The NIC driver's receive thread just need to set this to NULL.
   * The application layer's  eviction logic will set it appropriately. 
   * And the eviction logic in the NIC driver's trasmit thread will use it.
   */  
  struct pbuf* last;

  ///////////////////////////////////////////////////////////////////////////// 
  // Variables to support concurrent DELETE operations and eviction, 
  // as well as work stealing.
  ///////////////////////////////////////////////////////////////////////////// 

  /** 
   * Number of job-descriptors currently using this object 
   * in a response message. 
   */ 
  volatile int64_t ref_counter __attribute__((aligned(__SIZEOF_POINTER__)));  

  /** Indicates that the contained object was deleted by a thread. */
  uint8_t is_deleted;

  /** Indicates the total number of frames in the list. This field is only valid for the first Pbuf */
  unsigned total_frames;

}__attribute__((aligned(64)));

typedef struct pbuf pbuf_t; 


/** IP reassembly helper struct. */
struct ip_reassdata {
  struct ip_reassdata *next;
  struct pbuf *p;
  struct ip_hdr iphdr;
  uint16_t datagram_len;
  uint8_t flags;
} __attribute__((aligned(64)));

#define IP_ADDRESSES_AND_ID_MATCH(iphdrA, iphdrB)  \
  (ip_addr_cmp(&(iphdrA)->src, &(iphdrB)->src) && \
   ip_addr_cmp(&(iphdrA)->dest, &(iphdrB)->dest) && \
   IPH_ID(iphdrA) == IPH_ID(iphdrB)) ? 1 : 0

#define IP_REASS_FLAG_LASTFRAG 0x01


/** UDP header. */
struct udp_hdr {
  uint16_t src;
  uint16_t dest;
  uint16_t len;
  uint16_t chksum;
} __attribute__((packed, aligned(8)));

#define UDP_HLEN (8)

// Get one byte from the 4-byte address.
#define ip4_addr1(ipaddr) (((uint8_t*)(ipaddr))[0])
#define ip4_addr2(ipaddr) (((uint8_t*)(ipaddr))[1])
#define ip4_addr3(ipaddr) (((uint8_t*)(ipaddr))[2])
#define ip4_addr4(ipaddr) (((uint8_t*)(ipaddr))[3])
#define ip4_addr1_16(ipaddr) ((uint16_t)ip4_addr1(ipaddr))
#define ip4_addr2_16(ipaddr) ((uint16_t)ip4_addr2(ipaddr))
#define ip4_addr3_16(ipaddr) ((uint16_t)ip4_addr3(ipaddr))
#define ip4_addr4_16(ipaddr) ((uint16_t)ip4_addr4(ipaddr))

/**
 * This is the common part of all PCB types. It needs to be at the
 * beginning of a PCB type definition. It is located here so that
 * changes to this common part are made in one location instead of
 * having to change all PCB structs. 
 */
#define IP_PCB  \
  /* ip addresses in network byte order */ \
  ip_addr_t local_ip;     \
  ip_addr_t remote_ip;    \
   /* Socket options */   \
  uint8_t so_options;     \
   /* Type Of Service */  \
  uint8_t tos;            \
  /* Time To Live */      \
  uint8_t ttl             \


/** Common members of all PCB types. */
struct ip_pcb {
  IP_PCB;
};


struct udp_pcb {
  // Common members of all PCB types.
  IP_PCB;

  // Protocol specific PCB members.

  struct udp_pcb *next;

  uint8_t flags;

  /** Local port in host byte order. */
  uint16_t local_port;

  /** Remote port in host byte order. */
  uint16_t remote_port;
}__attribute__((aligned(64)));

typedef struct udp_pcb udp_pcb_t;


#define IP_DEFAULT_TTL           255
#define UDP_TTL                  (IP_DEFAULT_TTL)
#define UDP_FLAGS_CONNECTED      0x04U

#define SIZEOF_ETH_IP_HLEN       (SIZEOF_ETH_HDR + IP_HLEN)
#define MAX_IP_PAYLOAD           (ETH_MTU - IP_HLEN)

#endif

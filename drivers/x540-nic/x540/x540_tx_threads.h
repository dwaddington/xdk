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

#ifndef __TX_THREAD_H__
#define __TX_THREAD_H__

#include <network/memory_itf.h>
#include <network/nic_itf.h>
#include <libexo.h>
#include <netinet/in.h>
#include "../xml_config_parser.h"
#include "driver_config.h"
#include <common/cycles.h>

using namespace Exokernel;
using namespace std;
using namespace Component;

class Tx_thread : public Exokernel::Base_thread {

private:
  volatile bool      _start;
  unsigned           _affinity;  // tx thread cpu core id
  unsigned           _tid;       // global tx thread id [0,NUM_TX_THREADS)
  unsigned           _local_id;  // tx thread local id per NIC [0,NUM_TX_THREADS_PER_NIC)
  unsigned           _nic_index;       // NIC index
  unsigned           _tx_threads_per_nic;
  unsigned           _server_timestamp;
  unsigned           _stats_num;
  Config_params *    _params;
  Intel_x540_uddk_device * _nic;
  IMem * _mem;

private:

  /** 
   * Main entry point for the thread
   * 
   * @return 
   */
  void * entry(void *) {
    while(!_start) { usleep(1000); }

    PLOG("TX thread [global ID %u, local ID %u] on core %d started!!", _tid, _local_id, _affinity);

    /* send raw ETH packet */
    send_pkt_test();

    return NULL;
  } 

public:
  /** 
   * Constructor 
   * 
   * @param nic
   * @param mem
   * @param local_id
   * @param global_id
   * @param affinity 
   */
  Tx_thread(Intel_x540_uddk_device * nic,
            IMem * mem,
            unsigned local_id,
            unsigned global_id,
            unsigned affinity,
            Config_params * params) : Base_thread(NULL, affinity),
                                      _affinity(affinity),
		                      _tid(global_id),
                                      _local_id(local_id),
                                      _params(params)
  {
    _nic = nic;
    _mem = mem;
    _stats_num = _params->stats_num;
    _tx_threads_per_nic = _params->channels_per_nic;
    _server_timestamp = _params->server_timestamp;

    _start=false;
    _nic_index = global_id / _tx_threads_per_nic;
    start();
  }

  void activate() {
    _start = true;
  }

  ~Tx_thread() {
    exit_thread();
  }

  void send_pkt_test() {

    enum { PACKET_LEN = 64 };
  
    unsigned char packet[PACKET_LEN] = {
          0xa0, 0x36, 0x9f, 0x46, 0xe1, 0x9c,  // dest mac
          0xa0, 0x36, 0x9f, 0x1d, 0x65, 0xce,  // src mac
          0x08, 0x00, // type: ip datagram
          0x45, 0x00, // version
          0x00, 50, // length
          0x00, 0x05, 0x40, 0x00, 0x3f, 0x11, 0x27, 0xb4,//ip header with checksum
          //0x00, 0x05, 0x40, 0x00, 0x3f, 0x11, 0x00, 0x00,//ip header without checksum
          0x0a, 0x00, 0x00, 33, // src: 10.0.0.33
          0x0a, 0x00, 0x00, 6, // dst: 10.0.0.6
          0xdd, 0xd5, 0x2b, 0xcb, 0x00, 30, 0x00, 0x00, // UDP header
          0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
          10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
          20, 21};
  
    size_t pkt_burst_size = 32;
    unsigned bytes_to_alloc = 1514;
    struct exo_mbuf * xmit_addr_pool[1024];
    unsigned i;
    addr_t pkt_v, pkt_p;

    for (i = 0; i < 1024; i++) {
      void * temp;
      if (_mem->alloc((addr_t *)&temp, PACKET_ALLOCATOR, _nic_index, _nic->rx_core[_local_id]) != Exokernel::S_OK) {
        panic("PACKET_ALLOCATOR failed!\n");
      }
      assert(temp);
      pkt_v = (addr_t)temp;
      __builtin_memset((void *)pkt_v, 0, bytes_to_alloc);
  
      pkt_p = _mem->get_phys_addr((void *)pkt_v, PACKET_ALLOCATOR, _nic_index);
  
      __builtin_memcpy((void *)pkt_v, (void *)packet, PACKET_LEN);
  
      xmit_addr_pool[i] = (struct exo_mbuf *)malloc(sizeof(struct exo_mbuf));
      __builtin_memset(xmit_addr_pool[i], 0, sizeof(struct exo_mbuf));
      xmit_addr_pool[i]->phys_addr = pkt_p;
      xmit_addr_pool[i]->virt_addr = pkt_v;
      xmit_addr_pool[i]->len = PACKET_LEN;
      xmit_addr_pool[i]->nb_segment = 1;
    }
  
    unsigned queue = 2*_local_id;
    unsigned counter = 0;
    cpu_time_t start_time=0;
    cpu_time_t finish_time=0;
    uint64_t cpu_freq = (uint64_t)(get_tsc_frequency_in_mhz() * 1000000); 

    while (1) {
      if (counter==0)
        start_time=rdtsc();
  
      unsigned sent_num = 0;
      size_t n;
      /* main TX function, return the actual packets sent out */
  
      if (pkt_burst_size<=IXGBE_TX_MAX_BURST) {
        //_nic->send(&xmit_addr_pool[counter % 512], n, queue);
        sent_num = _nic->send(&xmit_addr_pool[0], pkt_burst_size, queue);
      }
      else {
        PWRN("pkt_burst_size exceeds IXGBE_TX_MAX_BURST!");
      }
      counter += sent_num;

      if (counter >= _stats_num) {
        finish_time=rdtsc();
        unsigned long long pps = (((unsigned long long)counter) * cpu_freq) / (finish_time - start_time);
        double gbps = (pps * (PACKET_LEN+20) * 8 * 1.0) / (1000*1000*1000);
        printf("[TID %d] TX THROUGHPUT: %f MPPS or %f Gbps (pkt size = %u bytes)\n",
               _local_id,
               pps*1.0/1000000,  
               gbps,
               PACKET_LEN);
        counter = 0;
      }
    }
  }
};

#endif

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



#ifndef __TX_THREAD_H__
#define __TX_THREAD_H__

#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <libexo.h>
#include "./stack/exo_stack.h"
#include <netinet/in.h>
#include <numa_channel/shm_channel.h>
#include <x540/xml_config_parser.h>
#include <x540/driver_config.h>
#include <common/cycles.h>

using namespace Exokernel;

typedef Shm_channel<pbuf_t, CHANNEL_SIZE, SPMC_circular_buffer> Shm_channel_t;

class Tx_thread : public Exokernel::Base_thread {

private:
  volatile bool      _start;
  unsigned           _affinity;  // tx thread cpu core id
  unsigned           _tid;       // global tx thread id [0,NUM_TX_THREADS)
  unsigned           _local_id;  // tx thread local id per NIC [0,NUM_TX_THREADS_PER_NIC)
  unsigned           _nic;       // NIC index

  Exo_stack *        _stack;
  Shm_channel_t *    _net_side_channel;  

  unsigned           _tx_threads_per_nic;
  unsigned           _server_timestamp;
  unsigned           _stats_num;
  Config_params *    _params;

private:

  /** 
   * Main entry point for the thread
   * 
   * @return 
   */
  void * entry(void *) {
    uint64_t counter = 0;
    assert(_stack);

    while(!_start) { usleep(1000); }

    PLOG("TX thread %d on core %d started!!", _tid, _affinity);

    while (_stack->get_nic_component()->get_comp_state(_nic) < NIC_READY_STATE) { sleep(1); }

    /* Let TX thread 0 to send arp request */
    while ((_stack->get_entry_in_arp_table(_stack->get_remote_ip())) < 0) {
      if (_local_id == 0)
        _stack->arp_output(_stack->get_remote_ip());
      sleep(1);
    }

    cpu_time_t start_timer, finish_timer;
    pbuf_t* pbuf_list;

    uint64_t cpu_freq = (uint64_t)(get_tsc_frequency_in_mhz() * 1000000);

    static uint64_t channel_counter = 0;
    while (1) {
      //if (_local_id < 1)
      //_stack->send_udp_pkt_test(_local_id);
      //_stack->send_pkt_test(_local_id);
      //else (sleep(1000));

      while (_net_side_channel->consume(&pbuf_list) != E_SPMC_CIRBUFF_OK) {
        cpu_relax();
      }
      channel_counter++;
      
#ifndef RUN_TERACACHE
      assert(pbuf_list);
#define TX_ENABLE
#ifdef TX_ENABLE
      
      addr_t pkt_v = (addr_t)(pbuf_list->pkt);
      assert(pkt_v);
      addr_t pkt_p = _stack->get_mem_component()->get_phys_addr((void *)pkt_v, PACKET_ALLOCATOR, _nic);
      assert(pkt_p);
      size_t len = 86;//22; //packet size = 22 + 42(network header)
      unsigned queue = 2*_local_id+1;
      bool recycle = true;

      _stack->udp_send_pkt((uint8_t *)pkt_v, pkt_p, len, queue, recycle, PACKET_ALLOCATOR);

      _stack->free_packets((pbuf_t *)jd->protocol_frame, false);

#else

      _stack->free_packets((pbuf_t *)jd->protocol_frame, true);

#endif

#endif

      //printf("[TX %d] Sending out an item %lu (frame_num = %d), size = %lu\n",_tid, counter++, jd->num_frames, len);
      //uint8_t * temp = (uint8_t *)pkt_v;
      //for (int i=0;i<22;i++) 
      //{
      //  printf("[%d]%x ",i,(*temp)&0xff);  
      //  temp++;
      //}
      //printf("\n");

#if 1
      if (counter == 0)
        start_timer = rdtsc(); /* timer reset */

      /* increment packet count */
      counter++;

      if (counter == _stats_num) {
        finish_timer = rdtsc();
        PLOG("[NIC %d CH %d @ CORE %d] TX THROUGHPUT %llu PPS (CHANNEL)",
                 _nic,
                 _local_id,
                 _affinity,
                 (((unsigned long long)_stats_num) * cpu_freq) / (finish_timer - start_timer));
        counter = 0;
      }
#endif

    }
  } 

public:
  /** 
   * Constructor : TODO add signal based termination
   * 
   * @param channel
   * @param stack
   * @param local_id
   * @param global_id
   * @param affinity 
   */
  Tx_thread(Shm_channel_t * channel,
	    Exo_stack * stack,
            unsigned local_id,
            unsigned global_id,
            unsigned affinity,
            Config_params * params) : Base_thread(NULL, affinity),
                                 _net_side_channel(channel),
				 _stack(stack),
                                 _local_id(local_id),
                                 _tid(global_id),
                                 _affinity(affinity),
                                 _params(params)
  {
    _stats_num = _params->stats_num;
    _tx_threads_per_nic = _params->channels_per_nic;
    _server_timestamp = _params->server_timestamp;

    _start=false;
    _nic = global_id / _tx_threads_per_nic;
    start();
  }

  void activate() {
    _start = true;
  }

  ~Tx_thread() {
    exit_thread();
  }
};

#endif

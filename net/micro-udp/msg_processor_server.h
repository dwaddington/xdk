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



#ifndef __EXO_MSG_PROCESSOR_SERVER_H__
#define __EXO_MSG_PROCESSOR_SERVER_H__

#include "../msg_processor.h"
#include "tx_threads.h"
#include <x540/xml_config_parser.h>

using namespace Exokernel;

namespace Exokernel
{

  class Msg_processor_server : public Exokernel::Msg_processor
  {
    Shm_channel_t ** _net_side_channel;
    unsigned _index;
    unsigned _eviction_threshold;
    unsigned _channel_size;
    unsigned _channels_per_nic;
    unsigned _request_rate;
    unsigned _eviction_period;
    unsigned _tx_threads_per_nic;
    unsigned _cpus_per_nic;
    unsigned _server_timestamp;
    std::string _tx_threads_cpu_mask;
    Config_params * _params;

  public:

    /////////////////////////////////////////////////////////////////////////////////////////
    //
    // CLASS CONSTRUCTOR FUNCTION
    //
    /////////////////////////////////////////////////////////////////////////////////////////


    Msg_processor_server(unsigned nic_index, 
                         IMem * imem, 
                         IStack * istack, 
                         Config_params * params) : Msg_processor(imem, istack) {
      _index = nic_index;

      /* populate parameters from config.xml */
      _params = params;
      _channel_size = _params->channel_size;
      _tx_threads_per_nic = _params->channels_per_nic;
      _tx_threads_cpu_mask = _params->tx_threads_cpu_mask;
      _cpus_per_nic = _params->cpus_per_nic;
      _eviction_period = _params->eviction_period;
      _request_rate = _params->request_rate;
      _channels_per_nic = _params->channels_per_nic;
      _tx_threads_cpu_mask = _params->tx_threads_cpu_mask;
      _server_timestamp = _params->server_timestamp;

      /* create channels */
      _net_side_channel = (Shm_channel_t **) malloc(_channels_per_nic * sizeof(Shm_channel_t*));

      for (unsigned i = 0; i < _channels_per_nic; i++) {
        _net_side_channel[i] = new Shm_channel_t(i + _index * _channels_per_nic, true, nic_index);
      }
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    //
    // MSG PROCESSOR MAIN FUNCTION
    //
    /////////////////////////////////////////////////////////////////////////////////////////


    status_t process(void * data, 
                     unsigned nic_id, 
                     unsigned tid, 
                     unsigned core, 
                     bool& pkt_reuse) {
      pbuf_t * pbuf_list = (pbuf_t *) data;
      unsigned frag_number = pbuf_list->total_frames;

      struct ip_hdr * iphdr = (struct ip_hdr *)(pbuf_list->pkt + SIZEOF_ETH_HDR);
      ip_addr_t src_ip;
      ip_addr_copy(src_ip, iphdr->src);
      struct udp_hdr *udphdr = (struct udp_hdr *)(pbuf_list->pkt + SIZEOF_ETH_IP_HLEN);
      uint16_t src_port = ntohs(udphdr->src);

      /* push the packet into channel */
      while (_net_side_channel[tid]->produce(pbuf_list) != E_SPMC_CIRBUFF_OK) {
        pkt_reuse = true;
        return E_FAIL;
      }

      pkt_reuse = false;
      return S_OK;
    }

    Shm_channel_t * get_channel(unsigned id) {
      return _net_side_channel[id];
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    //
    // CREATE TX THREADS
    //
    /////////////////////////////////////////////////////////////////////////////////////////

    void create_tx_threads(void * stack_ptr) {
      Tx_thread* tx_threads[_tx_threads_per_nic];
      unsigned tx_core[_tx_threads_per_nic];
      unsigned i;

      /* obtain NUMA cpu mask */
      struct bitmask * cpumask;
      cpumask = numa_allocate_cpumask();
      numa_node_to_cpus(_index, cpumask);

      for (i = 0; i < _tx_threads_per_nic; i++) {
        tx_core[i] = 0;
      }

      Cpu_bitset thr_aff_per_node(_tx_threads_cpu_mask);
      
      uint16_t pos = 0;
      unsigned n = 0;
      for (i = 0; i < _cpus_per_nic; i++) {
        if (thr_aff_per_node.test(pos)) {
          tx_core[n] = get_cpu_id(cpumask,i+1);
          n++;
        }
        pos++;
      }

      printf("TX Thread assigned core: ");
      for (i = 0; i < _tx_threads_per_nic; i++) {
        printf("%u ",tx_core[i]);
      }
      printf("\n");
    
      for(i = 0; i < _tx_threads_per_nic; i++) {
        unsigned global_id = i + _index * _tx_threads_per_nic;
        unsigned local_id  = i;
        unsigned core_id = tx_core[i];

        tx_threads[i]= new Tx_thread(_net_side_channel[i], 
                                     (Exo_stack*)stack_ptr, 
                                     local_id, 
                                     global_id, 
                                     core_id, 
                                     _params);

        PLOG("TX[%d] is running on core %u", global_id, core_id);
        assert(tx_threads[i]);
        tx_threads[i]->activate();
      }  
    }

  };

}

#endif

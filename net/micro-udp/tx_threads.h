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
#include "../../app/kvcache/job_desc.h"
#include <numa_channel/shm_channel.h>
#include <mcd_binprot.h>
#include <x540/evict_support.h>
#include <x540/xml_config_parser.h>
#include <x540/driver_config.h>

using namespace Exokernel;
using namespace KVcache;

typedef Shm_channel<job_descriptor_t, CHANNEL_SIZE, SPMC_circular_buffer> Shm_channel_t;

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

  //////////////////////////////////////////////////////////////////////////////
  // Variables for eviction support. 
  //////////////////////////////////////////////////////////////////////////////

  // TODO: Add support for periodic / proactive eviction.  
  // There will be a periodic garbage collector thread that will access the list
  // of pending frame lists. That periodic thread will also take care of
  // evicting items (with help of the application side), will check expiration
  // time, and will free pending frame lists in this list.  To minimally disturb
  // the Tx thread, we will implementen a double-buffer solution. 

  /** 
   * List of lists of frames (each being a key-value pair or object) that are
   * pending to be freed. 
   * This transmit thread should free them, but it could not do it earlier
   * because the reference counters of the lists of frames were not zero. That
   * means that the lists of frames were used to create a response message.  
   */
  List<Frames_list_node> __pending_frame_lists;

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

    job_descriptor_t * jd;
    cpu_time_t start_timer, finish_timer;

    uint64_t cpu_freq = (uint64_t)(get_tsc_frequency_in_mhz() * 1000000);

    static uint64_t channel_counter = 0;
    while (1) {
      //if (_local_id < 1)
      //_stack->send_udp_pkt_test(_local_id);
      //_stack->send_pkt_test(_local_id);
      //else (sleep(1000));

      while (_net_side_channel->consume(&jd) != E_SPMC_CIRBUFF_OK) {
        cpu_relax();
      }
      channel_counter++;
      
      pbuf_t* pbuf_list=(pbuf*) jd->protocol_frame;

#ifdef RUN_TERACACHE
      /////////////////////////////////////////////////////////////////////////
      // Processing Response 
      /////////////////////////////////////////////////////////////////////////

      assert(jd->status >= 0);

      if (_server_timestamp > 1)
        jd->ts_array[5] = rdtsc(); // timestamp after fetching from tx channel

      if (jd->command == GET || jd->command == GET_K) {
        if (jd->status == NO_ERROR.code) {
          assert(pbuf_list);
          uint8_t* pkt = pbuf_list->pkt;
          assert(pkt);
        }
        else {
          assert(pbuf_list == NULL);
        }

        /* recycle the original request frame */
        pbuf_t* discard_pbuf_list_0 = (pbuf_t*) jd->protocol_frame_discard_0;
        assert(discard_pbuf_list_0);

        pbuf_t* discard_pbuf_list_1 = (pbuf_t*) jd->protocol_frame_discard_1;
        assert(discard_pbuf_list_1 == NULL);
      }
      else if (jd->command == SET ||
               jd->command == ADD ||
               jd->command == REPLACE) {
        assert(pbuf_list == NULL);
        pbuf_t* discard_pbuf_list_1 = (pbuf_t*) jd->protocol_frame_discard_1;
        assert(discard_pbuf_list_1 == NULL);
      }
      else if (jd->command == DELETE) {
         assert(pbuf_list == NULL);
      }
      else {
        assert(pbuf_list == NULL);
      }

      /////////////////////////////////////////////////////////////////////////
      // Eviction and Recycle Logic 
      /////////////////////////////////////////////////////////////////////////

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
              if (_stack->get_mem_component()->alloc((addr_t *)&temp, FRAME_LIST_ALLOCATOR, 
                                                     _nic, _stack->get_rx_core(_local_id)) != Exokernel::S_OK) {
                panic("FRAME_LIST_ALLOCATOR failed!\n");
              }
              assert(temp);

              Frames_list_node* n = (Frames_list_node*) temp;
              n->frame_list = head;
              __pending_frame_lists.insert(n);

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

        // Iterate over the list of pending frame list to see if they can be freed.
        Frames_list_node* cur = __pending_frame_lists.head();
        Frames_list_node* next = NULL;
        while (cur != NULL) {
          next = cur->next();

          pbuf_t* frame_list = cur->frame_list;
          assert(frame_list != NULL);

          if (frame_list->ref_counter <= 0) {
            //PLOG("tid = %d: BEFORE freeing frame_list ", _tid);
            _stack->free_packets(frame_list, true);
            //PLOG("tid = %d: AFTER freeing frame_list ", _tid);
            cur->frame_list = NULL;

            __pending_frame_lists.remove(cur);
            _stack->get_mem_component()->free(cur, FRAME_LIST_ALLOCATOR, _nic);
          }
          else {
            // Do nothing.
          }

          cur = next;
        }
        // PLOG("tid = %d: AFTER CLEANING pending list", _tid);
      }
      else {
        /* GET and GETK command only */
        pbuf_t* discard_pbuf_list_0 = (pbuf_t*) jd->protocol_frame_discard_0;
        pbuf_t* discard_pbuf_list_1 = (pbuf_t*) jd->protocol_frame_discard_1;
        assert(discard_pbuf_list_0 != NULL);
        assert(discard_pbuf_list_1 == NULL);

        assert((jd->command == GET) || (jd->command == GET_K));
        _stack->free_packets(discard_pbuf_list_0, true); // On success or error, free the received packet anyway.
        jd->protocol_frame_discard_0 = NULL;
      }

      /* to this point, all discarded frames should be already freed */
      assert(jd->protocol_frame_discard_0 == NULL);      
      assert(jd->protocol_frame_discard_1 == NULL);     

      /* send reply */
      unsigned queue = 2*_local_id+1;
      udp_send_frame_list(jd, queue); 
#endif
      
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

      /* recycle job descriptor */
      _stack->get_mem_component()->free(jd, JOB_DESC_ALLOCATOR, _nic);

    
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

  void udp_send_frame_list(job_descriptor_t * jd, unsigned queue) {
    pbuf_t * pbuf_list = (pbuf_t *)(jd->protocol_frame);
    void * temp;
    if (_stack->get_mem_component()->alloc((addr_t *)&temp, APP_HEADER_ALLOCATOR,
                                                     _nic, _stack->get_rx_core(_local_id)) != Exokernel::S_OK) {
      panic("APP_HEADER_ALLOCATOR failed!\n");
    }
    assert(temp);

    uint8_t * app_hdr_virt = (uint8_t *)temp;
    mcd_binprot_header_t* mem_hdr = (mcd_binprot_header_t*) (jd->app_hdr);

    __builtin_memcpy(app_hdr_virt, mem_hdr, APP_HDR_SIZE_WITH_TS);

    addr_t app_hdr_phys = _stack->get_mem_component()->get_phys_addr(app_hdr_virt, APP_HEADER_ALLOCATOR, _nic);   
    uint32_t app_hdr_len = APP_HDR_SIZE_WITHOUT_EXTRA + mem_hdr->extras_len;

    if (_server_timestamp > 0) {
      assert(app_hdr_len == APP_HDR_SIZE_WITH_TS);

      /* insert server latency into reply msg */
      cpu_time_t curr_t = rdtsc();
      uint64_t server_latency = (curr_t - jd->ts_array[0]);
      __builtin_memcpy((uint8_t *)(app_hdr_virt+APP_HDR_SIZE),&server_latency,8);
    }

    if (pbuf_list == NULL) {
      /* sending non GET/GET_K SUCCESS response */
      if (_server_timestamp == 0)
        assert(app_hdr_len == APP_HDR_SIZE_WITHOUT_EXTRA);

      bool recycle = true;
      _stack->udp_send_pkt((uint8_t *)app_hdr_virt, app_hdr_phys, app_hdr_len, queue, recycle, APP_HEADER_ALLOCATOR);

    }
    else {
      /* sending GET/GET_K SUCCESS response */
      if (_server_timestamp == 0)
        assert((app_hdr_len == APP_HDR_SIZE_WITHOUT_EXTRA) || (app_hdr_len == APP_HDR_SIZE));

      unsigned udp_payload_len = ntohl(mem_hdr->total_body_len) + APP_HDR_SIZE_WITHOUT_EXTRA;
      
#if 0
      /* obj size sanity check */
      unsigned expected_udp_payload_len = 0;
      pbuf_t * tmp;
      for (tmp=pbuf_list; tmp!=NULL; tmp=tmp->next) {
        expected_udp_payload_len += tmp->ippayload_len;
      }
      expected_udp_payload_len -= UDP_HLEN;
      assert(udp_payload_len == expected_udp_payload_len);

      uint8_t * network_hdr=(uint8_t *)(pbuf_list->pkt);
      struct udp_hdr * udphdr = (struct udp_hdr *)(network_hdr + SIZEOF_ETH_HDR + IP_HLEN);
      assert((udp_payload_len + UDP_HLEN) == ntohs(udphdr->len));
#endif

      _stack->udp_send_get_reply((uint8_t *)app_hdr_virt, app_hdr_phys, app_hdr_len, pbuf_list, queue);
    }


  }
};

#endif

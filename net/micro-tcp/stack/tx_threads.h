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
  @author Won Jeon (won.jeon@samsung.com)
*/

#ifndef __TX_THREAD_H__
#define __TX_THREAD_H__

#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <libexo.h>
#include <x540/driver_config.h>
#include "exo_stack.h"
#include <netinet/in.h>

using namespace Exokernel;

class Tx_thread : public Exokernel::Base_thread {

private:
  volatile bool      _start;
  unsigned           _affinity;
  int                _tid;       // global tx thread id [0,NUM_TX_THREADS)
  int                _core_id;   // tx thread cpu core id
  int                _local_id;  // tx thread local id per NIC [0,NUM_TX_THREADS_PER_NIC)

  Exo_stack *        _stack;

private:

  /** 
   * Main entry point for the thread
   * 
   * @return 
   */
  void * entry(void *) {
    static uint64_t counter = 0;
    assert(_stack);

    while(!_start) { usleep(1000); }

    PLOG("TX thread %d on core %d started!!", _tid, _core_id);

    while (_stack->get_nic_component()->get_comp_state() < NIC_READY_STATE) { sleep(1); }

    unsigned count = 0;
#if 0
    /* Let TX thread 0 to send arp request */
    while ((_stack->get_entry_in_arp_table(_stack->get_remote_ip())) < 0) {
      if (_tid == 0)
        _stack->arp_output(_stack->get_remote_ip());
      sleep(1);
    }
#endif
		//_stack->tcp_server_test(_local_id);
		_stack->tcp_client_test(_local_id);

#if 0
    //TODO: get packet from APP and go through stack processing      
    struct pbuf * item;
    while (1) {
      //_stack->send_pkt_test(_local_id);
      //_stack->send_udp_pkt_test(_local_id);

      while (_stack->get_channel(_local_id)->consume(&item) != E_SPMC_CIRBUFF_OK) {
        cpu_relax();
      }
      printf("[TX %d] Sending out an item %ld\n",_tid, item->ref_counter);
    }
#endif
  } 

public:
  /** 
   * Constructor : TODO add signal based termination
   * 
   * @param stack
   * @param local_id
   * @param global_id
   * @param affinity 
   */
  Tx_thread(Exo_stack * stack,
            unsigned local_id,
            unsigned global_id,
            unsigned affinity) : _stack(stack),
                                 _local_id(local_id),
                                 _tid(global_id),
                                 _core_id(affinity),
                                 Base_thread(NULL, affinity)
  {
    _start=false;
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

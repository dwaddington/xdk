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

#ifndef __IRQ_THREAD_H__
#define __IRQ_THREAD_H__

#include <network/nic_itf.h>
#include "libexo.h"
#include "x540_device.h"
#include "../xml_config_parser.h"

using namespace Exokernel;

class Irq_thread : public Exokernel::Base_thread {

private:
  volatile bool             _start;
  unsigned                  _irq;
  unsigned                  _affinity;
  unsigned                  _tid;       // global rx thread id 
  unsigned                  _core_id;   // rx thread cpu core id
  unsigned                  _local_id;  // rx thread local id per NIC

  Intel_x540_uddk_device *  _dev;
  Config_params * _params;
  unsigned _rx_threads_per_nic;
private:

  /** 
   * Main entry point for the thread
   * 
   * 
   * @return 
   */
  void * entry(void *) {
    assert(_dev);

    while(!_start) { usleep(1000); }

    _dev->activate(_local_id);

    while (!_dev->all_setup_done) { usleep(1000); }

    while (!_dev->rx_start_ok) { usleep(1000); }

    //_dev->reg_info(); 

    PLOG("RX thread %d on core %d started!!", _tid, _core_id);

    if (_local_id == (_rx_threads_per_nic - 1)) {
      unsigned nic_idx = _tid / _rx_threads_per_nic;
      _dev->_inic->set_comp_state(NIC_READY_STATE, nic_idx);
    }

    while(1) {
      _dev->wait_for_msix_irq(_irq);
      //PINF("\n [Irq_thread %d on Core %d] GOT INTERRUPT !!!!",_tid,_core_id);
      _dev->interrupt_handler(_local_id);
    }

  }

public:
  /** 
   * Constructor : TODO add signal based termination
   * 
   * @param dev
   * @param local_id
   * @param global_id
   * @param affinity 
   * @param params 
   */
  Irq_thread(Intel_x540_uddk_device * dev,
             unsigned local_id,
             unsigned global_id,
             unsigned affinity,
             Config_params * params) : Base_thread(NULL, affinity),
                                  _tid(global_id),
                                  _core_id(affinity),
                                  _local_id(local_id),
			          _dev(dev),
                                  _params(params) {
    _irq = _dev->_irq[local_id];
    _start=false;
    _rx_threads_per_nic = _params->channels_per_nic;

    start();
  }

  void activate() {
    _start = true;
  }

  ~Irq_thread() {
    exit_thread();
  }

};

#endif

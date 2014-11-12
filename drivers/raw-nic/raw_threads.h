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






#ifndef __RAW_THREAD_H__
#define __RAW_THREAD_H__

#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include "libexo.h"
#include "driver_config.h"
#include "raw_device.h"

using namespace Exokernel;

class Raw_thread : public Exokernel::Base_thread {

	private:
		volatile bool             _start;
		unsigned                  _affinity;
		int                       _tid;       // global rx thread id [0,NUM_RX_THREADS)
		int                       _core_id;   // rx thread cpu core id
		int                       _local_id;  // rx thread local id per NIC [0,NUM_RX_THREADS_PER_NIC)

		Raw_device *              _dev;

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

			//_dev->activate(_local_id);

			//while (!_dev->all_setup_done) { usleep(1000); }

			while (!_dev->rx_start_ok) { usleep(1000); }

			PLOG("RX thread %d on core %d started!!", _tid, _core_id);
			unsigned count = 0;

			if (_local_id == 0) _dev->_inic->set_comp_state(NIC_READY_STATE, _dev->_index);

			while (_dev->_istack->get_comp_state(_dev->_index) < STACK_READY_STATE) {
			 	sleep(1); 
			}
				printf("ready!!!\n");

			while(1) {
				//recv packets from raw socket
				PINF("\n [Raw_thread %d on Core %d] Fetching Packets !!!!",_tid,_core_id);
				_dev->recv_handler(_local_id);
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
		 */
		Raw_thread(Raw_device * dev,
				unsigned local_id,
				unsigned global_id,
				unsigned affinity) : _dev(dev),
		_local_id(local_id),
		_tid(global_id),
		_core_id(affinity),
		Base_thread(NULL, affinity) {
			_start=false;
			start();
		}

		void activate() {
			_start = true;
		}

		~Raw_thread() {
			exit_thread();
		}

};

#endif

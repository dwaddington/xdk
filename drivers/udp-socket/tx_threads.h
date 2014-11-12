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
#define __Tx_THREAD_H__

#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include "libexo.h"
#include "udp_socket.h"
#include "driver_config.h"

using namespace Exokernel;

class Tx_thread : public Exokernel::Base_thread {

	private:
		volatile bool             _start;
		unsigned                  _affinity;
		int                       _tid;       // global rx thread id [0,NUM_RX_THREADS)
		int                       _core_id;   // rx thread cpu core id
		int                       _local_id;  // rx thread local id per NIC [0,NUM_RX_THREADS_PER_NIC)

		UDP_socket *              _dev;

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

			PLOG("TX thread %d on core %d started!!", _tid, _core_id);

			while(1) {
			//PINF("\n [Tx_thread %d on Core %d] Sending Packets !!!!",_tid,_core_id);
				_dev->send_handler(_local_id);
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
		Tx_thread(UDP_socket* dev,
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

		~Tx_thread() {
			exit_thread();
		}

};

#endif

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






#include <libexo.h>
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include "raw_device.h"
#include "driver_config.h"

using namespace Exokernel;

/** 
 * Interface INic implementation
 * 
 */
class INic_impl : public INic
{
  state_t _state;
  IStack * _stack;
  IMem * _mem;
  Raw_device *  _dev[NIC_NUM];

public:
  INic_impl() : _stack(NULL) {

    component_t t = NIC_COMPONENT;
    set_comp_type(t);

		for (unsigned i = 0; i < NIC_NUM; i++)
			set_comp_state(NIC_INIT_STATE, i);

    //signed nd = query_num_registered_devices();
    //printf("Got %d registered devices\n",nd);
  }

  status_t init(arg_t arg) {
    assert(arg);
    assert(_stack);
    assert(_mem);
    unsigned i;
	  nic_arg_t * nic_arg = (nic_arg_t *) arg;

    /* Initialize NIC driver */
    for (i = 0; i < nic_arg->num_nics; i++) {
      _dev[i] = new Raw_device(this, _stack, _mem, i);
    }
    
    for (i = 0; i < nic_arg->num_nics; i++) {
      _dev[i]->init_device((char *)ETH_IF, (char *)MAC_LOCAL, (char *)MAC_REMOTE, (char *)IP_1);
    }

		/*
    for (i = 0; i < nic_arg->num_nics; i++) {
      _dev[i]->wait_for_activate();
      PINF("NIC [%d] is fully activated OK!", i);
    }
		*/
  }

  status_t send_packets(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue) {
    cnt = _dev[device]->multi_send((struct exo_mbuf **)p, cnt, queue);
    if (cnt > 0)
      return Exokernel::S_OK;
    else
			return Exokernel::E_FAIL;
	}

	status_t send_packets_simple(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue) {
		cnt = _dev[device]->send((struct exo_mbuf **)p, cnt, queue);
		if (cnt > 0)
			return Exokernel::S_OK;
		else
			return Exokernel::E_FAIL;
	}

	/*
	 * Receive packets
	 **/

	status_t recv_packets(struct exo_mbuf ** rx_pkts, size_t& cnt, unsigned device, unsigned queue) {
		cnt = _dev[device]->recv(rx_pkts, cnt, queue);
		if (cnt > 0)
			return Exokernel::S_OK;
		else
			return Exokernel::E_FAIL;
	}

	status_t bind(interface_t itf) {
    assert(itf);
    Interface_base * itf_base = (Interface_base *)itf;
    switch (itf_base->get_comp_type()) {
      case STACK_COMPONENT:
           _stack = (IStack *)itf;
           break;
      case MEM_COMPONENT:
           _mem = (IMem *)itf;
           break;
      defaulf:
           printf("binding wrong component types!!!");
           assert(0);
    }

    return Exokernel::S_OK;
  }

  device_handle_t driver(unsigned device) {
    return (device_handle_t) _dev[device];
  }

  void run() {
    /* Start receiving packets */
    for (int i = 0; i < NIC_NUM; i++) {
      _dev[i]->rx_start_ok = true;
    }
    printf("Nic Component is up running...\n\n");

#if 0
//#define MAX_PKT_BURST 128  //defined in config file
		struct exo_mbuf **mbufs = (struct exo_mbuf **)calloc(MAX_PKT_BURST, sizeof(struct exo_mbuf *));

		int count = 0;
		while (1) {
			printf("count = %d\n", ++count);
			//allocate memory 
			//_mem
			unsigned char buffer[PKT_LEN] = { /*Fill up header for testing*/
				0x0, 0x1, 0x2, 0x3, 0x4, 0x5, //dst MAC
				0x6, 0x7, 0x8, 0x9, 0xa, 0xb, //src MAC
				0x08, 0x00, // type: ip datagram
				0x45, 0x00, // version
				0x00, 50,   // length
				0x00, 0x05, 0x40, 0x00, 0x3f, 0x11, 0x26, 0xee,//ip header
				0x0a, 0x00, 0x00, 100, // src: 10.0.0.100
				0x0a, 0x00, 0x00, 101, // dst: 10.0.0.101
				0xdd, 0xd5, 0x2b, 0xcb, 0x00, 30, 0x00, 0x00, // UDP header
			};

			//receive from dev 0 and queue 0  
			size_t rx_cnt=1;
			recv_packets(mbufs, rx_cnt, 0, 0);

			//send to upper layer, e.g., micro-UDP stack
			//SKIP this for now
			//_stack->receive_packets((pkt_buffer_t*)&mbuf, 10, 0/*port*/, 0/*device*/);

			//forward back to the sender
			printf("Forward back to the sender\n");
			size_t tx_cnt=rx_cnt;

			send_packets_simple((pkt_buffer_t*)mbufs, tx_cnt, 0, 0);

			assert(tx_cnt == rx_cnt);
			//free memory
			//if(tx_cnt < rx_cnt)
			//  free the memory of un-sent pkts

			sleep(2);
		}
#endif
	}

  status_t cpu_allocation(cpu_mask_t mask) {
    //TODO
    printf("%s Not implemented yet!\n",__func__);
    return Exokernel::S_OK;
  }

#if 0
  state_t query_state() {
    return get_comp_state(_index);
  }
#endif
};

/** 
 * Definition of the component
 * 
 */
class NicComponent : public Exokernel::Component_base,
                     public INic_impl
{
public:  
  DECLARE_COMPONENT_UUID(0x51a5efbb,0xa76b,0x47a8,0x9fb8,0xe3fe,0x757e,0x155b);

  void * query_interface(Exokernel::uuid_t& itf_uuid) {
    if(itf_uuid == INic::uuid()) {
      add_ref(); // implicitly add reference
      return (void *) static_cast<INic *>(this);
    }
    else 
      return NULL; // we don't support this interface
  }
};


extern "C" void * factory_createInstance()
{
  return static_cast<void*>(new NicComponent());
}

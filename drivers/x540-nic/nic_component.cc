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
#include "x540/x540_device.h"
#include "x540/xml_config_parser.h"

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
  Intel_x540_uddk_device **  _dev;
  Config_params * _params;
  unsigned _nic_num;

public:
  INic_impl() : _stack(NULL) {
    component_t t = NIC_COMPONENT;
    set_comp_type(t);
     
    for (unsigned i = 0; i < MAX_INSTANCE; i++)
      set_comp_state(NIC_INIT_STATE, i);

    signed nd = query_num_registered_devices();
    printf("Got %d registered devices\n", nd);
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

  status_t init(arg_t arg) {
    assert(arg);
    assert(_stack);
    assert(_mem);
    unsigned i;

    nic_arg_t * nic_arg = (nic_arg_t *) arg;
    _params = (Config_params *) (nic_arg->params);

    _nic_num = _params->nic_num;
    _dev = (Intel_x540_uddk_device **) malloc(_nic_num * sizeof(Intel_x540_uddk_device *));
    
    /* Initialize NIC driver */
    for (i = 0; i < _nic_num; i++) {
      _dev[i] = new Intel_x540_uddk_device(this, _stack, _mem, i, _params);
    }
    
    for (i = 0; i < _nic_num; i++) {
      _dev[i]->init_device();
    }

    for (i = 0; i < _nic_num; i++) {
      _dev[i]->wait_for_activate();
      PINF("NIC [%d] is fully activated OK!", i);
    }
  }

  void run() {
    /* Start receiving packets */
    for (int i = 0; i < _nic_num; i++) {
      _dev[i]->rx_start_ok = true;
    }
    printf("Nic Component is up running...\n");

  }

  status_t cpu_allocation(cpu_mask_t mask) {
    //TODO
    printf("%s Not implemented yet!\n",__func__);
    return Exokernel::S_OK;
  }

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

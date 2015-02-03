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

#include "nic_component.h"

using namespace Exokernel;
using namespace Component;

/** 
 * Interface INic implementation
 * 
 */
INic_impl::INic_impl() {
  _stack = NULL;
  component_t t = NIC_COMPONENT;
  set_comp_type(t);
     
  for (unsigned i = 0; i < MAX_INSTANCE; i++)
    set_comp_state(NIC_INIT_STATE, i);

  signed nd = query_num_registered_devices();
  printf("Got %d registered devices\n", nd);
}

status_t 
INic_impl::send_packets(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue) {
    cnt = _dev[device]->multi_send((struct exo_mbuf **)p, cnt, queue);
    if (cnt > 0)
      return Exokernel::S_OK;
    else
      return Exokernel::E_FAIL;
}

status_t 
INic_impl::send_packets_simple(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue) {
    cnt = _dev[device]->send((struct exo_mbuf **)p, cnt, queue);
    if (cnt > 0)
      return Exokernel::S_OK;
    else
      return Exokernel::E_FAIL;
}

status_t 
INic_impl::bind(interface_t itf) {
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

device_handle_t 
INic_impl::driver(unsigned device) {
    return (device_handle_t) _dev[device];
}

status_t 
INic_impl::init(arg_t arg) {
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

void 
INic_impl::run() {
    /* Start receiving packets */
    for (int i = 0; i < _nic_num; i++) {
      _dev[i]->rx_start_ok = true;
    }
    printf("Nic Component is up running...\n");

}

status_t 
INic_impl::cpu_allocation(cpu_mask_t mask) {
    //TODO
    printf("%s Not implemented yet!\n",__func__);
    return Exokernel::S_OK;
}

extern "C" void * factory_createInstance(Component::uuid_t& component_id)
{
  if(component_id == NicComponent::component_id()) {
    printf("Creating 'NicComponent' component.\n");
    return static_cast<void*>(new NicComponent());
  }
  else return NULL;
}

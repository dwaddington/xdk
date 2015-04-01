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

// ctor
Component::NicComponent::NicComponent() {
  for (unsigned i = 0; i < MAX_NIC_INSTANCE; i++)
    set_comp_state(NIC_INIT_STATE, i);

  signed nd = query_num_registered_devices();
  printf("Got %d registered devices\n", nd);
}

// dtor
Component::NicComponent::~NicComponent()
{
}

status_t 
Component::NicComponent::send_packets(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue) {
  cnt = _dev[device]->multi_send((struct exo_mbuf **)p, cnt, queue);
  if (cnt > 0)
    return Exokernel::S_OK;
  else
    return Exokernel::E_FAIL;
}

status_t 
Component::NicComponent::send_packets_simple(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue) {
  cnt = _dev[device]->send((struct exo_mbuf **)p, cnt, queue);
  if (cnt > 0)
    return Exokernel::S_OK;
  else
    return Exokernel::E_FAIL;
}

int 
Component::NicComponent::bind(IBase * component) {
  assert(component);
  IMem * mem_itf = (IMem *) component->query_interface(Component::IMem::iid());
  if (mem_itf != NULL) {
    _imem = mem_itf;
    printf("binding IMem to NicComponent!\n");
    return 0;
  }
  return -1;
}

device_handle_t 
Component::NicComponent::driver(unsigned device) {
  return (device_handle_t) _dev[device];
}

status_t 
Component::NicComponent::init(arg_t arg) {
  assert(arg);
  assert(_imem);
  unsigned i;

  nic_arg_t * nic_arg = (nic_arg_t *) arg;
  _params = (Config_params *) (nic_arg->params);

  _nic_num = _params->nic_num;

  _dev = (Intel_x540_uddk_device **) malloc(_nic_num * sizeof(Intel_x540_uddk_device *));
  
  INic * inic = (INic *) this->query_interface(Component::INic::iid());

  /* Initialize NIC driver */
  for (i = 0; i < _nic_num; i++) {
    _dev[i] = new Intel_x540_uddk_device(inic, _imem, i, _params);
  }
  
  for (i = 0; i < _nic_num; i++) {
    _dev[i]->init_device();
  }

  for (i = 0; i < _nic_num; i++) {
    _dev[i]->wait_for_activate();
    PINF("NIC [%d] is fully activated OK!", i);
  }
  return Exokernel::S_OK;
}

void 
Component::NicComponent::run() {
  /* Start receiving packets */
  for (unsigned i = 0; i < _nic_num; i++) {
    _dev[i]->rx_start_ok = true;
  }
  printf("Nic Component is up running...\n");
}

status_t 
Component::NicComponent::cpu_allocation(cpu_mask_t mask) {
  printf("%s Not implemented yet!\n",__func__);
  return Exokernel::S_OK;
}

extern "C" void * factory_createInstance(Component::uuid_t& component_id) {
  if(component_id == NicComponent::component_id()) {
    printf("Creating 'NicComponent' component.\n");
    return static_cast<void*>(new NicComponent());
  }
  else return NULL;
}

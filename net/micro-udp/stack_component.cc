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

#include "stack_component.h"

/** 
 * Interface IStack implementation
 * 
 */
IStack_impl::IStack_impl() {
  _count = 0;
  component_t t = STACK_COMPONENT;
  set_comp_type(t);

  for (unsigned i = 0; i < MAX_INSTANCE; i++)
    set_comp_state(STACK_INIT_STATE, i);
}

status_t
IStack_impl::receive_packet(pkt_buffer_t p, size_t pktlen, unsigned device, unsigned queue) {
  status_t t = (status_t) _stack[device]->ethernet_input((uint8_t *)p, pktlen, queue);
  return t;
}

void 
IStack_impl::arp_output(ip_addr_t *ipdst_addr, unsigned device) {
  _stack[device]->arp_output(ipdst_addr);
} 

void 
IStack_impl::udp_send_pkt(uint8_t *vaddr, addr_t paddr, unsigned udp_payload_len,
                bool recycle, unsigned allocator_id, unsigned device, unsigned queue) {
    _stack[device]->udp_send_pkt(vaddr, 
                                 paddr, 
                                 udp_payload_len, 
                                 queue, 
                                 recycle, 
                                 allocator_id);
}

int 
IStack_impl::get_entry_in_arp_table(ip_addr_t* ip_addr, unsigned device) {
  return _stack[device]->get_entry_in_arp_table(ip_addr); 
}

void 
IStack_impl::udp_bind(ip_addr_t *ipaddr, uint16_t port, unsigned device) {
  _stack[device]->udp_bind(ipaddr, port);
}

status_t 
IStack_impl::bind(interface_t itf) {
  assert(itf);
  Interface_base * itf_base = (Interface_base *)itf;
  switch (itf_base->get_comp_type()) {
    case NIC_COMPONENT:
         _nic = (INic *)itf;
         break;
    case MEM_COMPONENT:
         _mem = (IMem *)itf;
         break;
    default:
         printf("binding wrong component types!!!");
         assert(0);
  }
  
  return Exokernel::S_OK;
}

status_t
IStack_impl::init(arg_t arg) {
  assert(arg);
  assert(_nic);
  assert(_mem);
  stack_arg_t * stack_arg = (stack_arg_t *) arg;

  _params = (Config_params *) (stack_arg->params);
  _nic_num = _params->nic_num;

  _stack = (Exo_stack **)malloc(_nic_num * sizeof(Exo_stack *));
  
  while (_mem->get_comp_state(0) < MEM_READY_STATE) { sleep(1); }

  for (unsigned i = 0; i < _nic_num; i++) {
    while (_nic->get_comp_state(i) < NIC_TX_DONE_STATE) { sleep(1); }

    /* Initialize msg_processor class based on server or client app */
    Msg_processor * msg = NULL;
    if (stack_arg->app == SERVER_APP) {
      msg = new Msg_processor_server(i, _mem, this, _params);
    }
    else 
      panic("wrong stack app type");

    /* Initialize micro UDP stack */
    _stack[i] = new Exo_stack(this, _nic, _mem, i, msg, _params);

    if (stack_arg->app == SERVER_APP) msg->create_tx_threads(_stack[i]);

    set_comp_state(STACK_READY_STATE, i);
  }
  return Exokernel::S_OK;
}

void
IStack_impl::run() {
  printf("Stack Component is up running...\n");
}  

status_t
IStack_impl::cpu_allocation(cpu_mask_t mask) {
  //TODO
  printf("%s Not implemented yet!\n",__func__);
  return Exokernel::S_OK;
}

extern "C" void * factory_createInstance(Component::uuid_t& component_id)
{
  if(component_id == StackComponent::component_id()) {
    printf("Creating 'StackComponent' component.\n");
    return static_cast<void*>(new StackComponent());
  }
  else return NULL;
}

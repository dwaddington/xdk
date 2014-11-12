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
#include <micro-udp/stack/exo_stack.h>
#include "msg_processor_server.h"
#include <x540/xml_config_parser.h>
#include "../../app/kvblaster/trafgen/trafgen_xml_config_parser.h"
#include "../../app/kvblaster/trafgen/msg_processor_client.h"

/** 
 * Interface IStack implementation
 * 
 */
class IStack_impl : public IStack
{
public:
  unsigned _count;
  INic * _nic;
  Exo_stack **  _stack;
  IMem * _mem;
  unsigned _nic_num;
  Config_params * _params;
	KVBlaster::Trafgen_config_params * _trafgen_params;

public:
  IStack_impl() : _count(0) {
    component_t t = STACK_COMPONENT;
    set_comp_type(t);

    for (unsigned i = 0; i < MAX_INSTANCE; i++)
      set_comp_state(STACK_INIT_STATE, i);
  }

  status_t receive_packet(pkt_buffer_t p, size_t pktlen, unsigned device, unsigned queue) {

    status_t t = (status_t) _stack[device]->ethernet_input((uint8_t *)p, pktlen, queue);

    return t;
  }

  void arp_output(ip_addr_t *ipdst_addr, unsigned device) {
    _stack[device]->arp_output(ipdst_addr);
  } 

  void udp_send_pkt(uint8_t *vaddr, addr_t paddr, unsigned udp_payload_len,
                bool recycle, unsigned allocator_id, unsigned device, unsigned queue) {
    _stack[device]->udp_send_pkt(vaddr, 
                                 paddr, 
                                 udp_payload_len, 
                                 queue, 
                                 recycle, 
                                 allocator_id);
  }

  int get_entry_in_arp_table(ip_addr_t* ip_addr, unsigned device) {
     _stack[device]->get_entry_in_arp_table(ip_addr);
  }

  void udp_bind(ip_addr_t *ipaddr, uint16_t port, unsigned device) {
    _stack[device]->udp_bind(ipaddr, port);
  }

  status_t bind(interface_t itf) {
    assert(itf);
    Interface_base * itf_base = (Interface_base *)itf;
    switch (itf_base->get_comp_type()) {
      case NIC_COMPONENT:
           _nic = (INic *)itf;
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

  status_t init(arg_t arg) {
    assert(arg);
    assert(_nic);
    assert(_mem);
    stack_arg_t * stack_arg = (stack_arg_t *) arg;

    _params = (Config_params *) (stack_arg->params);
		if (stack_arg->app == TRAFGEN) {
			_trafgen_params = (KVBlaster::Trafgen_config_params *) (stack_arg->app_params);
		}
    _nic_num = _params->nic_num;

    _stack = (Exo_stack **)malloc(_nic_num * sizeof(Exo_stack *));
    
    while (_mem->get_comp_state(0) < MEM_READY_STATE) { sleep(1); }

    for (unsigned i = 0; i < _nic_num; i++) {
      while (_nic->get_comp_state(i) < NIC_TX_DONE_STATE) { sleep(1); }

      /* Initialize msg_processor class based on server or client app */
      Msg_processor * msg;
      if (stack_arg->app == TERA_CACHE)
        msg = new Msg_processor_server(i, _mem, this, _params);
      else if (stack_arg->app == TRAFGEN) {
				KVBlaster::Trafgen_config_params trafgen_params;
				trafgen_params.client_id = _trafgen_params->key_prefix;;
				trafgen_params.key_prefix = _trafgen_params->key_prefix;
        msg = new Msg_processor_client(i, _mem, this, _params, _trafgen_params);
			}
      else 
				panic("wrong stack app type");

      /* Initialize micro UDP stack */
      _stack[i] = new Exo_stack(this, _nic, _mem, i, msg, _params);

      if (stack_arg->app == TERA_CACHE || stack_arg->app == TRAFGEN) msg->create_tx_threads(_stack[i]);

      set_comp_state(STACK_READY_STATE, i);
    }
  }

  void run() {
    printf("Stack Component is up running...\n");
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
class StackComponent : public Exokernel::Component_base,
                     public IStack_impl
{
public:  
  DECLARE_COMPONENT_UUID(0x51a5efbb,0xa76b,0x47a8,0x9fb8,0xe3fe,0x757e,0x155b);

  void * query_interface(Exokernel::uuid_t& itf_uuid) {
    if(itf_uuid == IStack::uuid()) {
      add_ref(); // implicitly add reference
      return (void *) static_cast<IStack *>(this);
    }
    else 
      return NULL; // we don't support this interface
  }
};


extern "C" void * factory_createInstance()
{
  return static_cast<void*>(new StackComponent());
}

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

#include <libexo.h>
#include <network/nic_itf.h>
#include <network/stack_itf.h>
#include <network/memory_itf.h>
#include <micro-udp/stack/exo_stack.h>
#include "msg_processor_server.h"
#include <x540/xml_config_parser.h>
#include <component/base.h>

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

public:
  IStack_impl();

  status_t receive_packet(pkt_buffer_t p, size_t pktlen, unsigned device, unsigned queue);

  void arp_output(ip_addr_t *ipdst_addr, unsigned device);

  void udp_send_pkt(uint8_t *vaddr, addr_t paddr, unsigned udp_payload_len,
                bool recycle, unsigned allocator_id, unsigned device, unsigned queue);

  int get_entry_in_arp_table(ip_addr_t* ip_addr, unsigned device);

  void udp_bind(ip_addr_t *ipaddr, uint16_t port, unsigned device);

  status_t bind(interface_t itf);

  status_t init(arg_t arg);

  void run();

  status_t cpu_allocation(cpu_mask_t mask);

};

/** 
 * Definition of the component
 * 
 */
class StackComponent : public Component::IBase,
                     public IStack_impl
{
public:  
  DECLARE_COMPONENT_UUID(0x51a5efbb,0xa76b,0x47a8,0x9fb8,0xe3,0xfe,0x75,0x7e,0x15,0x5b);

  void * query_interface(Component::uuid_t& itf_uuid) {
    if(itf_uuid == IStack::iid()) {
      add_ref(); // implicitly add reference
      return (void *) static_cast<IStack *>(this);
    }
    else 
      return NULL; // we don't support this interface
  }
};

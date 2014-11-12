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
#include "nic_itf.h"
#include "tcpip_itf.h"

/** 
 * Interface INic implementation
 * 
 */
class ITcpip_impl : public ITcpip
{
  unsigned _count;
  INic * _nic;

public:
  ITcpip_impl() : _count(0) {
  }

  status_t process_packet(void * p) {
    printf("TCPIP: Processing packet [%p] %u\n",(void*)p,_count++);
    // pass back to NIC component
    _nic->tx_packet(p);

    return Exokernel::S_OK;
  }

  status_t bind(INic * nic_itf) {
    assert(nic_itf);
    _nic = nic_itf; /* we should check the UUID to be safe */
    return Exokernel::S_OK;
  }
    
};

/** 
 * Definition of the component
 * 
 */
class TcpipComponent : public Exokernel::Component_base,
                     public ITcpip_impl
{
public:  
  DECLARE_COMPONENT_UUID(0x51a5efbb,0xa76b,0x47a8,0x9fb8,0xe3fe,0x757e,0x155b);

  void * query_interface(Exokernel::uuid_t& itf_uuid) {
    if(itf_uuid == ITcpip::uuid()) {
      add_ref(); // implicitly add reference
      return (void *) static_cast<ITcpip *>(this);
    }
    else 
      return NULL; // we don't support this interface
  }
};


extern "C" void * factory_createInstance()
{
  return static_cast<void*>(new TcpipComponent());
}

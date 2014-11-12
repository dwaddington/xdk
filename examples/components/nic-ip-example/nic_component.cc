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
class INic_impl : public INic
{
private:
  ITcpip * _tcpip;

public:
  INic_impl() : _tcpip(NULL) {
  }

  status_t tx_packet(void * p) {
    printf("NIC: Transmitting packet [%p]\n",p);
    return Exokernel::S_OK;
  }

  status_t bind(ITcpip * tcpip_itf) {
    assert(tcpip_itf);
    _tcpip = tcpip_itf; // should check UUID to be safe
    return Exokernel::S_OK;
  }

  void run() {
    assert(_tcpip);
    unsigned * packet = new unsigned[10];
    for(unsigned i=0;i<10;i++) {
      printf("--\nNIC: received (simulated) packet [%p]\n", (void*) &packet[i]);
      _tcpip->process_packet(&packet[i]);
    }
    delete [] packet;
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

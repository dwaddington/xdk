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

#include "x540/x540_device.h"
#include <libexo.h>
#include <network/nic_itf.h>
#include <network/memory_itf.h>
#include "xml_config_parser.h"
#include <component/base.h>

using namespace Exokernel;
using namespace Component;

namespace Component
{

  /** 
   * Definition of the component
   * 
   */
  class NicComponent : public Component::IBase,
                       public Component::INic
  {
  public:
    state_t _state;
    IMem * _imem;
    Intel_x540_uddk_device **  _dev;
    Config_params * _params;
    unsigned _nic_num; 
  
  public:  
    DECLARE_COMPONENT_UUID(0x51a5efbb,0xa76b,0x47a8,0x9fb8,0xe3,0xfe,0x75,0x7e,0x15,0x5b);
  
    /* interface selection */
    void * query_interface(Component::uuid_t& itf_uuid) {
      if(itf_uuid == INic::iid()) {
        return (void *) static_cast<Component::INic *>(this);
      }
      return NULL; // we don't support this interface
    }

    int bind(IBase * component);

    // ctor
    NicComponent();
    ~NicComponent();

    // INic interface
    status_t send_packets(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue);
    status_t send_packets_simple(pkt_buffer_t * p, size_t& cnt, unsigned device, unsigned queue);
    device_handle_t driver(unsigned device);
    status_t init(arg_t arg);
    void run();
    status_t cpu_allocation(cpu_mask_t mask);
  };
}

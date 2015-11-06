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
#include <network/memory_itf.h>
#include <network/nic_itf.h>
#include <exo/shm_table.h>
#include "xml_config_parser.h"
#include <component/base.h>

using namespace Component;

namespace Component
{ 
  /** 
   * Definition of the component
   * 
   */
  class MemComponent : public Component::IBase,
                       public Component::IMem
  {
  public:
    Exokernel::Memory::Numa_slab_allocator *** _allocator;
    INic * _inic;
    unsigned** _allocator_core;
    unsigned _nic_num;
    unsigned _rx_threads_per_nic;
    unsigned _cpus_per_nic;
    uint64_t _shm_max_size;
    std::string _rx_threads_cpu_mask;
    Config_params * _params;

  public:
    DECLARE_COMPONENT_UUID(0xe1ad1bc2,0x63c5,0x4011,0xb877,0xa0,0x18,0x89,0xae,0x46,0x3d);
  
    void * query_interface(Component::uuid_t& itf_uuid) {
      if(itf_uuid == IMem::iid()) {
        return (void *) static_cast<Component::IMem *>(this);
      }
      return NULL; // we don't support this interface
    }

    /* called when the component ref count == 0 */
    void unload() {
      delete this;
    }
  
    int bind(IBase * component);

    // ctor
    MemComponent();
    ~MemComponent();

    // IMem interface
    status_t init(arg_t arg);
    addr_t get_phys_addr(void *virt_addr, allocator_t id, unsigned device);
    uint64_t get_num_avail_per_core(allocator_t id, unsigned device, core_id_t core);
    uint64_t get_total_avail(allocator_t id, unsigned device);
    allocator_handle_t get_allocator(allocator_t id, unsigned device);
    status_t bind(interface_t itf);
    status_t alloc(addr_t *p, allocator_t id, unsigned device, core_id_t core);
    status_t free(void *p, allocator_t id, unsigned device);
    void run();
    void * alloc(size_t n);
    status_t free(void * p);
    status_t cpu_allocation(cpu_mask_t mask);

    DUMMY_IBASE_CONTROL;
  };
}


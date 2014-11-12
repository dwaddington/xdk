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
#include <network/memory_itf.h>
#include <network/stack_itf.h>
#include <network/nic_itf.h>
#include "raw_device.h"
#include "driver_config.h"
#include <exo/shm_table.h>
#include <global.h>

using namespace Exokernel;
using namespace Exokernel::Memory;

/** 
 * Interface IMem implementation
 * 
 */
class IMem_impl : public IMem
{
  Numa_slab_allocator ** _allocator[NIC_NUM];
  IStack * _stack;
  INic * _nic;
  unsigned _allocator_core[NUM_RX_THREADS_PER_NIC];

public:
  IMem_impl() {
    unsigned i;
    for (i = 0; i < NIC_NUM; i++)
      _allocator[i] = NULL;

    component_t t = MEM_COMPONENT;
    set_comp_type(t);
    set_comp_state(MEM_INIT_STATE, 0);

    for (i = 0; i < NUM_RX_THREADS_PER_NIC; i++) {
      _allocator_core[i] = 0;
    }

    /* derive cpus for threads from RX_THREAD_CPU_MASK */
    unsigned pos = 1;
    unsigned n = 0;
    for (i = 0; i < CPU_NUM; i++) {
      pos = 1 << i;
      if ((RX_THREAD_CPU_MASK & pos) != 0) {
        _allocator_core[n] = i;
        n++;
      }
    }
  }

  status_t init(arg_t arg) {
    assert(arg);
    mem_arg_t * p = (mem_arg_t *)arg;
    unsigned i, j;

    for (i = 0; i < NIC_NUM; i++) {
      _allocator[i] = (Numa_slab_allocator **) malloc(sizeof(Numa_slab_allocator *) * (p->num_allocators));
    }

    alloc_config_t ** config_list = p->config_list;

    bool master = true;
    Shm_table * shmem_table = new Shm_table(master, 0); // The first process is shmem_table master.
    shm_table_entry_t stt;

    huge_shmem_set_system_max(SHM_MAX_SIZE);
    huge_shmem_set_region_max(SHM_MAX_SIZE);

    shmem_table->lock();

    for (j = 0; j < NIC_NUM; j++) {    
      for (i = 0; i < p->num_allocators; i++) {
        void * space_v = NULL;
        size_t BLOCK_SIZE = config_list[i]->block_size;
        size_t NUM_BLOCKS = config_list[i]->num_blocks;
        unsigned alignment = config_list[i]->alignment; 
        allocator_t id = config_list[i]->allocator_id;

        //printf("before round up, total_size = %lu (%lu * %lu)\n",Fast_slab_allocator::actual_block_size(BLOCK_SIZE) * NUM_BLOCKS, Fast_slab_allocator::actual_block_size(BLOCK_SIZE), NUM_BLOCKS);
        size_t total_size = Fast_slab_allocator::actual_block_size(BLOCK_SIZE) * NUM_BLOCKS;

        //thread should use their local id (not gloabal id) to get the right allocator.
				Cpu_bitset cpu;
				if (id == DESC_ALLOCATOR) 
					cpu.set(j*CPU_NUM);
				else {
          for (unsigned pos = 0; pos < NUM_RX_THREADS_PER_NIC; pos ++) {
						cpu.set(_allocator_core[pos]+j*CPU_NUM);
          }
        }

        //huge page size minimal requirement
        if (total_size < MB(2)) {
          do {
            total_size = total_size * 2;
          } while (total_size < MB(2));
        }
       
        //printf("ready to allocate %lu bytes in shared memory\n",total_size); 

        key_t key = j * CPU_NUM + id + 100;
        int handle = -1;
        try {
          space_v = huge_shmem_alloc(key, total_size, j, &handle, NULL);
        }
        catch(Exokernel::Exception e) {
          printf("huge_shmem_alloc error: %s\n",e.cause());
        }
        assert(space_v);
        
        size_t per_core_block_quota;
        if (id == DESC_ALLOCATOR)
          per_core_block_quota = NUM_BLOCKS;
        else
          per_core_block_quota = NUM_BLOCKS/NUM_RX_THREADS_PER_NIC;

        //create numa_allocator object
        if ((id == DESC_ALLOCATOR) || (id == PACKET_ALLOCATOR) || (id == NET_HEADER_ALLOCATOR))
          _allocator[j][i] = new Numa_slab_allocator(space_v,
                                                total_size,
                                                per_core_block_quota,
                                                BLOCK_SIZE,
                                                cpu,
                                                alignment,
                                                true,        //need physical addr
                                                id, //debug id
                                                NULL);       //debug desc
        else
          _allocator[j][i] = new Numa_slab_allocator(space_v,
                                                total_size,
                                                per_core_block_quota,                                       
                                                BLOCK_SIZE,
                                                cpu,                                       
                                                alignment,
                                                false,       //no physical addr needed
                                                id, //debug id
                                                NULL);       //debug desc

        //populate shared memory table
        stt.type_id = SMT_MEMORY_AREA;
        stt.sub_type_id = (smt_sub_type_t)id;
        stt.value[0] = (uint64_t)space_v;
        stt.value[1] = (uint64_t)key;  
        stt.value[2] = (uint64_t)total_size;
        stt.value[3] = (uint64_t)j;

        unsigned row = shmem_table->get_row_number();
        shmem_table->write_shm_table(row, &stt);
      }
    }

    shmem_table->unlock();

    set_comp_state(MEM_READY_STATE, 0);  // Other components can start memory allocations now

    return Exokernel::S_OK;
  }

  addr_t get_phys_addr(void *virt_addr, allocator_t id, unsigned device) {
    assert(virt_addr);
    return _allocator[device][id]->get_phy_addr(virt_addr);
  }

  uint64_t get_num_avail_per_core(allocator_t id, unsigned device, core_id_t core) {
    return (uint64_t)(_allocator[device][id]->num_avail(core));
  }

  uint64_t get_total_avail(allocator_t id, unsigned device) {
    return (uint64_t)(_allocator[device][id]->num_total_avail());
  }

  status_t bind(interface_t itf) {
    assert(itf);
    Interface_base * itf_base = (Interface_base *)itf;
    switch (itf_base->get_comp_type()) {
      case STACK_COMPONENT:
           _stack = (IStack *)itf;
           break;
      case NIC_COMPONENT:
           _nic = (INic *)itf;
           break;
      defaulf:
           printf("binding wrong component types!!!");
           assert(0);
    }
    
    return Exokernel::S_OK;
  }

  status_t alloc(addr_t *p, allocator_t id, unsigned device, core_id_t core) {
    *p = (addr_t)_allocator[device][id]->alloc(core);
		if (*p == (addr_t) NULL) printf("%u %u %u\n", id, device, core);
    assert(*p);
    return Exokernel::S_OK;
  }
 
  status_t free(void *p, allocator_t id, unsigned device) {
		if (p == NULL) printf("id: %u, device: %u \n", id, device);
    assert(p);
    _allocator[device][id]->free(p);
    return Exokernel::S_OK;
  }

  void run() {
    printf("Memory Component is up running...\n");
  }

  void * alloc(size_t n) {
    //TODO
//    printf("%s Not implemented yet!\n",__func__);
    return NULL;
  }

  status_t free(void * p) {
    //TODO
    printf("%s Not implemented yet!\n",__func__);
    return Exokernel::S_OK;
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
class MemComponent : public Exokernel::Component_base,
                     public IMem_impl
{
public:  
  DECLARE_COMPONENT_UUID(0xe1ad1bc2,0x63c5,0x4011,0xb877,0xa018,0x89ae,0x463d);

  void * query_interface(Exokernel::uuid_t& itf_uuid) {
    if(itf_uuid == IMem::uuid()) {
      add_ref(); // implicitly add reference
      return (void *) static_cast<IMem *>(this);
    }
    else 
      return NULL; // we don't support this interface
  }
};


extern "C" void * factory_createInstance()
{
  return static_cast<void*>(new MemComponent());
}

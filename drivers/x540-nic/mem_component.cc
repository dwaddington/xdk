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

#include "mem_component.h"

using namespace Exokernel;
using namespace Exokernel::Memory;
using namespace Component;

// ctor
Component::MemComponent::MemComponent() {
  set_comp_state(MEM_INIT_STATE, 0);
}

// dtor 
Component::MemComponent::~MemComponent() {
  
}

status_t
Component::MemComponent::init(arg_t arg) {
  assert(arg);
  unsigned i, j;
  mem_arg_t * p = (mem_arg_t *)arg;
  _params = (Config_params *) (p->params);

  _nic_num = _params->nic_num;
  _rx_threads_per_nic = _params->channels_per_nic;
  _cpus_per_nic = _params->cpus_per_nic;
  _shm_max_size = GB(_params->shm_max_size);
  _rx_threads_cpu_mask = _params->rx_threads_cpu_mask;

  _allocator = (Numa_slab_allocator ***) malloc(_nic_num * sizeof(Numa_slab_allocator **));
  _allocator_core = (unsigned **) malloc(_nic_num * sizeof(unsigned *));

  for (i = 0; i < _nic_num; i++) {
    _allocator[i] = NULL;
    _allocator_core[i] = (unsigned *) malloc(_rx_threads_per_nic * sizeof(unsigned));
  } 

  //obtain NUMA cpu mask 
  struct bitmask * cpumask[_nic_num];

  for (i = 0; i < _nic_num; i++) {
    cpumask[i] = numa_allocate_cpumask();
    numa_node_to_cpus(i, cpumask[i]);
    printf("[NODE %u] CPU MASK %lx\n",i, *(cpumask[i]->maskp));
  }

  for (j = 0; j < _nic_num; j++) {
    for (i = 0; i < _rx_threads_per_nic; i++) {
      _allocator_core[j][i] = 0;
    }

    Cpu_bitset thr_aff_per_node(_rx_threads_cpu_mask);

    uint16_t pos = 0;
    unsigned n = 0;
    for (i = 0; i < _cpus_per_nic; i++) {
      if (thr_aff_per_node.test(pos)) {
        _allocator_core[j][n] = get_cpu_id(cpumask[j],i+1);
        n++;
      }
      pos++;
    }

    printf("Allocator assigned core: ");
    for (i = 0; i < _rx_threads_per_nic; i++) {
      printf("%d ",_allocator_core[j][i]);
    }
    printf("\n");
  }

  for (i = 0; i < _nic_num; i++) {
    _allocator[i] = (Numa_slab_allocator **) malloc(sizeof(Numa_slab_allocator *) * (p->num_allocators));
  }

  alloc_config_t ** config_list = p->config_list;

  bool master = true;
  Shm_table * shmem_table = new Shm_table(master, 0); // The first process is shmem_table master.
  shm_table_entry_t stt;

  huge_shmem_set_system_max(_shm_max_size);
  huge_shmem_set_region_max(_shm_max_size);

  shmem_table->lock();

  for (j = 0; j < _nic_num; j++) {    
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
      if (id == DESC_ALLOCATOR) {
        cpu.set(_allocator_core[j][0]);
      }
      else {
        for (unsigned pos = 0; pos < _rx_threads_per_nic; pos ++) {
          cpu.set(_allocator_core[j][pos]);
        }
      }

      //huge page size minimal requirement
      if (total_size < MB(2))
        total_size = MB(2);
      //printf("ready to allocate %lu bytes in shared memory\n",total_size); 

      key_t key = j * p->num_allocators + id + 100;
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
        per_core_block_quota = NUM_BLOCKS/_rx_threads_per_nic;

      //create numa_allocator object
      if ((id == DESC_ALLOCATOR) || (id == PACKET_ALLOCATOR) || (id == NET_HEADER_ALLOCATOR))
        _allocator[j][i] = new Numa_slab_allocator(space_v,
                                              total_size,
                                              per_core_block_quota,
                                              BLOCK_SIZE,
                                              cpu,
                                              alignment,
                                              true,                        //need physical addr
                                              id,                          //debug id
                                              slab_alloc_2_str(id));       //debug desc
      else
        _allocator[j][i] = new Numa_slab_allocator(space_v,
                                              total_size,
                                              per_core_block_quota,                                       
                                              BLOCK_SIZE,
                                              cpu,                                       
                                              alignment,
                                              false,                       //no physical addr needed
                                              id,                          //debug id
                                              slab_alloc_2_str(id));       //debug desc

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

addr_t
Component::MemComponent::get_phys_addr(void *virt_addr, allocator_t id, unsigned device) {
  assert(virt_addr);
  return _allocator[device][id]->get_phy_addr(virt_addr);
}

uint64_t 
Component::MemComponent::get_num_avail_per_core(allocator_t id, unsigned device, core_id_t core) {
  return (uint64_t)(_allocator[device][id]->num_avail(core));
}

uint64_t
Component::MemComponent::get_total_avail(allocator_t id, unsigned device) {
  return (uint64_t)(_allocator[device][id]->num_total_avail());
}

allocator_handle_t
Component::MemComponent::get_allocator(allocator_t id, unsigned device) {
    return (allocator_handle_t) _allocator[device][id];
}

int 
Component::MemComponent::bind(IBase * component) {
  assert(component);
  INic * nic_itf = (INic *)component->query_interface(Component::INic::iid());
  if (nic_itf != NULL) {
    _inic = nic_itf;
    printf("binding INic to MemComponent!\n");
    return 0;
  }
  return -1;
}

status_t 
Component::MemComponent::alloc(addr_t *p, allocator_t id, unsigned device, core_id_t core) {
  *p = (addr_t)_allocator[device][id]->alloc(core);
  //if (*p == (addr_t) NULL) printf("%u %u %u\n",id, device, core);
  assert(*p);
  return Exokernel::S_OK;
}
 
status_t 
Component::MemComponent::free(void *p, allocator_t id, unsigned device) {
  //if (p == NULL) printf("id: %u, device: %u \n", id, device);
  assert(p);
  _allocator[device][id]->free(p);
  return Exokernel::S_OK;
}

void 
Component::MemComponent::run() {
  printf("Memory Component is up running...\n");
}

void * 
Component::MemComponent::alloc(size_t n) {
  printf("%s Not implemented yet!\n",__func__);
  return NULL;
}

status_t 
Component::MemComponent::free(void * p) {
  printf("%s Not implemented yet!\n",__func__);
  return Exokernel::S_OK;
}

status_t 
Component::MemComponent::cpu_allocation(cpu_mask_t mask) {
  printf("%s Not implemented yet!\n",__func__);
  return Exokernel::S_OK;
}

extern "C" void * factory_createInstance(Component::uuid_t& component_id)
{
  if(component_id == MemComponent::component_id()) {
    printf("Creating 'MemComponent' component.\n");
    return static_cast<void*>(new MemComponent());
  }
  else return NULL;
}

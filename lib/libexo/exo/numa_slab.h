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
  Authors:
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
*/

#ifndef __NUMA_ALLOCATOR_H__
#define __NUMA_ALLOCATOR_H__

#include "../libexo.h"
#include "config.h"
#include "fast_slab.h"
#include <common/cpu_bitset.h>
#include "shm_table.h"

#include "numa.h"

#include <string>


#ifndef SUPPRESS_NOT_USED_WARN
#define SUPPRESS_NOT_USED_WARN __attribute__((unused))
#endif


namespace Exokernel {

  namespace Memory {

#if defined(__x86_64__)
    /** 
     * Per-NUMA-node slab allocator. 
     * It is a "partitioned" fixed block allocator which uses per-core local
     * partitions to reduce contention, but allows cross-core frees. 
     * You only need one instance of this class per NUMA node. 
     */
    class Numa_slab_allocator
    {
    private:

      Fast_slab_allocator* _per_cpu_allocs[EXOLIB_MAX_CPUS];
      size_t _block_size;
      unsigned _numa_id;
      unsigned _debug_id;

    protected:
      Cpu_bitset _cpu_mask;
      //      unsigned                _first_core_id;
      //      unsigned                _last_core_id;

    public:

      /** 
       * Constructor.
       * 
       * @param addr_v Starting virtual address.
       * @param total_size Size of the total allocated memory.
       * @param block_size Size of each block in bytes.
       * @param per_cpu_block_quota Maximum number of blocks a client can
       *                             allocate.
       * @param cpu_mask Bitset with CPUs for which to create allocator. The CPUs
       *                 must belong to the same NUMA node. 
       * @param alignment Alignment requirement in bytes
       * @param needs_phys_addr Flag to indicate whether physical address is
       *                        needed for each block
       * @param debug_id Debug identifier [optional]
       * @param desc Debug descriptor [optional]
       */
      Numa_slab_allocator(void* addr_v,
                          size_t total_size,
                          size_t per_cpu_block_quota,
                          size_t block_size,
                          Cpu_bitset& cpu_mask,
                          unsigned alignment = 64,
                          bool needs_phys_addr = false,
                          unsigned debug_id = 0,
                          const char* desc = NULL)
        : _block_size(block_size), 
          _debug_id(debug_id), 
          _cpu_mask(cpu_mask) {

        if (numa_num_configured_nodes() > EXOLIB_MAX_CPUS) {
          panic("NUMA_ALLOCATOR (this = %p, allocator debug_id = %u): " 
                "ERROR we don't support more than %d hardware threads.",
                (void*)this, _debug_id, EXOLIB_MAX_CPUS);
        }

        assert(cpu_mask.size() == EXOLIB_MAX_CPUS);

        // Make sure pointers are NULL by default.
        __builtin_memset(_per_cpu_allocs, 0, sizeof(_per_cpu_allocs));

        //size_t total_partitions = cpu_mask.count();
        //size_t s = total_partitions * block_size * per_core_block_quota;
        size_t s = total_size;
#if (__SIZEOF_SIZE_T__ == 4)
        printf("Setting up NUMA slab allocator [%s] (dbg=%u) (%u MB) " 
               "across cores:\n 0x%s \n",                
               (desc != NULL) ? desc : "Unspecified", 
               debug_id, REDUCE_MB(s), cpu_mask.to_string().c_str());
#else
        printf("Setting up NUMA slab allocator [%s] (dbg=%u) (%lu MB) " 
               "across cores:\n 0x%s \n",                
               (desc != NULL) ? desc : "Unspecified", 
               debug_id, REDUCE_MB(s), cpu_mask.to_string().c_str());
#endif

        // Check that the CPUs in the CPU mask all belong to the same valid node.
        int numa_id = -1;
        for (size_t cpu = 0; cpu < cpu_mask.size(); cpu++) {
          if (cpu_mask.test(cpu)) {
            int nid = numa_node_of_cpu(cpu);
            assert(nid >= 0); // Should not get an invalid CPU.
        
            if (numa_id < 0) {
              numa_id = nid;
            }  
        
            if (numa_id != nid) {
              panic("NUMA_ALLOCATOR (this = %p, allocator debug_id = %u): " 
                    "ERROR CPUs in the mask belong to different NUMA nodes.", (void*)this, _debug_id);
            }
          }
        }

        _numa_id = numa_id; 

        // Create fixed block allocators for each CPU position.
        void* space_v = addr_v;
        for (size_t cpu = 0; cpu < cpu_mask.size(); cpu++) {
          if (cpu_mask.test(cpu)) {
            PLOG("cpu: %lu, block_size: %lu, per_cpu_block_quota: %lu\n", 
                   cpu, block_size, per_cpu_block_quota);

            size_t per_cpu_total_size = 
              per_cpu_block_quota * Fast_slab_allocator::actual_block_size(block_size); 

            _per_cpu_allocs[cpu] = new Fast_slab_allocator(space_v,
                                                           per_cpu_total_size,
                                                           block_size,
                                                           alignment,
                                                           cpu,
                                                           needs_phys_addr,
                                                           debug_id);
            assert(_per_cpu_allocs[cpu] != NULL);

            space_v = (void*)((addr_t)space_v + per_cpu_total_size);
          }
        }
      }


      /** Destructor. */
      virtual ~Numa_slab_allocator() {
        for (size_t i = 0; i < EXOLIB_MAX_CPUS; i++) {
          if (_per_cpu_allocs[i] != NULL) {
            delete _per_cpu_allocs[i];
          }
        }
      }


      /** Returns the debug ID of allocator. */
      unsigned debug_id() const { return _debug_id; }      

      /** 
       * Allocates a block.
       * @param core_id Identifier of the core from which the thread is calling.  
       * @return Pointer to allocated block
       */
      INLINE void* alloc(core_id_t core_id) {
        //    if (core_id >= _cpu_mask.size() || !_cpu_mask.test(core_id)) {  
        //      panic("[%s] No NUMA slab allocator for core_id: "
        //            "debugid = %u core_id = %u!\n", __func__, debug_id(), core_id); 
        //    }

        assert(_per_cpu_allocs[core_id] != NULL);

        return _per_cpu_allocs[core_id]->alloc();
      }
  
      /** 
       * Frees a block of data.  This method is cross-thread safe.
       * @param p Pointer to the block to be freed.
       * @return S_OK on success.
       */
      INLINE status_t free(void* p) {

        assert(p != NULL);
        core_id_t core_id = Fast_slab_allocator::get_id_from_block(p);

        //    if (core_id >= _cpu_mask.size() || !_cpu_mask.test(core_id)) {  
        //      panic("[%s] No NUMA slab allocator for core_id: "
        //            "debugid = %u core_id = %u!\n", __func__, debug_id(), core_id); 
        //    }

        assert(_per_cpu_allocs[core_id] != NULL);

        return _per_cpu_allocs[core_id]->free(p);
      }

      /** 
       * Gets the physical address for a block.
       * @param p Pointer to the block.
       * @return Physical address of the block.
       */
      INLINE static addr_t get_phy_addr(void* p) {
        assert(p);
        return Fast_slab_allocator::get_block_phy_addr(p);
      }

      /** 
       * Retrieves the current number of blocks available from a given allocator
       * @param core Core number of the allocator
       * @return Number of free blocks in a given allocator. 
       *         It returns zero for a core that does not have an allocator.
       */
      INLINE int64_t num_avail(core_id_t core) const {
        return (_per_cpu_allocs[core] == NULL) ? 0 : _per_cpu_allocs[core]->num_avail();
      }

      /** 
       * Retrieves the current total number of blocks available from all the
       * internal allocators.
       * @return Number of free blocks.
       */
      INLINE int64_t num_total_avail() const {
        int64_t num_blocks = 0; 
        for (size_t cpu = 0; cpu < _cpu_mask.size(); cpu++) {
          if (_cpu_mask.test(cpu)) {
            num_blocks += _per_cpu_allocs[cpu]->num_avail();
          }
        }

        return num_blocks;
      }

      /** 
       * Searches for the first core id with an allocator with at least the specified number of
       * objects. 
       */
      int find_first_with_avail(core_id_t init_core_id, int min_num_avail) {

        // TODO: Reimplement.
        panic("%s(...) not implemented yet!", __PRETTY_FUNCTION__);

        assert(min_num_avail > 0);
 
        //// Randomize the section of the initial core id.   
        //uint32_t v = rdtsc_low();
        //init_core_id = v % (_last_core_id - _first_core_id) + _first_core_id;

        //assert(init_core_id >= _first_core_id && init_core_id <= _last_core_id);

        //for (core_id_t i = init_core_id; i <= _last_core_id; i++) {
        //  if (_per_cpu_allocs[i] == NULL) 
        //    continue;
        //  else if (this->num_avail(i) >= (int64_t) min_num_avail)
        //    return i;
        //}

        //for (core_id_t i = _first_core_id; i < init_core_id; i++) {
        //  if (_per_cpu_allocs[i] == NULL)
        //    continue;
        //  else if (this->num_avail(i) >= (int64_t) min_num_avail)
        //    return i;
        //}
      
        return -1; 
      }

    };

#endif

  }
}

#endif

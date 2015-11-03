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

#ifndef __EXO_MEMORY_H__
#define __EXO_MEMORY_H__

/** 
 * Note: to use this API you will need to allocate some huge pages in your system
 * see Linux kernel Documentation/vm/hugetlbpage.txt
 */

#include <common/types.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <map>
#include <sys/shm.h>

#ifdef NUMA_ENABLE
        #include <numa.h>
#else
        #include "common/xdk_numa_wrapper.h"
#endif


#ifndef SUPPRESS_NOT_USED_WARN
#define SUPPRESS_NOT_USED_WARN __attribute__((unused))
#endif

#define PAGE_SIZE (4096UL)
#define HUGE_PAGE_SIZE (2 * 1024 * 1024UL)
#define HUGE_MAGIC 0x0fabf00dUL

namespace Exokernel
{
  namespace Memory
  {
    template <typename T>
    class Typed_allocator_base
    {
    public:
      virtual T* alloc(size_t count) = 0;
      virtual void free(T * p) = 0;
    };


    template <typename T>
    class Typed_stdc_allocator : public Typed_allocator_base<T>
    {
    public:
      inline T * alloc(size_t count = 1) {
        return (T *) ::malloc(sizeof(T)*count);
      }
      inline void free(T * p) {
        ::free(p);
      }
    };    

    class Allocator_base
    {
    public:
      virtual void* alloc(size_t count, signed arg) = 0;
      virtual void free(void * p) = 0;
    };


    class Stdc_allocator : public Allocator_base
    {
    public:
      void * alloc(size_t count = 1, signed arg = -1) {
        return (void *) ::malloc(count);
      }
      void free(void * p) {
        ::free(p);
      }
    };    



    /** 
     * Align size to upper huge page (2MB).
     * @param s Size in bytes
     * @return Aligned size in bytes.
     */
    SUPPRESS_NOT_USED_WARN 
    static size_t align_to_huge_page(size_t s) {
      return (((size_t)s) % HUGE_PAGE_SIZE == 0) ? 
              s : (((((size_t)s) / HUGE_PAGE_SIZE)+1)*HUGE_PAGE_SIZE);
    }

    /** 
     * Aligns size to upper page (4K).
     * @param s Size in bytes
     * @return Aligned size in bytes
     */
    SUPPRESS_NOT_USED_WARN
    static size_t align_to_page(size_t s) {
      return (((size_t)s) % PAGE_SIZE == 0) ? 
             s : (((((size_t)s) / PAGE_SIZE)+1)*PAGE_SIZE);
    }


    struct huge_malloc_metadata_t {
      uint32_t magic;
      size_t size;
      byte _padding[10];
      char userdata[0];
    };

    /** 
     * Alloc an area of memory using huge pages (2MB)
     * 
     * @param size Size in bytes to allocate
     * @param phys_addr [out] Physical address of the allocation
     * 
     * @return Pointer to allocated memory
     */
    void * huge_malloc(size_t size);

    /** 
     * Free a huge page allocation
     * 
     * @param ptr Pointer to memory area originally allocated through huge_malloc
     * 
     * @return 
     */
    int huge_free(void * ptr);

    /** 
     * Set the system number of huge pages throuogh
     * /sys/devices/system/node/node[0-9]/\*
     * 
     * @param pages Number of huge pages
     * 
     * @return S_OK on success, error code otherwise.
     */
    status_t huge_system_configure_nrpages(size_t pages);


    /** 
     * Map an existing hugh page allocation into the calling process address space
     * 
     * @param size Size in bytes
     * @param phys Physical address of already allocated memory
     * 
     * @return Pointer to mapped virtual space
     */
    void * huge_mmap(size_t size, addr_t phys);
    
#if defined(__x86_64__)
    /** 
     * Allocate N 4K pages and return physical address
     * 
     * @param num_pages Number of pages to allocate
     * @param phys_addr [out] Physical address
     * 
     * @return Virtual address
     */
    void * alloc_page(addr_t * phys_addr);
#endif
    /** 
     * Free all pages associated with a previous alloc_pages call
     * 
     * @param ptr Virtual pointer previously returned by alloc_pages
     * 
     * @return 0 on success, -1 on error
     */
    int free_page(void * ptr);


    /* Note: You need to reserve huge pages in the system
       Kernel boot parameters are best to assure space.
       e.g., hugepagesz=2K hugepages=1024 (to reserve 2G)
    */

    /** 
     * Set the maximum size per huge shared memory region
     * 
     * @param size Max size in bytes
     */
    void huge_shmem_set_region_max(size_t size);

    /** 
     * Set the total system maximum allocation for huge shared memory
     * 
     * @param size 
     */
    void huge_shmem_set_system_max(size_t size);

    /** 
     * Allocate shared memory region in huge page area
     * 
     * @param key Unique key identifier to associate with memory
     * @param size Size of memory region in bytes
     * @param numa_node NUMA node to allocate region from
     * @param handle [out] Handle for the region
     * @param shmaddr_hint Hint for virtual address
     * 
     * @return 
     */
    void * huge_shmem_alloc(key_t key, size_t size, unsigned numa_node, int * handle, void * shmaddr_hint = NULL);

    /** 
     * Allocate shared memory region in huge page area
     * 
     * @param key Unique key identifier to associate with memory
     * @param size Size of memory region in bytes
     * @param core Derive NUMA node for the region from this core
     * @param handle [out] Handle for the region
     * @param shmaddr_hint Hint for virtual address
     * 
     * @return 
     */
    void * huge_shmem_alloc_at_core(key_t key, size_t size, core_id_t core, int * handle, void * shmaddr_hint = NULL);

    /** 
     * Attach to an existing shared memory region
     * 
     * @param key Unique key identifier to associate with memory
     * @param size Size of memory region in bytes
     * @param shmaddr_hint Hint for virtual address
     * 
     * @return 
     */
    void * huge_shmem_attach(key_t key, size_t size, void * shmaddr_hint = NULL);

    /** 
     * Free shared memory region
     * 
     * @param shmaddr Virtual address mapped to region
     * @param handle Handle of the region
     */
    void huge_shmem_free(void * shmaddr, int handle);

    /** 
     * Detach from previously attached shared memory region
     * 
     * @param shmaddr Pointer to mapped memory
     */
    void huge_shmem_detach(void * shmaddr);

  }
}


namespace Exokernel
{
  /** 
   * Very basic slab allocator. Not thread safe.
   * 
   */
  class Device;
  class Slab_allocator
  {
    byte *                   _virt_base;
    addr_t                   _phys_base;
    bool                     * _taken;
    std::map<void *, bool *> _taken_map;
    Device *                 _device;
    size_t                   _block_size, _num_entries;
  public:
    Slab_allocator(Device * device, size_t bs, size_t n);

    ~Slab_allocator();

    void * alloc(addr_t * phys) {
      for(unsigned i=0;i<_num_entries;i++) {
        if(!_taken[i]) {
          _taken[i] = true;
          void * p = (void *) (_virt_base + (_block_size * i));
          _taken_map[p] = &_taken[i];
          *phys = _phys_base + (_block_size * i);
          return p;
        }
      }
    }

    int free(void * p) {
      bool * tflag = _taken_map[p];
      *tflag = false;
      return 0; 
    }
      
  };


}
#endif // __EXO_MEMORY_H__

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
  Copyright (C) 2013, Juan A. Colmenares <juan.col@samsung.com>
  Copyright (C) 2013, Fengguang Song <f.song@samsung.com>
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
  Copyright (C) 2013, Jilong Kuang <jilong.kuang@samsung.com>
*/

#ifndef __EXO_FAST_SLAB_ALLOCATOR__
#define __EXO_FAST_SLAB_ALLOCATOR__

/** 
 * If not zero, the initialization code zeroes the memory area given to the
 * Fast_slab_allocator via memset(...).
 */
#define ZERO_MEM_AT_INIT (0)

#include "assert.h"

#include "numa_memory.h"
#include "../private/__sym_nbb.h"
#include "pagemap.h"

namespace Exokernel {

  namespace Memory {

#if defined(__x86_64__)
    /** 
     * Fast single-size slab memory allocator that operates as a pool of pre-allocated single-size
     * blocks. 
     * Possible lock classes:  
     *    Exokernel::MCS_lock
     *    Exokernel::Spin_lock
     *    Exokernel::Ticket_lock
     */
    template <class Lock = Exokernel::Spin_lock>
    class Fast_slab_allocator_T {

    private: 
      enum { VERBOSE = 0 };

      unsigned _debug_id;

      /** Block header type. */
      struct Block_header {

        /** Fast_slab_allocator instance which originally allocated the block. */
        Fast_slab_allocator_T<Lock>* owner_allocator;

        /** User-define allocator identifier.  */
        int32_t id;


        // FIXME: We are not using the ref_count because with the block alignment 
        //        we currently don't ensure that this field in the block header
        //        is aligned; therefore, there is no atomicity guarantees.  

        /** Reference count (for debugging purposes).  */
        //volatile 
        int64_t ref_count; // __attribute__((aligned(__SIZEOF_POINTER__)));


        /** Physical address of the beginning of block's data. */
        addr_t block_phys_addr;

        /** Pointer (virtual address) to the beginning of block's data. */
        void* block;

      } __attribute__((aligned(CACHE_LINE_SIZE)));


      unsigned __block_header_size;

      /** Starting virtual address of preallocated contiguous memory area being managed. */
      void*  __mem_area; 

      /** Starting physical address of preallocated contiguous memory area being managed. */
      addr_t __mem_area_phys;

      /** Length of the managed memory area. */
      size_t __mem_area_len; 

      /** Length of an individual block, the data part. */
      size_t __block_len; 

      /** Actual length of an individual block (that is, header + data). */
      size_t __actual_block_len; 

      /** Number of preallocated blocks. */
      size_t __num_of_blocks; 

      /** Number of blocks available */
      volatile uint32_t __num_avail __attribute__((aligned(32)));

      /** Alignment for each block. */
      size_t __alignment;

      /** Circular buffer of preallocated blocks. */
      Symmetric_nbb<Block_header*>* __block_nbb; 

      void*                         __block_nbb_raw; 

      size_t                        __block_nbb_raw_size;

      /** Consumer's access point */
      Symmetric_consumer_axpoint<Block_header*>* __cons_axpoint __attribute__((aligned(__SIZEOF_POINTER__)));

      /** Producer's access point */
      Symmetric_producer_axpoint<Block_header*>* __prod_axpoint __attribute__((aligned(__SIZEOF_POINTER__))); 

      /** Lock for the consumer access-point. */
      Lock __cons_axpoint_lock  __attribute__((aligned(__SIZEOF_POINTER__)));

      /** Lock for the producer access-point. */
      Lock __prod_axpoint_lock __attribute__((aligned(__SIZEOF_POINTER__)));

    public:

      /**
       * Constructor. 
       * @param mem_area pointer to the start of the memory area to be managed.
       * @param mem_len length of the memory area.
       * @param block_len length of each block.
       * @param alignment byte alignment. It must be power of 2.
       * @param needs_phys_addr Flag to do virt-to-phys lookup for each block.
       * @param id allocator identifier (user-defined)
       */

      /*
         WARNING: if physical memory address is needed, the following two conditions must be met
         1. Actual block size (block_len + header) must be no larger than a huge page.
         2. Huge page size must be multiple of actual block size or the total needed memory size is less than a huge page.
      */

      Fast_slab_allocator_T(void* mem_area, 
                            size_t mem_len, 
                            size_t block_len,
                            int alignment = 0, 
                            int32_t core_id = -1,
                            bool needs_phys_addr = false,
                            unsigned debug_id = 0) : _debug_id(debug_id) {

        static Exokernel::Pagemap __page_map;
        assert(sizeof(Block_header) == CACHE_LINE_SIZE);  //cache-alignment requirement
        __block_header_size = sizeof(Block_header);

        if (VERBOSE) {
          PLOG("Fast_slab_allocator::\n\tmem_area(0x%lx-0x%lx)"
               "\n\tmem_len(%lu), \n\tblock_len(%lu), alignment(%d)", 
               (addr_t)mem_area, (addr_t)mem_area + mem_len,
               mem_len, block_len, alignment);
          PLOG("Fast_slab_allocator::Owner debug_id (%u) is %p", debug_id, (void*) this);
        }

        if((mem_area == NULL) || (mem_len == 0))
          throw Exokernel::Constructor_error();


#if (ZERO_MEM_AT_INIT != 0)
        __builtin_memset(mem_area, 0, mem_len);
#endif

        if (mem_area == NULL) {
          panic("Fast_slab_allocator: mem_area is NULL");
        } 

        if (alignment < 0 || (alignment > 1 && !is_power_of_two((unsigned long) alignment))) {
          panic("Fast_slab_allocator: Alignment parameter is not power of 2.");
        }

        __alignment = (alignment == 1) ? 0 : alignment;

        __mem_area = mem_area; 
        __mem_area_len = mem_len;
        __block_len = block_len; 
        __mem_area_phys = __page_map.virt_to_phys(mem_area);

        __actual_block_len = __block_header_size + block_len; 

        if (needs_phys_addr == true) {
          assert(__actual_block_len <= MB(2));
          assert((MB(2) % __actual_block_len == 0) || (mem_len <= MB(2)));
        }

        //printf("__actual_block_len = %d + %d\n",__block_header_size, block_len);

        // Determine the number of blocks
        __num_of_blocks = 0;

        char* m_start = NULL; 
        char* m_end = NULL;   
        char* cur = NULL;     

        if (!__alignment) {
          __num_of_blocks = (__mem_area_len / __actual_block_len);
        }
        else {
          m_start = (char*) __mem_area; 
          m_end = m_start + __mem_area_len; 
          cur = m_start; 

          while (cur < m_end) {
            char* d = cur + __block_header_size;

            if (!check_aligned(d, __alignment)) { 
              d = (char*) align_pointer(d, __alignment);
            } 

            cur = d + __block_len; 
            if (cur <= m_end) {
              __num_of_blocks++;
            }
          }
        }

        __num_avail = __num_of_blocks;

        if (VERBOSE) {
          PLOG("Fast_slab_allocator: (debug_id=%u) preallocated %lu free blocks\n", 
               _debug_id,__num_of_blocks);
        }

        if (__num_of_blocks == 0) {
          panic("Fast_slab_allocator: There is no room for any block.");
        }

        try {

          /* default to current core */
          if (core_id == -1) {
            PWRN("warning: numa_node_of_cpu returned -1; defaulting to NUMA zone 0");
            core_id = 0;
          }
         
          int node = numa_node_of_cpu(core_id);
          assert(node >= 0);

          __block_nbb_raw_size = round_up_page(sizeof(Symmetric_nbb<Block_header*>));

          /* TODO: this would be better with a NUMA aware heap allocator
             since libnuma can only allocate at page level granularity
          */
          __block_nbb_raw = numa_alloc_onnode(__block_nbb_raw_size, node);           
          assert(__block_nbb_raw);

          /* allocate the NBB (Non-blocking Buffer) using the previously allocated memory. This
             is used to maintain the list of free blocks. */
              
          __block_nbb = new (__block_nbb_raw) Symmetric_nbb<Block_header*>(__num_of_blocks, 
                                                                           (int32_t) core_id);

          assert(__block_nbb);
        }
        catch(...) {
          panic("Fast_slab_allocator: Uncaught exception.");
        }

        __cons_axpoint = &(__block_nbb->consumer_axpoint);
        __prod_axpoint = &(__block_nbb->producer_axpoint);

        /* populate the ring buffer */
        size_t block_cnt = 0; 

        m_start = (char*) __mem_area; 
        m_end = m_start + __mem_area_len; 
        cur = m_start; 

        //unsigned __used_page = 0;
        //aggregated_size is used to record the total size of used huge pages except the current one.
        uint32_t aggregated_size = 0;
         
        while (cur < m_end) {
          char* d = cur + __block_header_size;

          if (__alignment && !check_aligned(d, __alignment)) { 
            d = (char*) align_pointer(d, __alignment);
          } 

          cur = d + __block_len; 
          if (cur <= m_end) {
            Block_header* hdr = reinterpret_cast<Block_header*>(d - __block_header_size);
            assert(hdr != NULL);
            hdr->owner_allocator = this; 
            hdr->id = core_id;
            hdr->ref_count = 0;
            hdr->block = d; 

            // Store physical address of block
            assert((umword_t) d >= (umword_t) __mem_area);
            assert((umword_t) d < ((umword_t) __mem_area + (umword_t) __mem_area_len));

            /* the following code is to populate the physical address for each block. For each
               new huge page, we update the starting physical address and calculate the offset
               based on the virtual address and used memory size */
            if (needs_phys_addr) {
              if ((((umword_t) d - __block_header_size) % MB(2)) == 0) {
                aggregated_size = (umword_t) d - __block_header_size - (umword_t) __mem_area;
                __mem_area_phys = __page_map.virt_to_phys(d - __block_header_size);
              }
              assert(__mem_area_phys != 0);
              hdr->block_phys_addr = __mem_area_phys + ((umword_t) d - (umword_t) __mem_area - aggregated_size);
            }

            if(__prod_axpoint->insert_item(hdr) != NBB_OK) {
              panic("Fast_slab_allocator: "
                    "Couldn't insert item in the circular buffer during construction.");
            }
            assert(hdr->block);
            block_cnt++;
          }
        }

        // hack--------------------------
        cur = m_start; 
        while (cur < m_end) {
          char* d = cur + __block_header_size;

          if (__alignment && !check_aligned(d, __alignment)) { 
            d = (char*) align_pointer(d, __alignment);
          } 

          cur = d + __block_len; 
          if (cur <= m_end) {
            Block_header* hdr = reinterpret_cast<Block_header*>(d - __block_header_size);
            assert(hdr);
            assert(hdr->block);
          }
        }
        // hack--------------------

        assert(block_cnt == __num_of_blocks); 
      }

      /** Destructor. */
      ~Fast_slab_allocator_T() {
        /* clean up symmetric nbb storage */
        assert(__block_nbb);
        numa_free(__block_nbb_raw, __block_nbb_raw_size);
      } 

      /** 
       * Gets a block from the pool. 
       * @return a pointer to the block, if successful; otherwise, NULL.
       */
      void* alloc() {
        void* block = NULL; 
        Block_header* block_hdr = NULL;

        __cons_axpoint_lock.lock();

        int err = __cons_axpoint->read_item(block_hdr); 
        // if (err == NBB_OK) {
        // FIXME: No atomicity guarantees on ref_cout due to misalignment ; see note above.
        // int64_t ref_count = __sync_add_and_fetch(&(block_hdr->ref_count), 1);
        // asm volatile("mfence":::"memory"); // Although we made ref_counter aligned, we still need this
        //                                    // fence to guarantee atomicity of operations on ref_counter 
        // if(ref_count > 1) {
        //   PWRN("%s: ref count = %lld, block_hdr addr = %p, core id = %x, debud_id = %d\n",
        //        __func__, ref_count, block_hdr, Thread::core_id(), _debug_id);
        // }

        // assert(ref_count == 1);
        // }

        __cons_axpoint_lock.unlock();

        if (err == NBB_OK) {
          block = block_hdr->block;
          if (block == NULL) {
            PLOG("block is NULL? alloc from %d", _debug_id);
          }
          assert(block);

          uint32_t temp = __num_avail;
          temp --;
          __num_avail = temp;  
        }
        else if (err == NBB_EMPTY) {
          if (VERBOSE) {
            PLOG("[%lu] NBB_EMPTY! debug_id=%d", get_block_size(), _debug_id);
          }
          return NULL;
        }   
        else if (err == NBB_EMPTY_BUT_PRODUCER_INSERTING) {
          if (VERBOSE) {
            PLOG("[%lu] NBB_EMPTY as well! debug_id=%d", 
                 get_block_size(), _debug_id);
          }
          return NULL;
        }   


        return block;
      }


      /** 
       * Puts a block back into the pool. 
       * @param block the block
       * @return S_OK if succeeds. 
       * @return E_FAIL if the pool of blocks is pool. 
       * @return E_BAD_PARAM if the allocator is not the owner of the block or the pointer is NULL
       */
      status_t free(void* block) {
        if (block == NULL) {
          return E_BAD_PARAM;
        }

        Block_header* block_hdr = (Block_header*)((byte*)block - __block_header_size);
        if (block_hdr->owner_allocator != this) {
          asm(""); // To prevent weird effects of compiler's optimization.
          PERR("Fast_slab_allocator: I (%p) am not the owner (%p) of the block you gave me!\n",
               (void*) this, (void*) block_hdr->owner_allocator);
          return E_BAD_PARAM; 
        }

        __prod_axpoint_lock.lock();

        // FIXME: No atomicity guarantees on ref_cout due to misalignment ; see note above.
        // if (block_hdr->ref_count == 0) {
        //   __prod_axpoint_lock.unlock();
        //   PLOG("Warning: attempt to double free a block, fast_slab_allocator ignores it.\n");
        //   return E_FAIL;
        // }
        int err = __prod_axpoint->insert_item(block_hdr); 
        //if (err == NBB_OK) {
        // int64_t ref_count = __sync_sub_and_fetch(&(block_hdr->ref_count), 1);
        // asm volatile("mfence":::"memory"); // Although we made ref_counter aligned, we still need this
        //                                    // fence to guarantee atomicity of operations on ref_counter 
        // if (ref_count != 0) {
        //   PWRN("%s: ref count = %lld, block_hdr addr = %p, core id = %x, debud_id = %d\n",
        //        __func__, ref_count, block_hdr, Thread::core_id(), _debug_id);
        // }

        // assert(ref_count == 0);
        //}
        __prod_axpoint_lock.unlock();


        if (err == NBB_OK) {         
          uint32_t temp = __num_avail;
          temp ++;
          __num_avail = temp; 
          return S_OK;
        }

        panic("Fast_slab_allocator free() failed!!!!! err = %d\n", err);
        return E_FAIL;
      }


      /** Returns the size of each block. */
      size_t get_block_size() {
        return __block_len;
      }


      /** Returns number of blocks preallocated (allocator capacity). */
      size_t get_num_of_preallocated_blocks() {
        return __num_of_blocks;
      }

      /** 
       * Returns the actual size of a block (block header + data item), 
       * given the size of a data item. 
       * This static member function is useful to establish the size of 
       * the memory area to be managed.
       * 
       * @param item_size size of each block, as required by the application. 
       * @return the size of block header plus the item_size. 
       */
      static size_t actual_block_size(size_t item_size) {
        return sizeof(Block_header) + item_size; 
      }


      /** 
       * Returns the ID of the allocator where the block originated. 
       * @param block the block.
       * @return the origin allocator ID.
       */
      inline static int get_id_from_block(void* block) {
        if (block == NULL) { 
          return E_BAD_PARAM;
        }

        Block_header* block_hdr = (Block_header*)((byte*)block - sizeof(Block_header));
        return block_hdr->id;
      }


      /** 
       * Returns the physical address of the block (as stored in the block header during construction of
       * the allocator instance). 
       * @param block the block.
       * @return the block's physical address.
       */
      inline static addr_t get_block_phy_addr(void* block) {
        if (block == NULL) {
          return E_BAD_PARAM;
        }

        Block_header* block_hdr = (Block_header*)((byte*)block - sizeof(Block_header));
        return block_hdr->block_phys_addr;
      }

      /** Returns the number of blocks available */
      inline uint32_t num_avail() { 
        return __num_avail; 
      }

      /** Returns the number of blocks available */
      inline signed debug_id() { return this->_debug_id; }

    };


    typedef Fast_slab_allocator_T<> Fast_slab_allocator;


    ///////////////////////////////////////////////////////////////////////////


    /** 
     * Memory area. 
     */
    struct Mem_area {
      void* ptr; //<! Pointer to the beginning of the memory area. 
      size_t len; //<! Length of the memory area.
      addr_t phys_addr; //<! Physical address.
    };

#endif

  }
}

#endif

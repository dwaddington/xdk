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
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __EXO_SPAN_H__
#define __EXO_SPAN_H__

#include <common/assert.h>
#include <exo/memory.h>

#define CONFIG_BUILD_DEBUG
//#define STATS

#define USE_HINT

namespace Exokernel
{
  namespace Memory
  {
    /** 
     * Memory is managed in blocks known as "spans".  A span is the unit of growth
     * in the allocator.  This span class is based on the use of a
     * bitmap to track allocations.
     * 
     */
    template <size_t SPAN_SIZE, 
              unsigned BASE_ALIGNMENT,
              typename _Allocator = class Exokernel::Memory::Typed_stdc_allocator<void> >

    class Slab_metadata_allocator {

    private:

      enum { 
        BYTES_PER_MWORD = sizeof(umword_t),
        BITS_PER_MWORD  = BYTES_PER_MWORD * 8,        
        verbose = false
      };

      struct span_header_t {
        unsigned _num_blocks;
        unsigned _bitmap_size_bytes;
        umword_t _bitmap[0];
      } __attribute__((packed));

      span_header_t *     _data;
      void *              _unaligned_data;
      volatile mword_t    _initialized __attribute__((aligned(sizeof(mword_t))));
      unsigned            _span_size;
      size_t              _elem_size_bytes;
      byte *              _first_block_addr;
      byte *              _last_block_addr;
      mword_t             _stat_alloc_count  __attribute__((aligned(sizeof(mword_t))));
      mword_t             _stat_free_count   __attribute__((aligned(sizeof(mword_t))));
      mword_t             _last_word_checked __attribute__((aligned(sizeof(mword_t))));

      Allocator_base *    _allocator;
      bool                _local_free_allocator;

    public:

      // ctor
      Slab_metadata_allocator(size_t elem_size, Allocator_base * allocator = NULL) :
        _data(NULL),
        _unaligned_data(NULL),
        _initialized(0ULL), 
        _span_size(SPAN_SIZE),
        _elem_size_bytes(elem_size),
        _first_block_addr(0),
        _last_block_addr(0), 
        _stat_alloc_count(0),
        _stat_free_count(0),
        _last_word_checked(0)
      {
        assert(SPAN_SIZE >= PAGE_SIZE); // span memory is rounded up to N pages.
        if(allocator) {
          _allocator = allocator;
          _local_free_allocator = false;
        }
        else {
          _allocator = new Exokernel::Memory::Stdc_allocator();
          _local_free_allocator = true;
        }
      }

      
      ~Slab_metadata_allocator() {
        if(_unaligned_data)
          ::free(_unaligned_data);
        
        if(_local_free_allocator)
          delete _allocator;
      }

      /** 
       * Output some statistics
       * 
       */
      void info()
      {
        if(!((_stat_alloc_count == 0) && (_stat_free_count == 0))) {
          PINF("[%p] Slab_metadata_allocator allocated=%lu freed=%lu",(void*) this,_stat_alloc_count, _stat_free_count);
        }
      }

      /** 
       * Called to activate the span and alloc memory (_span_size bytes in size).
       * 
       * @param elem_size Size of element
       */
      bool init(bool lazy = false) {

        if(_initialized) return false; /* don't try a CAS unless we need to */

        if(!__sync_bool_compare_and_swap(&_initialized,0UL,1UL)) return false;
        
        if(verbose) {
          if(lazy) 
            PLOG("Lazy initializing Span object (%p)", (void*) this);
          else
            PLOG("Eager initializing Span object (%p)", (void*) this);
        }

        assert(_span_size % PAGE_SIZE == 0);

        /* allocate memory; multiples of page size */
        _unaligned_data = ::malloc(_span_size+BASE_ALIGNMENT);
        assert(_unaligned_data);

        __builtin_memset(_unaligned_data,0,_span_size+BASE_ALIGNMENT);
        _data = (span_header_t *)_unaligned_data;

        /* align memory */
        if(!check_aligned(_unaligned_data,BASE_ALIGNMENT))
          _data = (span_header_t *) align_pointer((void*)_unaligned_data, BASE_ALIGNMENT);
        
        assert(_data);        
        assert_alignment(_data,BASE_ALIGNMENT);
          
        _data->_num_blocks = 0;
        _data->_bitmap_size_bytes = BYTES_PER_MWORD;

        /* solve the best fit of bitmap bytes vs. block bytes */
        while((((_data->_num_blocks) * _elem_size_bytes) + _data->_bitmap_size_bytes) < (_span_size - sizeof(span_header_t))) {
          if((_data->_num_blocks % BITS_PER_MWORD) == 0) {
            _data->_bitmap_size_bytes += BYTES_PER_MWORD;
          }
          if((((_data->_num_blocks) * _elem_size_bytes) + _data->_bitmap_size_bytes) > _span_size) break;
          _data->_num_blocks++;
        }

        _data->_num_blocks--;

        assert(_data->_bitmap_size_bytes*sizeof(mword_t) >= _data->_num_blocks);

        byte * tmp = _first_block_addr = ((byte *)_data->_bitmap + _data->_bitmap_size_bytes);

        /* deal with alignment requirement */
        if(BASE_ALIGNMENT > 0) {

          while((unsigned long) _first_block_addr & (BASE_ALIGNMENT - 1)) 
            _first_block_addr++;

          /* we've lost some blocks due to alignment */
          unsigned num_blocks_lost = ((_first_block_addr - tmp) / _elem_size_bytes);

          if(verbose)
            PDBG("Slab_metadata_allocator: Alignment lost %u blocks.",num_blocks_lost);

          _data->_num_blocks -= num_blocks_lost;
        }

        assert(_data->_num_blocks > 0);
        _last_block_addr = _first_block_addr + (_data->_num_blocks * _elem_size_bytes);

        if(verbose)
          PDBG("[0x%p] New Slab_metadata_allocator object: 4K (%lu bits for bitmap, %u total blocks, block size %lu bytes, 0x%p-0x%p)",
               (void *) this,
               _data->_bitmap_size_bytes*sizeof(mword_t), 
               _data->_num_blocks,
               _elem_size_bytes,
               _first_block_addr,
               _last_block_addr);

        assert(_last_block_addr);
        _initialized = 2;
        return true;
      }

    private:

      void * __check_bitmap_word(unsigned w) 
      {
        unsigned idx = 0;

        umword_t current_mword = _data->_bitmap[w];
        if(current_mword == ~(0UL)) return NULL;

        idx = __builtin_ffsll(~current_mword) - 1;
        assert(idx < BITS_PER_MWORD);

        /* check range; just because there is a bitmap bit, doesn't mean that
           there is a block available 
        */
        if((idx + (w*BITS_PER_MWORD)) >= _data->_num_blocks)
          return NULL;

        /* do atomic compare and exchange */
        umword_t new_word = current_mword | (1UL << idx);

        if(!__sync_bool_compare_and_swap((volatile umword_t *) &_data->_bitmap[w], 
                                         current_mword,
                                         new_word)) {
              
          return NULL;
        }
          
        /* success, now calculate block address */
        unsigned block_num = idx + (w*BITS_PER_MWORD);

        assert(block_num < _data->_num_blocks);
        assert(_first_block_addr > 0);


        //      PLOG("Span_next_free allocating from block %u",block_num);

        byte * block = _first_block_addr + (_elem_size_bytes * block_num);
            
        if(BASE_ALIGNMENT) {
          assert((BASE_ALIGNMENT != 1) && !(BASE_ALIGNMENT & (BASE_ALIGNMENT - 1))); /* check 2^N */
          assert(!((unsigned long) block & (BASE_ALIGNMENT - 1)));                   /* check alignment */
        }
            
        if(block > _last_block_addr)
          panic("Out of range block in Small_object_allocator : block (0x%p) > _last_block_addr (0x%p)", 
                block, _last_block_addr);

#ifdef STATS
        __sync_fetch_and_add(&_stat_alloc_count,1);
#endif

        return (void *) block;
      }

    public:
      /** 
       * Find the next free block
       * 
       * 
       * @return Pointer to next free block, or NULL if non available
       */
      void * span_next_free(bool& init_occurred) {
        
        if(_initialized == 0) {
          init_occurred = init(); 
        }
       
        while(_initialized < 2); // wait for initialization to complete.
        
        assert(_last_block_addr);
        assert(_data);
        const unsigned num_bitmap_words = _data->_bitmap_size_bytes/sizeof(umword_t);

        for(unsigned w=0;w<num_bitmap_words;w++) {
          void * p = __check_bitmap_word(w);
          if(p) {
#ifdef USE_HINT
            __sync_fetch_and_add(&_last_word_checked,w);
#endif
            return p;
          }
        }
        return NULL; /* no space left */
      }


      /** 
       * Check a block pointer is valid in this span
       * 
       * @param ptr 
       * 
       * @return 
       */
      bool is_valid(void * ptr) const {
        assert(_initialized);
        return 
          (ptr >= _first_block_addr && ptr <= _last_block_addr) &&
          ((((byte *) ptr) - _first_block_addr) % _elem_size_bytes == 0)
          ;
      }

      /** 
       * Free a block in the span.
       * 
       * @param ptr Address of the block to free
       * 
       * @return S_OK or E_INVALID_REQUEST if out of bounds.
       */
      status_t span_free(void * ptr) {

        if(_initialized < 2)
          panic("Small_object_allocator: free called on uninitialized Slab_metadata_allocator object at 0x%p\n",this);

        /* range check */
        if(!((ptr >= _first_block_addr) && (ptr <= _last_block_addr)))
          return E_OUT_OF_BOUNDS; // this is not an exception
       

        /* calculate slot address */
        unsigned slot = (((byte*) ptr) - _first_block_addr) / _elem_size_bytes; // slot counts from 0

        assert(slot < _data->_num_blocks);

        /* now we have a valid slot, clear the bit */
        unsigned w = slot / BITS_PER_MWORD;
        unsigned idx = slot % BITS_PER_MWORD;

        assert(w <= _data->_bitmap_size_bytes/sizeof(umword_t));
        umword_t * bitmap = _data->_bitmap;
#ifdef CONFIG_BUILD_DEBUG
        unsigned retries = 0;
#endif
      retry_free:

#ifdef CONFIG_BUILD_DEBUG
        if(retries > 256) panic("Small_object_allocator: free retry livelock?");
#endif

        assert(bitmap[w] & (1UL << idx)); /* make sure its already set */
        umword_t current_word = bitmap[w];
        umword_t new_word = current_word & ~(1UL << idx);

        if(!__sync_bool_compare_and_swap((volatile umword_t *) &_data->_bitmap[w], //&bitmap[w], 
                                         current_word,
                                         new_word)) {
          retries++;
          goto retry_free; /* contention happened; retry */
        }

#ifdef STATS
        __sync_fetch_and_add(&_stat_free_count,1);
#endif
        return S_OK;
      }


    } __attribute__((aligned));

  }
}

#endif // __EXO_SPAN_H__

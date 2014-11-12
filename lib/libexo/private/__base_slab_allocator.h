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

#ifndef __EXO_BASE_SLAB_ALLOCATOR_H__
#define __EXO_BASE_SLAB_ALLOCATOR_H__

#include "__span.h"
#include <stdlib.h>

namespace Exokernel
{
  namespace Memory
  {
    /**
     * Class for small object allocation.  This is generally used as the base
     * allocator for other slab allocators.
     *
     * Limitations: this allocator does not support dynamic shrinking only 
     *              dynamic growth. This is not ABA safe.
     * 
     * @param BLOCK_SIZE Size of the blocks in the slab
     * @param BASE_ALIGNMENT Alignment of first block in slab
     * 
     */
    template <unsigned BASE_ALIGNMENT = 8>
    class Base_object_allocator
    {
    private:

      enum { 
        verbose         = 0,
        SPAN_SIZE       = PAGE_SIZE*64,
        BYTES_PER_MWORD = sizeof(umword_t),
        BITS_PER_MWORD  = BYTES_PER_MWORD * 8,
        Max_span_count  = MB(256)/SPAN_SIZE,
      };

      bool     _initialized;
      umword_t _allocated_bytes;
      size_t   _block_size;

    private:
   
      typedef Exokernel::Memory::Slab_metadata_allocator<SPAN_SIZE, BASE_ALIGNMENT> Span;   

      Span * _spans[Max_span_count];
      void * _span_memory;

    private:

      /** 
       * Allocate a new object (spans are initialized lazily)
       * 
       * 
       * @return Pointer to new block.
       */
      void * _alloc() {

        void * p = NULL;       
        for(unsigned i=0;i<Max_span_count;i++) {

          bool init_occurred = false;

          p = _spans[i]->span_next_free(init_occurred);

          if(init_occurred) {
            __sync_fetch_and_add(&_allocated_bytes, SPAN_SIZE);
          }
          if(p)
            return p;
        }

        PLOG("Total footprint = %lu KB", _allocated_bytes / 1024);
        panic("no more spans! (Max_span_count=%u)",Max_span_count);
        return 0; //rptr;
      }


      /** 
       * Free a previously allocated block.
       * 
       * @param ptr Pointer to block
       * 
       * @return S_OK on success, E_FAIL on block not found.
       */
      status_t _free(void* ptr) {
        
        for(unsigned i=0;i<Max_span_count;i++) {
          assert(_spans[i]);

          /* TODO: perform a lazy free to help with ABA problem */
          if(_spans[i]->span_free(ptr) == S_OK) 
            return S_OK;
        }
        PWRN("WARNING: free on object not found (Base_object_allocator)");
        return E_FAIL;
      }


    public:
      // ctor
      Base_object_allocator(size_t block_size) : _initialized(false), _allocated_bytes(0), _block_size(block_size) {
        
        if(verbose) {
          PLOG("Base_object_allocator(%lu) %lu objects per span, %u max spans",
               block_size, SPAN_SIZE/block_size, Max_span_count);
          PLOG("Sizeof(Base_object_allocator) = %lu KB",
               REDUCE_KB(sizeof(Base_object_allocator)));
        }

        assert(Max_span_count > 0);

        /* initialize span objects */
        _span_memory = ::malloc(sizeof(Span)*Max_span_count);
        assert(_span_memory);

        __builtin_memset(_span_memory,0,sizeof(Span)*Max_span_count);

        Span * objs = (Span *) _span_memory;
        assert(objs);
        for(unsigned i=0;i<Max_span_count;i++,objs++) {          
          _spans[i] = ::new ((void*)objs) Span(_block_size);
          assert(_spans[i]);          
        }
        
        /* allocate memory for first span, the rest are lazyily allocated */
        _spans[0]->init(false);

        // PDBG("Allocated %lu KB for Span placeholders",
        //      (sizeof(Exokernel::Memory::Span<SPAN_SIZE, BASE_ALIGNMENT>)*Max_span_count) / 1024);
        _initialized = true;
      }

      // dtor
      ~Base_object_allocator() {
        //        Genode::env()->heap()->free(_span_memory,sizeof(Span)*Max_span_count);
        ::free(_span_memory);
      }

     
    public:

      /** 
       * Output debugging statistics
       * 
       */
      void info() {
        for(unsigned i=0;i<Max_span_count;i++) {
          assert(_spans[i]);
          _spans[i]->info();
        }
      }


      /** 
       * Check if a pointer is valid in a span.
       * 
       * @param ptr Pointer to block
       * 
       * @return true if valid
       */      
      bool is_valid(void * ptr) const {
        for(unsigned i=0;i<Max_span_count;i++)
          if(_spans->valid(ptr)) return true;
        return false;
      }

      /*******************************/
      /* Genode::Allocator interface */
      /*******************************/

      /** 
       * Allocate block
       * 
       * @param size Part of Genode interface. Not needed but checked against template block size parameter.
       * @param ptr Out pointer to block
       * 
       * @return true on success, false otherwise
       */
      bool alloc(size_t size, void** ptr) {

        //        PLOG("%s alloc(%lu)",__PRETTY_FUNCTION__,size);
        assert(_initialized);
        if(size != _block_size) {
          PDBG("Allocate called with size (%lu) != block size (%lu)",size,_block_size);
          assert(0);
          return false;
        }
        void * p = _alloc();
        assert(p);
        *ptr = p;
        return true;
      }

      /** 
       * Free a previously allocated block
       * 
       * @param ptr Pointer to block
       * @param size_t Size (not used).
       */
      void free(void* ptr, size_t) {
        assert(_initialized);
        if(_free(ptr) != S_OK)
          panic("Base_object_allocator: attempt to free object 0x%p failed.",ptr);
      }

      size_t overhead(size_t) {
        assert(0);
        return 0;        
      }
    };


  }
}

#endif // __EXO_BASE_SLAB_ALLOCATOR_H__

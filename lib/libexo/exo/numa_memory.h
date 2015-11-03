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

#ifndef __EXO_NUMA_H__
#define __EXO_NUMA_H__

#include "memory.h"
#include "errors.h"
#include "spinlocks.h"
#include "locks.h"

#ifdef NUMA_ENABLE
        #include <numa.h>
#else
        #include "common/xdk_numa_wrapper.h"
#endif
#include <vector>
#include <map>

namespace Exokernel
{
  namespace Memory
  {
    SUPPRESS_NOT_USED_WARN
    static void dump_numa_info() {
      printf("NUMA available:%s\n", (numa_available()==0) ? "yes" : "no");
      printf("Configured nodes:%d\n", numa_num_configured_nodes());
      printf("Configured cpus:%d\n", numa_num_configured_cpus());
    }

    /** 
     * Provides coarse grained (page-granular) allocation of memory. It
     * is implemented as a wrapper to the libnuma APIs
     */
    class NUMA_basic_allocator : public Allocator_base
    {
    private:
      enum {
        _verbose = true
      };

      numa_node_t _node;
      bool _numa_available;
      std::map<void *, size_t> _allocations; /* it's ok that this meta data isn't NUMA aware */

    public:

      NUMA_basic_allocator(numa_node_t numa_node) :
        _node(numa_node)
      {
        _numa_available = (numa_available()==0);

        if(_verbose) {
          if(_numa_available) {
            PDBG("NUMA_basic_allocator: NUMA is supported on this platform.");
          }
          else {
            PDBG("NUMA_basic_allocator: defaulting to ::malloc since NUMA is not supported on this platform.");
          }
        }
      }

      void* alloc(size_t s, signed param) {
        if(!_numa_available)
          return ::malloc(s);
        else 
          return numa_alloc_onnode(s,(int) param);
      }
      
      void free(void * p) {
        if(!_numa_available)
          return ::free(p);
        else {
          assert(_allocations.find(p)!=_allocations.end());
          return numa_free(p,_allocations[p]);
        }
      }
    };
    
    
    /* Basic allocator which support NUMA and is thread-safe
       but this class does not support memory deallocation
       except by destruction of the class.  Allocation size is 
       limited to 4K (page size).  This class is normally
       used to allocate metadata memory for higher-level memory
       allocators 
    */
    class Basic_metadata_allocator
    {
    private:

      /** 
       * Basic data structure representing a single allocated page from
       * a specific NUMA node
       * 
       */
      class _MD_page_allocator {
      private:
        unsigned             _numa_node;
        char *               _ptr;
        unsigned long        _bytes_remaining;
        Exokernel::Spin_lock _lock;
        
      public:
        _MD_page_allocator(unsigned numa_node)  {
          _numa_node = numa_node;

          if(_numa_node > (unsigned) numa_max_node()) {
            throw Exokernel::Constructor_error(Exokernel::E_INVALID_REQUEST);
          }
          _ptr = (char *) numa_alloc_onnode(PAGE_SIZE,_numa_node);
          if(_ptr == NULL)
            throw Exokernel::Constructor_error(E_NO_MEM);
          _bytes_remaining = PAGE_SIZE;
        }

        ~_MD_page_allocator() {
          numa_free((void*)_ptr, PAGE_SIZE);
          assert(_ptr);          
        }

        void * alloc_from_page(size_t s) {

          if(s > _bytes_remaining) 
            return NULL;

          _lock.lock();
          if(s > _bytes_remaining) {
            _lock.unlock();
            return NULL;
          }
          void * r = (void*) (_ptr + (PAGE_SIZE - _bytes_remaining));
          _bytes_remaining -= s;
          _lock.unlock();
          return r;
        }
      };

      /** 
       * Inner metadata allocator; instantiated on a per-NUMA zone basis
       * 
       */
      class _MD_allocator {

      private:
        Exokernel::RW_lock                _lock;
        std::vector<_MD_page_allocator *> _pages;

        int _numa_node;
        
      public:

        _MD_allocator() : _numa_node(-1) {
        }

        ~_MD_allocator() {
          for(unsigned i=0;i<_pages.size();i++) {
            delete _pages[i];
          }
        }
        
        void set_numa_node(unsigned int numa_node) {
          _numa_node = numa_node;
        }

        void * alloc(size_t s) {
          
          if(s > PAGE_SIZE) return NULL;
          assert(s <= PAGE_SIZE);

          void * ptr;
          
          /* hold read lock, look for existing space */
          _lock.rdlock();
          if(!_pages.empty()) {

            std::vector<_MD_page_allocator*>::reverse_iterator rit;
            for (rit = _pages.rbegin(); rit != _pages.rend(); ++rit) {
              ptr = (*rit)->alloc_from_page(s);
              if (ptr) {
                _lock.unlock();
                return ptr;
              }
            }
          }
          _lock.unlock();
          
          /* need to create a new page */
          _MD_page_allocator * new_pa = new _MD_page_allocator(_numa_node);
          ptr = new_pa->alloc_from_page(s);
          assert(ptr);
          
          /* add to list */
          _lock.wrlock();
          _pages.push_back(new_pa);
          _lock.unlock();
          return ptr;
        }
      };

    private:
      int _max_numa_node;
      _MD_allocator * _per_numa_allocator;

    public:

      Basic_metadata_allocator() : _max_numa_node(numa_max_node()) {
        _per_numa_allocator = new _MD_allocator[_max_numa_node+1];
        assert(_per_numa_allocator);
        for(signed i=0;i<(_max_numa_node+1);i++) {
          /* TODO: check numa node exists */
          PLOG("calling set numa_node");
          _per_numa_allocator[i].set_numa_node(i);
        }
      }

      ~Basic_metadata_allocator() {

        delete [] _per_numa_allocator;
      }

      void * alloc(size_t size, unsigned numa_zone) {
        if((int) numa_zone > _max_numa_node) {
          assert((int)numa_zone <= _max_numa_node);
          return NULL;
        }
        else {
          return _per_numa_allocator[numa_zone].alloc(size);
        }
      }            
    } __attribute__((deprecated));
  }
}

#endif

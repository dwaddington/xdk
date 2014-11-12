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
  @author Daniel Waddington (d.waddington@samsung.com)
  @date: Oct 1, 2012
*/

#ifndef __EXT2FS_BLOCK_CACHE__
#define __EXT2FS_BLOCK_CACHE__

#include <env.h>
#include <cycles.h>

namespace Ext2fs
{  
  enum { 
    BLOCK_SIZE = 512,
    BLOCKS_PER_SILO = 8192, // 4MB WARNING: this cannot be larger than AHCI DMA buffer size.
    MAX_SILOS = 16
  };

  /** 
   * This is a very simple allocator to help reduce the number
   * of calls to env()->alloc_pages();
   * 
   */
  template <unsigned NUM_CHUNKS, size_t PAGES_PER_CHUNK>
  class Simple_allocator
  {
    struct Chunk {
      Spin_lock _chunk_lock;
      unsigned  _taken;
      void *    _p;
    };

    Chunk _chunks[NUM_CHUNKS]; /* array of chunk entries */

  public:
    Simple_allocator() {
      __builtin_memset(_chunks,0,sizeof(_chunks));
    }

    void * alloc()
    {
      for(unsigned i=0;i<NUM_CHUNKS;i++) {
        _chunks[i]._chunk_lock.lock();
        if(_chunks[i]._taken == 0) {
          _chunks[i]._taken++;
          if(_chunks[i]._p == NULL) {
            _chunks[i]._p = env()->alloc_pages(PAGES_PER_CHUNK);
          }
          _chunks[i]._chunk_lock.unlock();
          return _chunks[i]._p;
        }
        _chunks[i]._chunk_lock.unlock();
      }
      panic("not more chunks.");
      return NULL;
    }

    void free(void * p) {
      for(unsigned i=0;i<NUM_CHUNKS;i++) {
        _chunks[i]._chunk_lock.lock();
        if(_chunks[i]._p == p) {
          _chunks[i]._taken--;
          assert(_chunks[i]._taken == 0);
        _chunks[i]._chunk_lock.unlock();
          return;
        }
        _chunks[i]._chunk_lock.unlock();
      }
    }
  };


  /** 
   * This is the filesystem level block cache.  Reading multiple 
   * sectors from the disk is more efficient than reading one
   * sector at once.  For the moment this is a very simple
   * cache aimed at contiguous reads - more like a prefetcher
   * 
   */
  class Block_cache : public block_device_session_t
                      
  {
  private:

    /** 
     * Total number of silos in the block cache
     */
    Simple_allocator<MAX_SILOS, BLOCKS_PER_SILO/8> _mem_allocator;
    
    /** 
     * A silo holds a group of contiguous blocks (4MB worth) 
     * 
     */
    class Silo {

    private:
      void *            _pages;
      size_t            _len;

    public:
      enum { SILO_SIZE_BYTES = BLOCK_SIZE * BLOCKS_PER_SILO };
      aoff64_t    _start_offset;
      aoff64_t    _end_offset;      
      Block_cache * _block_cache;

    public:
      Silo(Block_cache * block_cache,
           aoff64_t offset, 
           block_device_session_t * physical_device_session) : _block_cache(block_cache) {

        _start_offset = offset & ~(0x1FFULL);   /* truncate to nearest block */        
        _len = BLOCKS_PER_SILO * 512;
        _end_offset = _start_offset + _len; 

        //        _pages = env()->alloc_pages(_len / L4_PAGESIZE); /* hmm, we might optimize this? */
        /* allocate memory from head allocator */
        _pages = _block_cache->_mem_allocator.alloc();
        assert(_pages);

        /* populate pages */
        //        OmniOS::info("Allocating silo (_start_offset=%llu,_end_offset=%llu); "
        //                      "performing read.\n", _start_offset,_end_offset);
      //        OmniOS::info("Silo memory @ %p\n",_pages);
        check_ok(physical_device_session->read(_start_offset,_len,_pages));
      }

      ~Silo() {
        //        OmniOS::info("Silo memory @ %p..about to free.\n",_pages);
        //        OmniOS::info("Freeing silo memory (_start_offset=%llu,_end_offset=%llu)\n",
        //                       _start_offset,_end_offset);

        //        env()->free(_pages);
        _block_cache->_mem_allocator.free(_pages);
      }

      /** 
       * Read from the silo
       * 
       * @param offset File system offset
       * @param byte_count Number of bytes
       * @param buffer 
       * 
       * @return S_OK on success, E_FAIL if offset is not in this silo
       */
      status_t read(aoff64_t offset, unsigned byte_count, void * buffer) 
      {
        if(!((offset >= _start_offset) &&
             ((offset + byte_count) <= _end_offset))) {
          return E_FAIL;
        }

        byte * p = ((byte *)_pages) + (offset - _start_offset);
        __builtin_memcpy(buffer,p,byte_count);
        return S_OK;
      }
    };

    struct Silo_entry
    {
      Spin_lock _lock;
      Silo *    _silo;
      uint64_t  _age;
    };

    Silo_entry _silo_entries[MAX_SILOS];
      

  private:
    block_device_session_t * _physical_device_session;

    /** 
     * Find a free silo and take the oldest and use that
     * 
     * 
     * @return Best silo to use (in locked state)
     */
    Silo_entry * find_best_silo() {
      Silo_entry * oldest_silo = _silo_entries;

      for(unsigned i=0;i<MAX_SILOS;i++) {
        _silo_entries[i]._lock.lock();
        if(_silo_entries[i]._age > oldest_silo->_age) 
          oldest_silo = &_silo_entries[i];
        /* found free entry - short circuit */
        if(_silo_entries[i]._age == 0) {
          return &_silo_entries[i];
        }
        _silo_entries[i]._lock.unlock();
      }
      /* remove and release silo */
      oldest_silo->_lock.lock();
      delete oldest_silo->_silo;
      oldest_silo->_age = 0;
      oldest_silo->_silo = NULL;

      return oldest_silo;
    }

    /** 
     * Allocate a silo, either a new one (preferred) or an eviction
     * 
     * @param offset File system offset
     * 
     * @return Allocated silo (locked state)
     */
    Silo_entry * allocate_silo(aoff64_t offset) {
      Silo_entry * s = find_best_silo();
      assert(s);
      assert(s->_age == 0);
      s->_silo = new Silo(this,offset, _physical_device_session);
      s->_age = read_cycles();
      /* silo is still locked */
      return s;
    }

    /** 
     * Top-level function to do a read from the silos
     * 
     * @param offset File position byte offset
     * @param byte_count Number of bytes 
     * @param data Data
     * 
     * @return 
     */
    status_t do_silo_read(aoff64_t offset, unsigned byte_count, void * data)
    {
      /* first try to read from the silos */
      for(unsigned i=0;i<MAX_SILOS;i++) {
        if(_silo_entries[i]._silo) {
          _silo_entries[i]._lock.lock();
          if(_silo_entries[i]._silo->read(offset,byte_count,data)==S_OK) {
            _silo_entries[i]._lock.unlock();
            return S_OK;
          }
          _silo_entries[i]._lock.unlock();
        }
      }
      /* otherwise create a new silo */
      Silo_entry * new_silo = allocate_silo(offset);
      status_t err = new_silo->_silo->read(offset,byte_count,data);
      assert(err == S_OK);
      new_silo->_lock.unlock();
      return err;
    }

  public:
    // ctor
    Block_cache(block_device_session_t * physical_device_session) :
      _physical_device_session(physical_device_session)
    {
      __builtin_memset(_silo_entries,0,sizeof(_silo_entries));
    }

  public: 
    // Interface: block_device_session_t
    //
    status_t read(aoff64_t offset, unsigned byte_count, void * buffer) 
    {
      if(byte_count > Silo::SILO_SIZE_BYTES) {
        info("[EXT2FS]: WARNING read too big for block cache.\n");
        return _physical_device_session->read(offset,byte_count,buffer);
      }
      else
        return do_silo_read(offset,byte_count,buffer);
    }

    status_t dummy_read(aoff64_t offset, unsigned byte_count, void * buffer) 
    {
      return _physical_device_session->dummy_read(offset,byte_count,buffer);
    }

    status_t write(aoff64_t offset , unsigned byte_count, void * buffer) 
    {
      return _physical_device_session->write(offset,byte_count,buffer);
    }
    
  };
  
}

#endif // __EXT2FS_BLOCK_CACHE__

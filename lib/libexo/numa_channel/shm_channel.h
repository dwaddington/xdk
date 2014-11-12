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
  Copyright (C) 2014, Changhui Lin <changhui.lin@samsung.com> 
  Copyright (C) 2014, Jilong Kuang <jilong.kuang@samsung.com> 
*/

#ifndef __EXO_NUMA_CHANNEL_H__
#define __EXO_NUMA_CHANNEL_H__

#include "shm_area.h"
#include "spmc_circbuffer.h"

#define SHMSZ_BASE (4096)

#define DEBUG_CHANNEL (0)

namespace Exokernel {

  /**
   * NUMA-aware shared-memory based channel. Be sure to setup huge page and shared memory param in the system.
   * e.g. huge_shmem_set_system_max(SHM_MAX_SIZE) and huge_shmem_set_region_max(SHM_MAX_SIZE);
   *
   * This template class enables inter-process communication. 
   * A channel has two ends, each of which communicates with its counterpart using
   * shared memory.
   * There are two buffers residing in the shared memory, one for a producer to
   * insert messages and the other for a consumer to read messages.
   *
   * There are three template parameters: 
   * 1) T: T* is the data type transferred through the channel. 
   * 2) SIZE: the number of slots of buffer created in the shared memory.
   * 3) BUFFER: the buffer implementation used in the shared memory (the default
   *            buffer implementation is SPMC_circular_buffer<T,SIZE>). 
   */
  template <class T, unsigned SIZE, 
            template <class T, unsigned SIZE> class BUFFER = SPMC_circular_buffer >
  class Shm_channel {

  private:

    Shm_area* _shm_areas[2];    /**< Two shared memory areas. */
    BUFFER<T, SIZE>* _buffers[2]; /**< Two buffers, each in one shared memory area. */

  public:

    /**
     * Constructor.
     * 
     * One process calls the constructor with 'need_alloc = TRUE' to create a
     * channel;  this process is responsible for allocating the shared memory and
     * initializing the buffer residing in the shared memory.
     *
     * The other process calls this constructor with 'need_alloc = FALSE' to
     * connect to its counterpart; it identifies the allocated shared memory based
     * on the identifier 'idx'.
     *
     * @param idx the index of the channel.
     * @param need_alloc tells whether or not the channel will allocate
     *                   the shared memory area.
     * @param numa_node The numa memory node where the channel resides
     */
    Shm_channel(size_t idx, bool need_alloc, unsigned numa_node) {

      // Shared memory areas for the two buffers.
      for(size_t i = 0; i < 2; i++) {
        _shm_areas[i] = new Shm_area();

        void* ptr = _shm_areas[i]->init(2*idx + i + 1000, 
                                        SHMSZ_BASE + sizeof(BUFFER<T, SIZE>), 
                                        need_alloc,
                                        numa_node);
        if (need_alloc) {
          // create two new buffers on the shared memory.
          _buffers[i] = new(ptr) BUFFER<T, SIZE>();

#if (DEBUG_CHANNEL != 0)
          std::cout << "When creat: buf[" << i << "] : attached @ " 
                    << _buffers[i] << std::endl;
#endif
        }
        else {
          // Switch the two existing buffers created by its counterpart.
          _buffers[(i+1)%2] = (BUFFER<T, SIZE> *)ptr;

#if (DEBUG_CHANNEL != 0)
          std::cout << "When match: buf[" << (i+1)%2 << "] : attached @ " 
                    << _buffers[(i+1)%2] << std::endl;
#endif
        }
      }
    }

    /** Destructor. */ 
    ~Shm_channel() {
      for(size_t i = 0; i < 2; i++) { 
        _shm_areas[i]->~Shm_area();
      }
    }

    /**
     * Inserts an item into the channel.
     * @param[out] item the ptr to the item.
     * @return status of this operation.
     */
    inline status_t produce(T* item) { 
      return _buffers[0]->produce(item); 
    }

    /**
     * Extracts an item from the channel.
     * @param[out] item the ptr to the extracted item.
     * @return status of this operation
     */
    inline status_t consume(T** item) {
      return _buffers[1]->consume(item); 
    }

    /** Prints on screen information about the channel. */
    void dump() {
      std::cout << std::endl << "**** Shm_channel information :" << std::endl;
      std::cout << "** Produce end: " << std::endl; 
      _buffers[0]->dump();
      std::cout << "** Consume end: " << std::endl; 
      _buffers[1]->dump();
      std::cout << std::endl;
    }

  };

}

#endif

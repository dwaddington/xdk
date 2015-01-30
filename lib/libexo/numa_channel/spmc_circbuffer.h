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
  Copyright (C) 2013, Jilong Kuang <jilong.kuang@samsung.com>
  Copyright (C) 2013, Juan A. Colmenares <juan.col@samsung.com>
*/

#ifndef __EXO_SPMC_CIRCULAR_BUFFER_H__
#define __EXO_SPMC_CIRCULAR_BUFFER_H__

#include "common/types.h"

namespace Exokernel {  

  /* dummy assembly operation to prevent compiler re-ordering of instructions */
#define COMPILER_BARRIER() do { asm volatile("" ::: "memory"); } while(0)

#define HEAD_TAIL_DISTANCE    (8)

  enum {
    E_SPMC_CIRBUFF_OK = 0,
    E_SPMC_CIRBUFF_EMPTY = 1,
    E_SPMC_CIRBUFF_FULL =  2,
    E_SPMC_CIRBUFF_CONTENTION = 4,
    E_SPMC_CIRBUFF_INV_PARAM = 5,
    E_SPMC_INV = 6,
  };

  /** 
   * Circular buffer for a single-producer, multiple-consumer model. 
   * The circular buffer stores pointers to instances of type T; that is, T*.  
   * SIZE has to be power of 2. 
   */
  template <class T, unsigned SIZE>
  class SPMC_circular_buffer {

  private:

    const uint32_t _mask;

    volatile uint32_t _prod_head __attribute__((aligned(32))); 
    volatile uint32_t _cons_head __attribute__((aligned(32))); 

    volatile uint32_t _prod_tail __attribute__((aligned(32)));
    volatile uint32_t _cons_tail __attribute__((aligned(32)));

    const uint32_t _begin __attribute__((aligned)); 
    const uint32_t _end __attribute__((aligned)); 

    T* _buffer[SIZE] __attribute__((aligned(64))); 

  public:

    /** Constructor. */
    SPMC_circular_buffer() : _mask(SIZE-1), _begin(0), _end(SIZE) {
    
      // Make sure SIZE or _end is power-of-two.   
      assert((_end != 0) && ((_end & (_end - 1)) == 0));

      _prod_head = 0; 
      _prod_tail = 0; 
      _cons_head = 0; 
      _cons_tail = 0; 
  
      for (size_t i = 0; i < _end; i++) {
        _buffer[i] = NULL; 
      }
 
      for (size_t i = 0; i < _end; i++) {
        assert((((uint64_t) &_buffer[i]) % 8) == 0);
      }
    }


    /** 
     * Removes the next available from the channel and returns a pointer to it. 
     *
     * @param[in,out] item pointer to the returned item.
     * @return E_SPMC_CIRBUFF_OK on success. 
     * @return E_SPMC_CIRBUFF_CONTENTION if it fails due to contention with
     *         another consumer. 
     * @return E_SPMC_CIRBUFF_EMPTY if the buffer is empty. 
     */
    status_t consume(T** item) {
      uint32_t cons_head, prod_tail;
      uint32_t cons_next, entries;

      cons_head = _cons_head;
      prod_tail = _prod_tail;

      entries = prod_tail - cons_head;

      if (entries < HEAD_TAIL_DISTANCE) {
        return E_SPMC_CIRBUFF_EMPTY; 
      }

      cons_next = cons_head + 8;
      bool result = __sync_bool_compare_and_swap(&_cons_head, cons_head, cons_next);

      if (result) {
        //uint32_t index = cons_head & _mask;
        uint32_t index = cons_head % SIZE;
        *item = _buffer[index];
        COMPILER_BARRIER();

        /*
         * If there are other dequeues in progress that preceded us,
         * we need to wait for them to complete
         */
        while (_cons_tail != cons_head)
          cpu_relax();

        _cons_tail = cons_next;
        return E_SPMC_CIRBUFF_OK;
      }
      return E_SPMC_CIRBUFF_CONTENTION;
    }


    /** 
     * Inserts an item into the buffer.
     * @param item pointer to a data item 
     * 
     * @return E_SPMC_CIRBUFF_OK on success. 
     * @return E_SPMC_CIRBUFF_FULL if the buffer is full. 
     */
    status_t produce(T* item) {
      if (item == NULL) {
        return E_SPMC_CIRBUFF_INV_PARAM;
      }

      uint32_t prod_head, cons_tail;
      uint32_t prod_next, free_entries;
 
      prod_head = _prod_head;
      cons_tail = _cons_tail;

      free_entries = _mask + cons_tail - prod_head;

    
      if (free_entries < HEAD_TAIL_DISTANCE) {
        return E_SPMC_CIRBUFF_FULL; 
      } 

      prod_next = prod_head + 8;
      _prod_head = prod_next;

      //uint32_t index = prod_head & _mask;
      uint32_t index = prod_head % SIZE;
 
      _buffer[index] = item;
      COMPILER_BARRIER();

      _prod_tail = prod_next;
      return E_SPMC_CIRBUFF_OK;
    }


    /** Prints on screen information about the channel. */
    void dump() {
      std::cout << "===========================" << std::endl
                << "SPMC buffer :" << std::endl
                << "prod_head  = " << _prod_head << std::endl
                << "prod_tail  = " << _prod_tail << std::endl
                << "cons_head  = " << _cons_head << std::endl
                << "cons_tail  = " << _cons_tail << std::endl
                << "begin = " << _begin << std::endl
                << "end   = " << _end << std::endl
                << "===========================" << std::endl;
    }

  };

}

#endif // __EXO_SPMC_CIRCULAR_BUFFER_H__


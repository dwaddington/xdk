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

#include "../exo/types.h"

namespace Exokernel {  

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

    const static uint64_t __dummy_addr = ~(0x0);

    /** Index of the next item to write. */
    volatile uint64_t _head __attribute__((aligned)); 

    /** Index of the next item to read. */
    volatile uint64_t _tail __attribute__((aligned));

    const uint64_t _begin __attribute__((aligned)); 
    const uint64_t _end __attribute__((aligned)); 

    T* _buffer[SIZE] __attribute__((aligned)); 

  public:

    /** Constructor. */
    SPMC_circular_buffer() : _begin(0), _end(SIZE) {
    
      // Make sure SIZE or _end is power-of-two.   
      assert((_end != 0) && ((_end & (_end - 1)) == 0));

      _head = 0; 
      _tail = 0; 
  
      for (size_t i = 0; i < _end; i++) {
        _buffer[i] = (T*) __dummy_addr; 
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
      unsigned temp = _tail;
      if(_head - temp == 0) {
        return E_SPMC_CIRBUFF_EMPTY; 
      }

      bool result = __sync_bool_compare_and_swap(&_tail, temp, (temp+1));

      if (result) {
        unsigned index = temp % SIZE;
        *item = _buffer[index];
        _buffer[index] = (T*) __dummy_addr;
        return E_SPMC_CIRBUFF_OK;
      }

      return E_SPMC_CIRBUFF_CONTENTION;
    }


    /** 
     * Inserts an item into the buffer.
     * @param item pointer to a data item 
     * 
     * @return E_SPMC_CIRBUFF_OK on success. 
     * @return E_SPMC_CIRBUFF_CONTENTION if it fails due to contention with
     *         another consumer. 
     * @return E_SPMC_CIRBUFF_FULL if the buffer is full. 
     */
    status_t produce(T* item) {
      if (item == NULL) {
        return E_SPMC_CIRBUFF_INV_PARAM;
      }

      if ((_head - _tail) == SIZE) {
        return E_SPMC_CIRBUFF_FULL; 
      } 

      unsigned index = _head % SIZE;

      if (_buffer[index] == (T*)__dummy_addr) {
        _buffer[index] = item;
        __sync_fetch_and_add(&_head, 1); // GCC built-in function.  
        return E_SPMC_CIRBUFF_OK;
      } 

      return E_SPMC_CIRBUFF_CONTENTION;
    }


    /** Prints on screen information about the channel. */
    void dump() {
      std::cout << "===========================" << std::endl
                << "SPMC buffer :" << std::endl
                << "head  = " << _head << std::endl
                << "tail  = " << _tail << std::endl
                << "begin = " << _begin << std::endl
                << "end   = " << _end << std::endl
                << "===========================" << std::endl;
    }

  };

}

#endif 


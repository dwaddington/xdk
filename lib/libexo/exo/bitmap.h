/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2011-2013
 
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
  Copyright (C) 2011, Daniel G. Waddington <d.waddington@samsung.com>
*/

#ifndef __EXO_BITMAP__
#define __EXO_BITMAP__

#include "spinlocks.h"

#define BITS_PER_WORD (sizeof(unsigned)*8)

namespace Exokernel
{
  /** 
   * Bitmap based tracking, usually used in a slab allocator (thread-safe version). Locks
   * are managed on a per-dword section of the bitmap (i.e. one spin lock for each 32 or 64
   * elements).
   * 
   */
  class Bitmap_tracker_threadsafe
  {
  private:
    unsigned   _max_slot;
    unsigned   _words_for_bitmap;
    volatile unsigned * _bitmap;
    Spin_lock _l;

  public:

    /** 
     * Atomically clear a bit
     * 
     * @param word Word (counting from 0)
     * @param bit Bit position (counting from 0)
     */
    void clear_bit(unsigned word, unsigned bit) {
    }
    
  public:

    Bitmap_tracker_threadsafe(size_t slots) {
      _max_slot = slots;
      _words_for_bitmap = (slots/BITS_PER_WORD);
      if(slots % BITS_PER_WORD) 
        _words_for_bitmap++;

      _bitmap = new unsigned [_words_for_bitmap];

      memset((void*)_bitmap,0,sizeof(unsigned) * _words_for_bitmap);
    }

    ~Bitmap_tracker_threadsafe() {
      delete []_bitmap;
    }


    /** 
     * Return the index to the next free slot; mark slot taken
     * 
     * 
     * @return Index of a free slot
     */
    signed next_free() {

      signed return_idx=-1;

      /* pseudo random starting point */
      unsigned w = 0; //(rdtsc() % _words_for_bitmap);

      unsigned words_checked = 0;
      while(words_checked < _words_for_bitmap) {

        int idx;
        unsigned curr_val = _bitmap[w];

        idx = __builtin_ffs(~curr_val);

        if(idx == 0) {
          //          if(++w == _words_for_bitmap) w = 0;          
          w++;
          if(w==_words_for_bitmap) w = 0;
          words_checked++;
          continue;
        }
        idx--;

        unsigned new_val = curr_val | (1U << idx);
          
        if(!__sync_bool_compare_and_swap(&_bitmap[w],curr_val,new_val))  {
          continue;
        }

        return_idx = idx + (w*BITS_PER_WORD);
        if((unsigned)return_idx >= _max_slot)  {
          break;
        }
        return return_idx;
      }

      return E_INSUFFICIENT_RESOURCES;
    }
    

    /** 
     * Release a slot and set bit to 0 in bitmap to indicate that it is free
     * 
     * @param slot Slot to release
     */
    status_t mark_free(unsigned slot) {

      if(slot > _max_slot) {
        panic("mark_free: slot out of range");
        return E_INVALID_REQUEST;
      }

      unsigned w = slot / BITS_PER_WORD;
      unsigned idx = slot % BITS_PER_WORD;
      assert(w <= _words_for_bitmap);

    retry:
      unsigned old_val = _bitmap[w];
      unsigned new_val = old_val & ~(1U << idx);
      
      if(!__sync_bool_compare_and_swap(&_bitmap[w],old_val,new_val))
        goto retry;

      return S_OK;
    }

    /** 
     * Return whether or not a slot is taken
     * 
     * @param slot 
     * 
     * @return 
     */
    bool is_set(unsigned slot)
    {
      unsigned w = slot / BITS_PER_WORD;
      unsigned idx = slot % BITS_PER_WORD;
      if(w > _words_for_bitmap) panic("invalid slot (0x%x) in bitmap is_set call\n",slot);

      return (_bitmap[w] & (1<<idx));
    }
  };
  
}

#undef BITS_PER_WORD

#endif // __EXO_BITMAP__

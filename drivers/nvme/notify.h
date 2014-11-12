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






#ifndef __NVME_NOTIFY_H__
#define __NVME_NOTIFY_H__

#include <queue>

/** 
 * WARNING: this class has issues with wrapping around the count
 * 
 */
class Notify_object
{
private:
  Exokernel::Semaphore _sem;
  Exokernel::Atomic _next_when;
  Exokernel::Atomic _count;

  void notify(unsigned command_id) {
    atomic_t p = _count.increment_and_fetch();
    if(p == _next_when.read()) {
      _sem.post();
    }
  }


public:

  void set_when(atomic_t when) {
    _next_when.init_value(when);
  }

  /** 
   * Wait for counting event to occur
   * 
   */
  void wait() {
    _sem.wait();
  }

public:
  static void notify_callback(unsigned command_id, void * p) {
    Notify_object * pThis = (Notify_object *)p;
    pThis->notify(command_id);
  }
};    

#endif

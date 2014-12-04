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






#ifndef __CQ_THREAD_H__
#define __CQ_THREAD_H__

#include "nvme_types.h"
#include <libexo.h>

#define ACTIVE_CQ_THREAD  

class NVME_IO_queues;

/** 
 * Class for the CQ thread
 * 
 * @param qbase 
 * 
 * @return 
 */
class CQ_thread : public Exokernel::Base_thread
{
private:
  unsigned _core;
  unsigned _irq;

  NVME_IO_queues * _queues;

public:
  /* debugging */
  unsigned long g_times_woken;
  unsigned long g_entries_cleared;

public:
  CQ_thread(NVME_IO_queues * qbase, unsigned core, unsigned vector);

  void* entry(void* qb);

  unsigned irq() const { return _irq; }
};


/** 
 * Callback manager is used to maintain a table of call back functions
 * and their corresponding void * parameter (usually a cast this pointer).
 * 
 * @param max Maximum number of outstanding call backs (default 128)
 */
class Callback_manager
{
private:

  //  Exokernel::Spin_lock _lock;
  unsigned _hd;

  size_t              _max_callbacks;
  notify_callback_t * _callbacks;
  void **             _callback_params;
  unsigned canary;

public:
  Callback_manager(size_t max) : _hd(1), _max_callbacks(max+1) {
    canary = 0xbeeffeed;

    _callbacks = new notify_callback_t [_max_callbacks];
    memset(_callbacks,0,sizeof(notify_callback_t)*_max_callbacks);

    _callback_params = new void * [_max_callbacks];
    memset(_callback_params,0,sizeof(void*)*_max_callbacks);
  }

  ~Callback_manager() {
    delete _callbacks;
    delete _callback_params;
  }

  /** 
   * Register a callback function.
   * 
   * @param cf Callback function
   * 
   * @return Unique 'slot' identifier for the callback which should be 
   *         used for the command_id of the issued command.
   */
  void register_callback(unsigned slot, notify_callback_t cf, void * param) {
    if(slot > _max_callbacks) {
      panic("exceed max slot capacity in callback manager (slot=%u)",slot);
    }
    assert(_callbacks[slot] == 0);
    _callbacks[slot] = cf;
    _callback_params[slot] = param;
  }


  /** 
   * Clear a previously registered call back
   * 
   * @param slot 
   */
  void deregister_callback(unsigned slot) {    
    //    Exokernel::Spin_lock_guard g(_lock);
    assert(_callbacks[slot]);
    _callbacks[slot] = NULL;
  }

  /** 
   * Call callback function assigned to specific slot.
   * 
   * @param slot Slot corresponding to the callback function
   * 
   * @return E_FAIL if not function assigned to slot.
   */
  status_t call(unsigned slot, unsigned command_id) {
    assert(canary==0xbeeffeed);
    PLOG("Calling callback slot=%u command_id=%u", slot, command_id);
    //    Exokernel::Spin_lock_guard g(_lock);
    if(!_callbacks[slot & 0xffff]) {
      PWRN("call on callback (slot=%x) that is empty.",slot);
      return Exokernel::E_FAIL;
    }
    /* make call back */
    (_callbacks[slot])(command_id,_callback_params[slot]);
    return Exokernel::S_OK;
  }

};



#endif // __CQ_THREAD_H__

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
#include <boost/atomic.hpp>

/** 
 * WARNING: this class has issues with wrapping around the count
 * 
 */
class Notify_object
{
private:
  Semaphore _sem;
  boost::atomic<unsigned long> _next_when;
  //  boost::atomic<unsigned> _count;

  //  Exokernel::Atomic _next_when;
  //  Exokernel::Atomic _count;

  void notify(unsigned long command_id) {

    if(command_id >= _next_when) {
      _sem.post();
    }

    // PLOG("notified of command id=%u", command_id);
    // atomic_t p = _count.increment_and_fetch();
    // if(_count. >= _next_when.read()) {
    //   PLOG("!!!!!posting!!!!");
    //   _sem.post();
    // }
    // else {
    //   PLOG("p=%lu",p);
    // }
  }


public:
  Notify_object() : _next_when(0) {
  }


  void set_when(atomic_t when) {

    //if(when > _next_when.load()) {
      _next_when.store(when);
    //}

    // TRACE();
    // if(_count.read() >= when) { /* could have got there already! */
    //   PLOG("We are there already!!!!!!!!!!!!!!!!!!!!");
    //   _sem.post();
    //   return;
    // }
    // while(!_next_when.cas_bool(_next_when,when));
  }

  /** 
   * Wait for counting event to occur
   * 
   */
  void wait() {
    PLOG("waiting for notify object to be signalled..");
    _sem.wait();
  }

public:
  static void notify_callback(unsigned command_id, void * p) {
    TRACE();
    Notify_object * pThis = (Notify_object *)p;
    pThis->notify(command_id);
  }
};    

class Notify {
  public:
    virtual void action() = 0;
    virtual void wait() = 0;
    virtual bool free_notify_obj() = 0;
};

class Notify_Async : public Notify {
  private:
    unsigned _id;
    bool _free_notify_obj;
  public:
    Notify_Async():_id(0), _free_notify_obj(false){}
    Notify_Async(unsigned id):_id(id), _free_notify_obj(false){}
    Notify_Async(bool free_noitfy_obj):_id(0), _free_notify_obj(free_noitfy_obj){}
    Notify_Async(unsigned id, bool free_noitfy_obj):_id(id), _free_notify_obj(free_noitfy_obj){}
    ~Notify_Async(){}

    void action() {
      PLOG(" Notification called (id = %u) !!", _id);
    }

    void wait() {
      PERR(" Should not be called !!");
      assert(false);
    }

    bool free_notify_obj() {
      return _free_notify_obj;
    }
};

class Notify_Sync : public Notify {
  private:
    Semaphore _sem;

  public:
    Notify_Sync() {}
    ~Notify_Sync() {}

    void action() {
      _sem.post();
    }

    void wait() {
      _sem.wait();
    }

    bool free_notify_obj() {
      return false;
    }
};

class Notify_Verify : public Notify {
  private:
    void* _p;
    char _content;
    unsigned _size;

  public:
    Notify_Verify(void* p, char content, unsigned size) : _p(p), _content(content),_size(size) {}
    ~Notify_Verify() {}

    void action() {
//printf("t = %x%x\n",0xf & (_content >> 4), 0xf & _content);
      for(unsigned i = 0; i<_size; i++) {
//printf("%u: c = %x%x size=%u\n", i, 0xf & (((char*)_p)[i] >> 4), 0xf & ((char*)_p)[i], _size);
        assert(((char*)_p)[i]==_content);
      }
    }

    void wait() {}

    bool free_notify_obj() {
      return true;
    }

};

#endif

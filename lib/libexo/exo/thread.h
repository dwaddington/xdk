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
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
*/

#ifndef __EXO_THREAD_H__
#define __EXO_THREAD_H__

#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <string.h>

#include "sync.h"
#include "errors.h"
#include "types.h"
#include "logging.h"

#ifndef SUPPRESS_NOT_USED_WARN
#define SUPPRESS_NOT_USED_WARN __attribute__((unused))
#endif

namespace Exokernel
{
  class Base_thread
  {
  private:
    pthread_t _thread;
    cpu_set_t _affinity;
    bool _exit;
    void* _arg;
    Exokernel::Event _start_event;

    struct Thread_params {
      void* _param;
      Base_thread* _this;
    };

    static void* __entry(void * p) {
      Thread_params* c = (Thread_params*) p;
      void* r = c->_this->__trampoline(c->_param);
      ::free(p);
      return r;
    }

    void* __trampoline(void* p) {
      _start_event.wait();
      return entry(p);
    }

  protected:
    /** 
     * Thread's entry function. 
     * It must be implemented by the derived concrete class.
     * It must be called directly, but via start().
     */
    virtual void* entry(void* param) = 0;

    /** 
     * Used by super class to check exit status.
     * @return the exit status.  
     */
    bool thread_should_exit() const {
      return _exit;
    }

  public:

    /** 
     * Constructor.
     * @param arg argument 
     * @param cpu single CPU affinity (optional).
     */
    Base_thread(void* arg = NULL, int cpu = -1) {
      _arg = arg; 

      if(!(cpu < sysconf(_SC_NPROCESSORS_ONLN))) {
        PLOG("cpu=%d online=%ld\n",cpu,sysconf(_SC_NPROCESSORS_ONLN));
        assert(cpu < sysconf(_SC_NPROCESSORS_ONLN));
      }

      CPU_ZERO(&_affinity);
      if (cpu > -1) {
        CPU_SET(cpu, &_affinity);              
      }

      int rc;
      Thread_params* p = (Thread_params*) malloc(sizeof(Thread_params));
      assert(p);
      p->_param = _arg;
      p->_this = this;

      if (CPU_COUNT(&_affinity) > 0) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        rc = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &_affinity);
        if (rc != 0) {
          throw Exokernel::Exception("pthread_attr_setaffinity_np failed.");
        }

        rc = pthread_create(&_thread, &attr, __entry, (void*) p);
        
        if (rc != 0) {
          perror("pthread_create failed (with attr):");
          throw Exokernel::Exception("pthread_create failed.");
        }
      
      }
      else {
        rc = pthread_create(&_thread, NULL, __entry, (void*) p);
        if (rc != 0) {
          perror("pthread_create failed (without attr):");
          throw Exokernel::Exception("pthread_create failed.");
        }
      }           
    }

    /** Destructor. */
    virtual ~Base_thread() {
      join();
    }

    /** Starts the thread. */
    void start() {
      _start_event.wake_all();
    }

    /** Makes the thread exit. */
    inline void exit_thread() {
      join();
    } __attribute__((deprecated));


    /** Called by parent to exit the thread. */    
    int join() {
      _exit = true;
      void * rc;
      return pthread_join(_thread, &rc);
    }

    /** Kills the thread (should not be used). */
    int kill(int sig=-9) {
      return pthread_kill(_thread, sig);
    }

    /** Cancels the thread. */
    int cancel() {
      return pthread_cancel(_thread);
    }

  };

  /** Set the name of a thread visible by top etc. */
  SUPPRESS_NOT_USED_WARN
  static status_t set_thread_name(const char * name) {
    if(strlen(name) > 15) 
      return Exokernel::E_INVALID_ARG;

    if(prctl(PR_SET_NAME,name,0,0,0))
      return Exokernel::E_FAIL;
    else
      return Exokernel::S_OK;
  }

}

#endif // __EXO_THREAD_H__

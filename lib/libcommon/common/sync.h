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

#ifndef __EXOKERNEL_SYNC_H__
#define __EXOKERNEL_SYNC_H__

#include "types.h"
#include "errors.h"
#include <pthread.h>
#include <semaphore.h>

/** 
 * Wrapper class for semaphores
 * 
 * @param kind 
 */
class Semaphore
{
private:
  sem_t _sem;

public:
  Semaphore(bool cross_process = false, unsigned int val = 0) {
    int rc = sem_init(&_sem, cross_process, val);
    assert(rc == 0);
  }

  int post() {
    return sem_post(&_sem);
  }

  int wait() {
    return sem_wait(&_sem);
  }

  ~Semaphore() {
    sem_destroy(&_sem);
  }

};

/** 
 * Wrapper class for pthread mutexes
 * 
 */  
class Mutex
{
private:
  pthread_mutex_t     _mutex;
  pthread_mutexattr_t _attr;

public:
  /** 
   * Constructor
   * 
   * @param kind PTHREAD_MUTEX_FAST_NP, PTHREAD_MUTEX_RECURSIVE_NP, 
   *        or PTHREAD_MUTEX_ERRORCHECK_NP
   */
  Mutex(int kind = PTHREAD_MUTEX_FAST_NP) {
    pthread_mutexattr_init(&_attr);
    if(pthread_mutexattr_settype(&_attr,kind)!=0) 
      throw Exokernel::Exception("bad mutex kind");

    pthread_mutex_init(&_mutex, &_attr);
  }

  ~Mutex() {
    pthread_mutexattr_destroy(&_attr);
    pthread_mutex_destroy(&_mutex);
  }


  int lock() {
    return pthread_mutex_lock(&_mutex);
  }

  int unlock() {
    return pthread_mutex_unlock(&_mutex);
  }

  int try_lock() {
    return pthread_mutex_trylock(&_mutex);
  }
};


/** 
 * Binary event used for synchronizing multiple threads on a binary
 * condition variable
 * 
 */
class Event
{
private:
  pthread_cond_t  _cond_var;
  pthread_mutex_t _var_lock;
  bool            _var;

public:

  Event() : _var(false) {
    int rc;
    rc = pthread_mutex_init(&_var_lock,NULL);
    assert(rc==0);

    rc = pthread_cond_init(&_cond_var, NULL);
    assert(rc==0);
  }

  ~Event() {
    pthread_mutex_destroy(&_var_lock);
    pthread_cond_destroy(&_cond_var);
  }

  /** 
   * Wait until the condition exists
   * 
   * 
   * @return S_OK or E_FAIL
   */
  status_t reset_and_wait() {
    int rc;
    pthread_mutex_lock(&_var_lock);
    _var = false;
    while(!_var)
      rc = pthread_cond_wait(&_cond_var, &_var_lock);
    pthread_mutex_unlock(&_var_lock);

    return rc==0 ? Exokernel::S_OK : Exokernel::E_FAIL;
  }

  /** 
   * Wait for condition without reset
   * 
   * 
   * @return 
   */
  status_t wait() {
    int rc;
    pthread_mutex_lock(&_var_lock);
    while(!_var)
      rc = pthread_cond_wait(&_cond_var, &_var_lock);
    pthread_mutex_unlock(&_var_lock);

    return rc==0 ? Exokernel::S_OK : Exokernel::E_FAIL;
  }

  /** 
   * Wait for condition without reset
   * 
   * 
   * @return 
   */
  status_t wait_and_reset() {
    int rc;
    pthread_mutex_lock(&_var_lock);
    while(!_var)
      rc = pthread_cond_wait(&_cond_var, &_var_lock);
    _var = false;
    pthread_mutex_unlock(&_var_lock);

    return rc==0 ? Exokernel::S_OK : Exokernel::E_FAIL;
  }

  /** 
   * Wake any waiting thread
   * 
   * 
   * @return S_OK on success
   */
  status_t wake_one() {
    int rc;
    pthread_mutex_lock(&_var_lock);
    _var = true;
    rc = pthread_cond_signal(&_cond_var);
    assert(rc==0);
    pthread_mutex_unlock(&_var_lock);
      
    return rc==0 ? Exokernel::S_OK : Exokernel::E_FAIL;
  }


  /** 
   * Wake all waiting threads
   * 
   * 
   * @return S_OK on success
   */
  status_t wake_all() {
    int rc;
    pthread_mutex_lock(&_var_lock);
    _var = true;
    rc = pthread_cond_broadcast(&_cond_var);
    pthread_mutex_unlock(&_var_lock);
      
    return rc==0 ? Exokernel::S_OK : Exokernel::E_FAIL;
  }


  /** 
   * Reset event 
   * 
   */
  void reset() {
    pthread_mutex_lock(&_var_lock);
    _var = false;
    pthread_mutex_unlock(&_var_lock);
  }
};


#endif // __EXOKERNEL_SYNC_H__

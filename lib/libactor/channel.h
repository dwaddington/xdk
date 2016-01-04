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
  Copyright (C) 2015, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/spin/mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <common/utils.h>

#include "actor_debug.h"

/** 
 * Configuration item.  If this is defined, for send_and_wait calls
 * the client will synchronize on another semaphore instead of
 * spinning.  This costs, but is useful when threads are sharing cores.
 * 
 */
#define CONFIG_SEM_ON_REPLY

/** 
 * Common interface for all channels. All methods should be thread-safe
 * 
 */
class Channel
{
public:
  virtual void * alloc_msg_buff(std::size_t bytes)=0;
  virtual void free_msg_buff(void * p)=0;
  //  virtual int sync_zerocopy_send_and_wait(void * data, size_t len, long msec_timeout = -1)=0;
  virtual int sync_zerocopy_send(void * data, size_t len)=0;
  virtual int sync_zerocopy_wait_and_respond(void (*callback)(void *, size_t))=0;

  virtual int sync_zerocopy_send_and_wait(void * data, 
                                          size_t len, 
                                          void **out_data, 
                                          size_t* out_len,
                                          long msec_timeout = -1)=0;

  virtual int sync_zerocopy_wait_and_respond(void (*callback)(void *, size_t, void**, size_t*))=0;

  virtual int sync_zerocopy_wait(void ** data, size_t * len)=0;
  virtual int async_zerocopy_respond(void * data, size_t len)=0;

  virtual ~Channel() {}
};


class SharedMemoryChannel : public Channel
{
protected:
  class Semaphore
  {
  private:
    sem_t _sem;
    unsigned char _padding[64];

  public:
    Semaphore(bool cross_process = true, unsigned int val = 0) {
      sem_init(&_sem, cross_process, val);
    }

    ~Semaphore() {
      sem_destroy(&_sem);
    }

    int post() {
      int r = sem_post(&_sem);
      if(r==-1) return errno;
      return 0;
    }

    int wait() {
      int r = sem_wait(&_sem);
      if(r==-1)
        return errno;
      
      return 0;
    }

    int timed_wait(unsigned long usec_timeout) {
      struct timespec ts;
      if(clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        PERR("clock_gettime failed.");
        return -1;
      }

      ts.tv_sec += usec_timeout / 1000000;
      ts.tv_nsec += (usec_timeout % 1000000) * 1000;
      int rc = sem_timedwait(&_sem, &ts);      
      if(rc == -1) { 
        return errno;
      }
      return rc;
    }
  };


private:

  /* static constants */
  static const size_t QUEUE_CAPACITY   = 8;
  static const size_t MAX_MESSAGE_SIZE = 2048;  // also see CHANNEL_MEMORY_SIZE

  /* private types */
  struct Slab {
    unsigned long current;
    /* semantics is +1 so that client won't pop another until
       it has finished with the current message slot.
    */
    unsigned char data[QUEUE_CAPACITY+1][MAX_MESSAGE_SIZE];
    unsigned char padding[256]; /* false sharing prevention */

  public:
    Slab() : current(0) {
    }

    void * next() {
      void * p = (void *) &data[current];
      current++;
      if(current == QUEUE_CAPACITY+1) current = 0;
      return p;
    }
  };

  typedef struct {
    long int offset;
    size_t size;
  } queue_entry_t;
    
  /* use mpmc LFQ instead of spsc - there may be an issue
     with the boost spsc implementation */
  typedef boost::lockfree::queue<queue_entry_t, 
                                 boost::lockfree::capacity<QUEUE_CAPACITY> > 
  ring_buffer_t;

  // struct ring_buffer_t : public boost::lockfree::spsc_queue<queue_entry_t, 
  //                                                           boost::lockfree::capacity<QUEUE_CAPACITY> > 
  // {
  //   unsigned char padding[64]; /* false sharing prevention */
  // };

  // or typedef interprocess_semaphore semaphore_t;
  typedef Semaphore semaphore_t; 

  enum channel_mode_t {
    REQ  = 1,
    RESP = 2,
  };

  std::string                                  channel_name_;
  boost::interprocess::managed_shared_memory * segment_;
  ring_buffer_t *                              s2c_;
  ring_buffer_t *                              c2s_;
  Slab *                                       msgslab_;
  uint32_t *                                   channel_init_;

#ifdef CONFIG_SEM_ON_REPLY
  semaphore_t *                                sem_s2c_;
#endif
  semaphore_t *                                sem_c2s_;

  channel_mode_t                               mode_;
  unsigned                                     push_retries_;


public:

  /** 
   * Client-role constructor
   * 
   * @param channel_name Name of channel advertised as shared memory segment
   * 
   */
  SharedMemoryChannel(const char * channel_name);

  /** 
   * Server-role constructor
   * 
   * @param channel_name 
   * @param capacity 
   */
  SharedMemoryChannel(const char * channel_name, size_t capacity);


  /** 
   * Destructor
   * 
   */
  ~SharedMemoryChannel();

  /** 
   * Allocate shared memory in the segment
   * 
   * @param bytes 
   * 
   * @return 
   */
  INLINE void * alloc_msg_buff(std::size_t bytes) {

    assert(bytes <= MAX_MESSAGE_SIZE);
    return msgslab_->next();

#ifdef USE_SEGMENT_ALLOCATOR_FOR_MESSAGES /* performs bad, but useful for testing */
    return segment_->allocate(bytes);
#endif
  }

  INLINE void free_msg_buff(void * p) {
#ifdef USE_SEGMENT_ALLOCATOR_FOR_MESSAGES /* performs bad, but useful for testing */
    segment_->deallocate(p);
#endif
  }


  /** 
   * Synchronously send a message, block and wait for reply. Use same
   * buffer both ways (i.e. zero-copy)
   * 
   * @param data Data buffer
   * @param len Lenght of data buffer
   */
  // int sync_zerocopy_send_and_wait(void * data, 
  //                                 size_t len, 
  //                                 long msec_timeout = -1);


  int sync_zerocopy_send(void * data, size_t len);

  /** 
   * Wait for a message, then reply in the same buffer (i.e. zero-buffer)
   * 
   * @param callback Function to call to handle message
   */
  int sync_zerocopy_wait_and_respond(void (*callback)(void *, size_t));


  /** 
   * Synchronously send message, block and wait for reply.  Use a send-allocated 
   * buffer for the result.
   * 
   * @param data Data to send.
   * @param len Length of data to send
   * @param out_data Reply data. Must be freed by caller.
   * @param out_len Length of reply data.
   * @param msec_timeout Time out in milliseconds (return POSIX ETIMEDOUT)
   */
  int sync_zerocopy_send_and_wait(void * data, 
                                  size_t len, 
                                  void **out_data, 
                                  size_t* out_len,
                                  long msec_timeout = -1);


  /** 
   * Wait for a message, then reply in the same buffer (i.e. zero-buffer)
   * 
   * @param callback Function to call to handle message
   */
  int sync_zerocopy_wait_and_respond(void (*callback)(void *, size_t, void**, size_t*));

  int sync_zerocopy_wait(void ** data, size_t * len);

  int async_zerocopy_respond(void * data, size_t len);


  /** 
   * Reset the count for push retries.
   * 
   */
  inline void reset_push_retries_count() {
    push_retries_ = 0;
  }

  /** 
   * Return spin retry count.  This can be used to help indicate 
   * queue overflow.
   * 
   * 
   * @return 
   */
  inline unsigned push_retries() const {
    return push_retries_;
  }
  

};

#endif

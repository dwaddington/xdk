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

#include "channel.h"

static const int kPopMaxRetries = 100;

SharedMemoryChannel::SharedMemoryChannel(const char * channel_name, size_t capacity) : push_retries_(0) {
    
  using namespace boost::interprocess;

  LOG("server-side ctor (channel_name=%s)",channel_name);

  channel_name_ = channel_name;
  mode_ = RESP;
    
  /* remove any previous channel with this name */
  shared_memory_object::remove(channel_name);    
    
  try {      
    permissions perm;
    perm.set_unrestricted();
    segment_ = new managed_shared_memory(create_only, channel_name, capacity, 0, perm);
    LOG("segment capacity: %ld",segment_->get_size());
  }
  catch(interprocess_exception& e) {
    ERROR("could not create managed_shared_memory for data plane (capacity=%ld bytes)",
          capacity);
    assert(0);
    throw e;
  }
  
  try {      
    s2c_ = segment_->construct<ring_buffer_t>("s2c")();
    assert(s2c_);

    c2s_ = segment_->construct<ring_buffer_t>("c2s")();
    assert(c2s_);

    sem_c2s_ = segment_->construct<semaphore_t>("sem_c2s")(true,0);
    assert(sem_c2s_);

    msgslab_ = segment_->construct<Slab>("msgslab")();
    assert(msgslab_);

#ifdef CONFIG_SEM_ON_REPLY
    sem_s2c_ = segment_->construct<semaphore_t>("sem_s2c")(true,0);
    assert(sem_s2c_);
#endif

    channel_init_ = segment_->construct<uint32_t>("initflag")(0);
    assert(channel_init_);
  }
  catch(interprocess_exception& e) {
    //      PERR("SharedMemoryChannel constructor (server) exception:%s",e.what());
    ERROR("data plane shared memory construction failed. try deleting shared memory");
    assert(0);
    throw e;
  }

  *channel_init_ = 0xbeeffeed;
}

/** 
 * Client-role constructor
 * 
 * @param channel_name 
 */
SharedMemoryChannel::SharedMemoryChannel(const char * channel_name) : 
  s2c_(NULL),
  c2s_(NULL),
  msgslab_(NULL),
  channel_init_(NULL),
#ifdef CONFIG_SEM_ON_REPLY
  sem_s2c_(NULL),
#endif
  sem_c2s_(NULL)
{
  LOG("client-side (channel_name=%s)", channel_name);

  using namespace boost::interprocess;

  assert(channel_name);
  channel_name_ = channel_name;
  mode_ = REQ;

  bool incomplete;
  unsigned retries = 0;

  try {
    segment_ = new managed_shared_memory(open_only, channel_name);
    LOG("client-side ctor (segment size=%ld)",segment_->get_size());      
      
    do {
      if(!s2c_) 
        s2c_ = segment_->find<ring_buffer_t>("s2c").first;

      if(!c2s_)
        c2s_ = segment_->find<ring_buffer_t>("c2s").first;

      if(!sem_c2s_)
        sem_c2s_ = segment_->find<semaphore_t>("sem_c2s").first;

      if(!msgslab_)
        msgslab_ = segment_->find<Slab>("msgslab").first;

      if(!channel_init_)
        channel_init_ = segment_->find<uint32_t>("initflag").first;

      incomplete = !sem_c2s_ || !s2c_ || !c2s_ || !msgslab_ || !channel_init_;

#ifdef CONFIG_SEM_ON_REPLY
      if(!sem_s2c_)
        if(!(sem_s2c_ = segment_->find<semaphore_t>("sem_s2c").first)) 
          incomplete = true;
#endif
      if(incomplete) {
        usleep(100000);
        retries++;
        if(retries > 10000) {
          LOG("Error: client cannot connect to server side.");
          return;
        }
      }

    } while (incomplete);
  }
  catch(interprocess_exception& e) {
    ERROR("SharedMemoryChannel constructor (client, channel=%s) exception:%s",
          channel_name,
          e.what());
    throw e;
  }

  /* wait for server to signal completed initialization */
  while(*channel_init_!=0xbeeffeed) {
    usleep(1000);
  }
  //  *channel_init_ = 0;// why?
}


/** 
 * Destructor
 * 
 * 
 */
SharedMemoryChannel::~SharedMemoryChannel() {

  using namespace boost::interprocess;

  if(mode_==RESP) {
    /* clean up */
//     segment_->destroy_ptr(c2s_);
//     segment_->destroy_ptr(s2c_);
//     segment_->destroy_ptr(sem_c2s_);
//     segment_->destroy_ptr(msgslab_);
//     segment_->destroy_ptr(channel_init_);

// #ifdef CONFIG_SEM_ON_REPLY
//     segment_->destroy_ptr(sem_s2c_);
// #endif
    
    shared_memory_object::remove(channel_name_.c_str());
  }

  delete segment_;
}


/** 
 * Synchronously send a message, block and wait for reply. Use same
 * buffer both ways (i.e. zero-copy)
 * 
 * @param data Data buffer
 * @param len Lenght of data buffer
 */
// int SharedMemoryChannel::sync_zerocopy_send_and_wait(void * data, 
//                                                      size_t len,
//                                                      long msec_timeout) {

//   assert(mode_ == REQ);
//   assert(data);
//   assert(len > 0);
    
//   queue_entry_t send;
//   send.offset = segment_->get_handle_from_address(data); // convert to offset
//   send.size = len;
//   while(!c2s_->push(send)) {
//     push_retries_++;
//     cpu_relax();
//   }
//   if(sem_c2s_->post()==EINTR)
//     return EINTR;
    
// #ifdef CONFIG_SEM_ON_REPLY
//   {
//     int rc;
//     if(msec_timeout != -1) {
//       if((rc = sem_s2c_->timed_wait(msec_timeout*1000))!=0) {
//         return rc;
//       }
//     }
//     else {
//       if((rc = sem_s2c_->wait())!=0)
//         return rc;
//     }
//   }
// #endif

//   queue_entry_t resp;
//   while(!s2c_->pop(resp)) cpu_relax();

//   assert(resp.offset == send.offset);
//   return 0;
// }

int SharedMemoryChannel::sync_zerocopy_send(void * data, size_t len) {

  assert(mode_ == REQ);
  assert(data);
  assert(len > 0);
    
  queue_entry_t send;
  send.offset = segment_->get_handle_from_address(data); // convert to offset
  send.size = len;
  while(!c2s_->push(send)) {
    push_retries_++;
    cpu_relax();
  }
  if(sem_c2s_->post()==EINTR)  /* wake up receiver */
    return EINTR;

  return 0;
}


/** 
 * Wait for a message, then reply in the same buffer (i.e. zero-buffer)
 * 
 * @param callback Function to call to handle message
 */
int SharedMemoryChannel::sync_zerocopy_wait_and_respond(void (*callback)(void *, size_t)) {

  assert(mode_ == RESP);
    
  queue_entry_t r;
  if(sem_c2s_->wait()==EINTR) 
    return EINTR;

  int retries = 0;
  while(!c2s_->pop(r)) {
    cpu_relax();
    if(retries++ > kPopMaxRetries) {
      PDBG("sync_zerocopy_wait_and_respond could not pop after signalling");
      assert(0);
      return -1;
    }
  }

  void * p = segment_->get_address_from_handle(r.offset);
  assert(p);

  /* callback to handle message */
  callback(p, r.size);
    
  /* send same thing back */
  while(!s2c_->push(r)) {
    cpu_relax();
    push_retries_++;
  }

#ifdef CONFIG_SEM_ON_REPLY
  if(sem_s2c_->post()==EINTR) 
    return EINTR;
#endif
  return 0;
}


/** 
 * Synchronously send message, block and wait for reply.  Use a send-allocated 
 * buffer for the result.
 * 
 * @param data Data to send.
 * @param len Length of data to send
 * @param out_data Reply data. Must be freed by caller.
 * @param out_len Length of reply data.
 */
int SharedMemoryChannel::sync_zerocopy_send_and_wait(void * data, 
                                                     size_t len, 
                                                     void **out_data, 
                                                     size_t* out_len,
                                                     long msec_timeout) {

  assert(mode_ == REQ);
  assert(data);
  assert(len > 0);
  assert(c2s_);   

  queue_entry_t send;
  send.offset = segment_->get_handle_from_address(data); // convert to offset
  send.size = len;
  while(!c2s_->push(send)) {
    push_retries_++;
    cpu_relax();
  }

  if(sem_c2s_->post()==EINTR)
    return EINTR;

  /* wait for reply */
#ifdef CONFIG_SEM_ON_REPLY
  {
    int rc;
    if(msec_timeout != -1) {
      if((rc = sem_s2c_->timed_wait(msec_timeout*1000))!=0) {
        return rc;
      }
    }
    else {
      if((rc = sem_s2c_->wait())!=0)
        return rc;
    }
  }
#endif
  queue_entry_t resp;

  int retries = 0;
  while(!s2c_->pop(resp)) {
    cpu_relax();
    if(retries++ > kPopMaxRetries) {
      PDBG("sync_zerocopy_send_and_wait could not pop after signalling");
      assert(0);
      return -1;
    }
  }

  *out_data = segment_->get_address_from_handle(resp.offset);
  *out_len = resp.size;

  return 0;
}


/** 
 * Wait for a message, then reply in the same buffer (i.e. zero-buffer)
 * 
 * @param callback Function to call to handle message
 */
int SharedMemoryChannel::sync_zerocopy_wait_and_respond(void (*callback)(void *, size_t, void**, size_t*)) {

  assert(mode_ == RESP);
  assert(sem_c2s_);
  assert(s2c_);

  queue_entry_t r;

  if(sem_c2s_->wait() == EINTR) 
    return EINTR;

  int retries = 0;
  while(!c2s_->pop(r)) {
    cpu_relax();
    if(retries++ > kPopMaxRetries) {
      PDBG("sync_zerocopy_wait_and_respond could not pop after signalling");
      assert(0);
      return -1;
    }
  }

  void * p = segment_->get_address_from_handle(r.offset);
  assert(p);

  queue_entry_t response;
  void * resp = NULL;

  /* callback to handle message */
  callback(p, r.size, &resp, &response.size);
  r.offset = segment_->get_handle_from_address(resp);

  /* send same thing back */
  while(!s2c_->push(r)) {
    cpu_relax();
    push_retries_++;
  }

#ifdef CONFIG_SEM_ON_REPLY
  if(sem_s2c_->post()==EINTR)
    return EINTR;
#endif
  return 0;
}

int SharedMemoryChannel::sync_zerocopy_wait(void ** data, size_t * len) {
  assert(data);
  assert(len);
  assert(sem_c2s_);
    
  queue_entry_t r;
  if(sem_c2s_->wait()==EINTR)
    return EINTR;

  int retries = 0;
  while(!c2s_->pop(r)) {
    cpu_relax();
    if(retries++ > kPopMaxRetries) {
      PDBG("sync_zerocopy_wait could not pop after signalling");
      assert(0);
      return -1;
    }
  }

  *data = segment_->get_address_from_handle(r.offset);
  *len = r.size;

  return 0;
}

int SharedMemoryChannel::async_zerocopy_respond(void * data, size_t len) {    
  assert(data);
  assert(len > 0);
  queue_entry_t response;

  response.offset = segment_->get_handle_from_address(data);
  response.size = len;

  /* send same thing back */
  while(!s2c_->push(response)) {
    cpu_relax();
    push_retries_++;
  }
#ifdef CONFIG_SEM_ON_REPLY
  if(sem_s2c_->post()==EINTR)
    return EINTR;
#endif

  return 0;
}

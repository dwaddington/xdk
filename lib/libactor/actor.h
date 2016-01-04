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

#ifndef __ACTOR_H__
#define __ACTOR_H__

#include <unistd.h>
#include <sstream>
#include <string>
#include <signal.h>
#include <errno.h>
#include <mutex>
#include <vector>

#include <boost/thread.hpp>
#include <common/dump_utils.h>
#include <common/cpu_mask.h>
#include <common/sync.h>
#include <google/protobuf/message.h>

#include "actor.pb.h"
#include "channel.h"
#include "connection.h"

#include "network_channel.h"
#include "actor_debug.h"
#include "thread_group.h"

#define CHANNEL_MEMORY_SIZE KB(32)

extern void actor_sig_handler(int sig);
void termination_handler(int sig);

class Actor
{
private:

  ThreadGroup                  session_thread_group_;  /* collection of session threads */
  boost::thread *              bootstrap_thread_;      /* bootstrap IPC thread */
  volatile bool                thread_exit_;           /* actor-wide exit flag which should be checked by session threads */
  uint32_t                     session_id_;            /* counter used for creating session ids */
  cpu_mask_t                   cpu_mask_;              /* cpu mask for all session threads */

public:

  Actor(uint64_t cpu_affinity_mask = 0);

  ~Actor();

  void initialize_actor(const char * endpoint);
  std::string create_endpoint(const char * endpoint);
  void exit_session_threads();
  void wait_for_exit();


  /////////////////////////////////////////////////////////////////////////
  // VIRTUAL FUNCTIONS
  /////////////////////////////////////////////////////////////////////////

  /** 
   * Function for the user-defined actor to override.  This message handler
   * is called by ALL of the session threads and therefore must be made
   * thread safe.
   * 
   * @param type Message type identifier
   * @param msg Message data
   * @param len Message data len
   * 
   * @return 
   */
  virtual ::google::protobuf::Message * message_handler(uint32_t type, 
                                                        void * msg, 
                                                        int len,
                                                        uint32_t session_id)=0;

  /** 
   * Override this to be called for each new session thread.  This can
   * be used to set up state between a session_id and some service
   * (e.g., queue).
   * 
   * @param session_id Session identifier
   */
  virtual void init_session_thread(uint32_t session_id) {}

  /** 
   * Override to provide thread initialization functionality.
   * 
   */
  virtual void init_loop_thread() {}

  /** 
   * Override to provide thread cleanup functionality.
   * 
   */
  virtual void cleanup_loop_thread() {}


private:

  std::string create_session_thread(const char * endpoint, uint32_t session_id = 0);
  void set_thread_affinity();
  void exit_bootstrap_thread();

  void session_thread_entry(std::string session_name, 
                            Channel * channel, 
                            uint32_t session_id);

  void network_session_thread_entry(std::string session_name, 
                                    NetworkChannel * channel, 
                                    uint32_t session_id);
  /** 
   * Entry point for the bootstrap thread which is repsonsible for creating
   * new sessions.
   * 
   * @param endpoint Name of bootstrap end point
   */
  void bootstrap_thread_entry(const char * endpoint);

};




#endif // __ACTOR_H__

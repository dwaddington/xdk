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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "channel.h"
#include "actor.pb.h"
#include "actor_debug.h"

class Session;

/** 
 * Boot-strap connection for an Actor.  This is what provides access to the
 * create_session API.
 * 
 * @param connection_url Connection endpoint URL (any string form)
 */
class Connection 
{
  friend class Session;

private:
  Channel * channel_;
  //  uint32_t  counting_session_id_; /* used if not explicitly given in create_session */
  static const unsigned CTOR_TIMEOUT_IN_SECONDS = 5;

public:

  /** 
   * Constructor
   * 
   * @param connection_url Connection URL 
   */
  Connection(const char * connection_url);

  /** 
   * Destructor
   * 
   */
  ~Connection();

  /** 
   * Used to create a new session. If you don't specify a session id 
   * (which will be passed to the server side message handler) then
   * an incrementing value is used.
   * 
   * @param base_end_point Optional unique identifier for the session
   * @param request_id Optional session identifier (otherwise incrementing value is used)
   * 
   * @return Pointer to new session instance
   */
  Session * create_session(const char * base_end_point = NULL, uint32_t request_id = 0);

  /** 
   * Signal the actor to shut down.
   * 
   */
  void signal_actor_shutdown();

protected:

  Channel * channel() const {
    return channel_;
  }
};


/** 
 * A 'Session' must be created for each client thread.  Sessions cannot be shared
 * amongst threads.
 * 
 * @param conn Parent connection
 */
class Session
{
private:
  std::string   endpoint_;
  Channel *     channel_;
  uint32_t      session_id_;
  
public:

  /** 
   * Constructor
   * 
   * @param conn 
   */
  Session(Connection& conn, const char * base_end_point = NULL, uint32_t requested_id = 0);


  /** 
   * Destructor
   * 
   */
  ~Session();

  /** 
   * Send a message, do not wait for reply.
   * 
   * @param msg_to_send 
   * 
   * @return 
   */
  int send_msg_oneway(::google::protobuf::Message& msg_to_send);

  /** 
   * Send a message and wait for a reply.
   * 
   * @param msg_to_send 
   * @param msg_reply 
   * 
   * @return 
   */
  int send_msg(::google::protobuf::Message& msg_to_send,
               ::google::protobuf::Message& msg_reply);




  /** 
   * Get the session id that was passed into the create_session call
   * 
   * 
   * @return 
   */
  inline uint32_t session_id() const {
    return session_id_;
  }

};

#endif

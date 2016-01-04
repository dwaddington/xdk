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

#ifndef __NETWORK_CHANNEL_H__
#define __NETWORK_CHANNEL_H__

#include <common/utils.h>
#include <string>
#include <zmq.hpp> /* apt-get install libzmq3-dev */

/** 
 * NetworkChannel class supports TCP-based message passing (i.e. RPC) over
 * ZeroMQ.
 *
 * @author Daniel Waddington
 */
class NetworkChannel {

public:

  static const int kNumIoThreads = 1;

  static const int kMaxMessageSize = KB(32);

  /** 
   * Constructor.
   * 
   * @param session_name 
   * @param endpoint ZMQ end point (e.g., tcp://localhost:5555, tcp://\*:5432)
   */
  NetworkChannel(std::string& session_name,
                 const char* endpoint);

  /** Destructor.*/
  virtual ~NetworkChannel();

  /** 
   * Synchronously waits for data.
   * 
   * @param data Received data, allocated server-side
   * @param len 
   */
  int sync_wait(void** data, size_t* len);

  /** 
   * Synchronous responds by sending a packet down the connection.
   * 
   * @param resp_data Data to send.
   * @param resp_len Length of data in bytes.
   * @return 0 on success, EINTR on thread interruption.
   */
  int sync_respond(void* resp_data, size_t resp_len);

  /** Closes the session. */
  void close_session();

  /** Allocates a message buffer. */
  inline void* alloc_msg_buff(size_t len) { return ::malloc(len); }

  /** Frees a message buffer. */
  inline void free_msg_buff(void* buf) { ::free(buf); }

private:
  zmq::context_t context_;
  zmq::socket_t* socket_;
  std::string    endpoint_;

};

#endif

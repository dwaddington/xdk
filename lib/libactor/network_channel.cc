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

#include "network_channel.h"
#include "actor.pb.h"
#include "actor_debug.h"

#include <common/logging.h>

NetworkChannel::NetworkChannel(std::string& session_name,
                               const char * endpoint) 
                               : context_(kNumIoThreads),
                                 endpoint_(endpoint),
                                 socket_(NULL)
{
  LOG("new NetworkChannel (session_name=%s, endpoint=%s)",
       session_name.c_str(),
       endpoint);

  assert(endpoint);

  socket_ = new zmq::socket_t(context_, ZMQ_REP);
  int linger = 0;
  socket_->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
  socket_->bind(endpoint_.c_str());
}

NetworkChannel::~NetworkChannel() {
  if (socket_) {
    delete socket_;
  }
}


int NetworkChannel::sync_wait(void** data, size_t* len) {

  // Recv incoming message.
  assert(socket_);
  
  if(!socket_->connected()) {
    socket_->bind(endpoint_.c_str());
  }
  assert(socket_->connected());
      
  void* buffer = this->alloc_msg_buff(kMaxMessageSize);
  assert(buffer);
      
  LOG("waiting on socket.recv (%s) ..", endpoint_.c_str());

  try {
    size_t recvd_bytes = socket_->recv(buffer, kMaxMessageSize);
    assert(recvd_bytes > 0);

    *data = buffer;
    *len = recvd_bytes;
    return 0;
  }
  catch (zmq::error_t e) {
    LOG("socket.recv exception (in NetworkChannel::sync_wait): %s", e.what());
    this->free_msg_buff(buffer);
    return EINTR;
  }

}


int NetworkChannel::sync_respond(void* resp_data, size_t resp_len) {
  assert(resp_data);

  try {
    //  assert(socket_->connected());
    if(socket_->connected()) {
      socket_->send(resp_data, resp_len);
    }

    free_msg_buff(resp_data);
    return 0;
  }
  catch(zmq::error_t e) {
    PLOG("socket.send exception: %s", e.what());
    return EINTR;
  }
}

void NetworkChannel::close_session() {

  // socket_->close();
  // socket_->unbind(endpoint_.c_str());
  delete socket_;

  socket_ = new zmq::socket_t(context_, ZMQ_REP);
  int linger = 0;
  socket_->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
  assert(socket_);

  socket_->bind(endpoint_.c_str());
}

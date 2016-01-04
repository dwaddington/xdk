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

#include "connection.h"
#include "actor_debug.h"


/** 
 * Constructor
 * 
 * @param connection_url 
 */
Connection::Connection(const char * connection_url) {
  channel_ = NULL;

  /* TODO: put eventual time out */
  unsigned remaining_retries = CTOR_TIMEOUT_IN_SECONDS * 3;

  while (channel_ == NULL) {
    try {
      channel_ = new SharedMemoryChannel(connection_url);
    }
    catch (boost::interprocess::interprocess_exception& e){ 
      channel_ = NULL;
    }
    usleep(100000);
    if(remaining_retries == 0)
      assert(0); //TODO

    remaining_retries--;
  }

  assert(channel_);
}


Connection::~Connection() {

  if(channel_) 
    delete channel_;
}


/** 
 * Signals the remote actor to shutdown.  We may need a synchronous version of
 * this in the future.
 * 
 */
void Connection::signal_actor_shutdown()
{
  /* send shutdown signal */
  using namespace ActorProtocol;
  LOG("Connection dtor (client-side)");

  ActorRequest cmd;
  cmd.set_type(ACTOR_REQ);
  cmd.set_operation(ActorRequest::ACTOR_SHUTDOWN);

  /* create message */
  assert(channel());
  int msg_size = cmd.ByteSize();
  void * msg_buff = channel_->alloc_msg_buff(msg_size);

  if(!cmd.SerializeToArray(msg_buff,msg_size)) {
    ERROR("send_msg serialization failed");
    assert(0);
  }
    
  /* one-way */
  channel_->sync_zerocopy_send(msg_buff, msg_size);

  assert(channel_);
  delete channel_;
  channel_ = NULL;
}

Session * Connection::create_session(const char * base_end_point, uint32_t request_id) {
  Session * s;
  try {
    s = new Session(*this, base_end_point, request_id);
  }
  catch(...) {
    return NULL;
  }
  return s;
}


/////////////////////////////////////////////////////////////////////////////////////////

#define SESSION_RPC_TIMEOUT_MSEC 3000

/** 
 * Constructor
 * 
 * @param conn 
 */
Session::Session(Connection& conn, 
                 const char * base_end_point, 
                 uint32_t requested_id) {
 
  using namespace ActorProtocol;
  LOG("(Session::ctor) new session (client-side)");

  ActorRequest cmd;
  cmd.set_type(ACTOR_REQ);
  cmd.set_operation(ActorRequest::CREATE_SESSION);
  if(base_end_point)
    cmd.set_base_endpoint(base_end_point);

  if(requested_id > 0)
    cmd.set_request_id(requested_id);

  /* send create session message */
  assert(conn.channel());
  int msg_size = cmd.ByteSize();
  void * msg_buff = conn.channel()->alloc_msg_buff(msg_size);
  assert(msg_buff);

  if(!cmd.SerializeToArray(msg_buff,msg_size)) {
    ERROR("send_msg serialization failed");
    assert(0);
  }
    
  void * reply_msg;
  size_t reply_msg_len;
  ActorReply reply;

  int rc = conn.channel()->sync_zerocopy_send_and_wait(msg_buff, 
                                                       msg_size, 
                                                       &reply_msg, 
                                                       &reply_msg_len,
                                                       SESSION_RPC_TIMEOUT_MSEC);

  if(rc == ETIMEDOUT) {
    ERROR("connection reply timed out");
    assert(0); // TODO
  }

  if(!reply.ParseFromArray(reply_msg, reply_msg_len)) {
    ERROR("bad actor reply.");
    assert(0);
  }

  LOG("got session %s",reply.session_endpoint().c_str());
  endpoint_ = reply.session_endpoint();

  /* now create the channel to the new endpoint */
  channel_ = new SharedMemoryChannel(endpoint_.c_str());
  assert(channel_);
}


/**
 * Destructor
 */
Session::~Session() {

  LOG("destroying session");

  using namespace ActorProtocol;

  /* send close session signal to other side */
  ActorRequest cmd;
  cmd.set_type(ACTOR_REQ);
  cmd.set_operation(ActorRequest::CLOSE_SESSION);
  send_msg_oneway(cmd);

  /* clean up */
  delete channel_;
}

/** 
 * Send a message, do not wait for reply.
 * 
 * @param msg_to_send 
 * 
 * @return 
 */
int Session::send_msg_oneway(::google::protobuf::Message& msg_to_send) {

  /* send */
  assert(channel_);
  int msg_size = msg_to_send.ByteSize();
  void * msg_buff = channel_->alloc_msg_buff(msg_size);

  if(!msg_to_send.SerializeToArray(msg_buff,msg_size)) {
    ERROR("send_msg serialization failed");
    assert(0);
  }
    
  channel_->sync_zerocopy_send(msg_buff,msg_size);
  return 0;
}


/** 
 * Send a message and wait for a reply.
 * 
 * @param msg_to_send 
 * @param msg_reply 
 * 
 * @return 
 */
int Session::send_msg(::google::protobuf::Message& msg_to_send,
                      ::google::protobuf::Message& msg_reply) {

  /* send */
  assert(channel_);

  int msg_size = msg_to_send.ByteSize();
  assert(msg_size > 0);

  void * msg_buff = channel_->alloc_msg_buff(msg_size);

  if(!msg_to_send.SerializeToArray(msg_buff,msg_size)) {
    ERROR("send_msg serialization failed");
    assert(0);
  }
    
  void * reply_msg;
  size_t reply_msg_len;

  channel_->sync_zerocopy_send_and_wait(msg_buff, msg_size, &reply_msg, &reply_msg_len);

  if(!msg_reply.ParseFromArray(reply_msg, reply_msg_len)) {
    ERROR("bad send_msg parse from array");
    return -1;
  }

  channel_->free_msg_buff(reply_msg);
  return 0;
}


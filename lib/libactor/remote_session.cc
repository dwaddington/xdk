/*
  DataHawk

  Copyright (c) 2015, Samsung Electronics Co., Ltd. All rights reserved.

  This software is confidential and proprietary. The software may not
  be used, reproduced, modified, or distributed without the prior 
  written consent from Samsung Electronics Co., Ltd.

  This software is for INTERNAL DISTRIBUTION ONLY.  The code
  is subject to one or more open source licenses.

*/

/*
 * @author Daniel Waddington
 */

#include <common/logging.h>
#include <common/types.h>
#include <common/errors.h>
#include <common/dump_utils.h>

#include "remote_session.h"

/** 
 * Constructor
 * 
 * @param endpoint URL to connect to in ZeroMQ form (e.g., tcp://127.0.0.1:4567)
 */
RemoteSession::RemoteSession(const char * endpoint) :
  context_(kNumIoThreads),
  endpoint_(endpoint)
{
  LOG("Remote session endpoint (%s)", endpoint);

  socket_ = new zmq::socket_t(context_, ZMQ_REQ);

  try {
    socket_->connect(endpoint);
    LOG("socketed connect OK to endpoint (%s)", endpoint);
  }
  catch(zmq::error_t e) {
    LOG("remote session thread interrupted on connect");
  }
}

/** 
 * Destructor
 * 
 */
RemoteSession::~RemoteSession() {

  using namespace ActorProtocol;

  socket_->close();
  delete socket_;
}


/** 
 * Synchronously send a message to the remote party and block for a reply.
 * 
 * @param msg_to_send Google protobuf based message to send.
 * @param msg_reply Google protobuf message to put the reply in.
 * 
 * @return 0 on success. EINTR on interruption, E_FAIL otherwise.
 */
int RemoteSession::send_msg(::google::protobuf::Message& msg_to_send,
                            ::google::protobuf::Message& msg_reply)
{
  int msg_size = msg_to_send.ByteSize();
  assert(msg_size > 0);

  void * msg_buff = ::malloc(msg_size);

  if(!msg_to_send.SerializeToArray(msg_buff,msg_size)) {
    ERROR("send_msg serialization failed");
    assert(0);
    return E_FAIL;
  }
    
  void * reply_msg = malloc(kMaxMessageSize);
  size_t reply_msg_len =  kMaxMessageSize;

  /* send message */
  try {
    socket_->send(msg_buff, msg_size);
  }
  catch(zmq::error_t e) {
    return EINTR;
  }
  
  /* wait for reply */
  size_t r;
  try {
    r = socket_->recv(reply_msg, reply_msg_len);
  }
  catch(zmq::error_t e) {
    return EINTR;
  } 

  if(!msg_reply.ParseFromArray(reply_msg, r)) {
    ERROR("bad send_msg parse from array");
    ::free(msg_buff);
    return E_FAIL;
  }

  ::free(msg_buff);
  return 0;
}

/** 
 * Signal a remote actor to shutdown and close all sessions.
 * 
 */
void RemoteSession::signal_actor_shutdown() {

  /* send shutdown signal */
  using namespace ActorProtocol;

  ActorRequest cmd;
  cmd.set_type(ACTOR_REQ);
  cmd.set_operation(ActorRequest::ACTOR_SHUTDOWN);

  /* create message */
  int msg_size = cmd.ByteSize();
  void * msg_buff = ::malloc(msg_size);  

  if(!cmd.SerializeToArray(msg_buff,msg_size)) {
    ERROR("send_msg serialization failed");
    assert(0);
  }

  //  hexdump(msg_buff, msg_size);
    
  /* one-way */
  socket_->send(msg_buff, msg_size);

  ::free(msg_buff);
}

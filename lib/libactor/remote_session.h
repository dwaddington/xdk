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

#ifndef __REMOTE_CONNECTION__
#define __REMOTE_CONNECTION__

#include <common/utils.h>
#include <zmq.hpp>

#include "actor.pb.h"
#include "actor_debug.h"

/** 
 * RemoteSession class is used to create a TCP connection to 
 * a remote actor.
 * 
 */
class RemoteSession
{
private:
  static const int kNumIoThreads = 1;
  static const int kMaxMessageSize = KB(128);
 
  zmq::context_t context_;
  zmq::socket_t  * socket_;
  std::string    endpoint_;

public:

  /** 
   * Constructor
   * 
   * @param endpoint URL to connect to (e.g., tcp://myserver.org:5555)
   */
  RemoteSession(const char * endpoint);

  ~RemoteSession();

  int send_msg(::google::protobuf::Message& msg_to_send,
               ::google::protobuf::Message& msg_reply);

  void signal_actor_shutdown();
  
};

#endif // __REMOTE_CONNECTION__

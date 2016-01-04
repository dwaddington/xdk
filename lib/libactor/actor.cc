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

#include <stdlib.h>
#include <common/utils.h>

#include "actor.h"

/** 
 * SIGUSR1 handle for interrupting threads for termination.
 * 
 * @param sig SIGUSR1
 */
void termination_handler(int sig) {
}


/** 
 * Construct Actor instance with a specific CPU affinity
 * 
 * @param cpu_affinity_mask Affinity mask to use for both bootstrap and session threads
 */
Actor::Actor(uint64_t cpu_affinity_mask)  : thread_exit_(false), session_id_(1) {
  
  if(cpu_affinity_mask > 0) {
    cpu_mask_.set_mask(cpu_affinity_mask);                              
  }
}


/** 
 * Destructor
 * 
 */
Actor::~Actor() {

  /* exit bootstrap thread */
  exit_bootstrap_thread();
 
  /* clean up session threads */
  exit_session_threads();
}


/** 
 * Signal bootstrap thread to exit, join and then delete the instance.
 * 
 */
void Actor::exit_bootstrap_thread() {
  /* signal bootstrap thread to exit */
  if(bootstrap_thread_) {
    if(bootstrap_thread_->native_handle())
      pthread_kill(bootstrap_thread_->native_handle(), SIGUSR1);
    bootstrap_thread_->join();
    delete bootstrap_thread_;
    bootstrap_thread_ = NULL;
  }
}


/** 
 * Entry point for bootstrap thread.
 * 
 * @param endpoint End point URL of which the bootstrap thread should create a shared memory channel
 */
void Actor::bootstrap_thread_entry(const char * endpoint) {

  /* set affinity of thread if needed */
  set_thread_affinity();

  /* set up signal handling */
  struct sigaction action;
  action.sa_handler = termination_handler;
  sigemptyset( &action.sa_mask );
  action.sa_flags = 0;
  sigaction(SIGUSR1, &action, NULL);

  /* create bootstrap channel */
  Channel * bootstrap_channel = new SharedMemoryChannel(endpoint, KB(64));

  ActorProtocol::ActorRequest cmd;
  ActorProtocol::ActorReply reply;

  while(!thread_exit_) {

    void * msg = NULL;
    size_t msg_len;

    /* wait for message */
    LOG("bootstrap-thread waiting for message.");

    if(bootstrap_channel->sync_zerocopy_wait(&msg, &msg_len)==EINTR) {
      LOG("bootstrap thread exiting after EINTR");
      thread_exit_ = true;
      break;
    }

    if(!cmd.ParseFromArray(msg,msg_len)) {
      PERR("bad bootstrap message.");
      continue;
    }
     
    switch(cmd.operation()) {
    case ActorProtocol::ActorRequest::CREATE_SESSION: {
      LOG("received CREATE_SESSION actor command.");

      /* create session */
      std::string new_session_name;
      if(cmd.has_base_endpoint())
        /* if base endpoint defined by client, use it */
        new_session_name = create_session_thread(cmd.base_endpoint().c_str());
      else
        /* otherwise use a default end point id */
        new_session_name = create_session_thread(endpoint);

      /* send reply */
      reply.set_type(ACTOR_REPLY);
      reply.set_session_endpoint(new_session_name);
      size_t resp_len = reply.ByteSize();
      void * resp_data = bootstrap_channel->alloc_msg_buff(resp_len);

      reply.SerializeToArray(resp_data,resp_len);
      bootstrap_channel->async_zerocopy_respond(resp_data,resp_len);
      break;
    }
    case ActorProtocol::ActorRequest::ACTOR_SHUTDOWN: {
      exit_session_threads(); /* make sure session threads are shutdown */
      thread_exit_ = true; /* exit this thread */
      break;
    }
    default:
      PERR("unknown actor command.");
      continue;
    }
  }

  LOG("actor (%p) bootstrap thread exited.", (void*) this);

  delete bootstrap_channel;
}


/** 
 * Session thread entry point.  When a new session is created a new
 * channel is established for the session name.
 * 
 * @param session_name Name of session
 * @param channel Channel instance to which this session thread is assigned
 */
void Actor::session_thread_entry(std::string session_name, 
                                 Channel * channel, 
                                 uint32_t session_id) {
    
  using namespace ActorProtocol;

  /* set affinity of thread if configured */
  set_thread_affinity();

  /* set up signal handling */
  struct sigaction action;
  action.sa_handler = termination_handler;
  sigemptyset( &action.sa_mask );
  action.sa_flags = 0;
  sigaction(SIGUSR1, &action, NULL);

  assert(channel);
  LOG("session thread for session (%s)",session_name.c_str());

  /* call init for session */
  init_session_thread(session_id);

  Unknown ap_msg;
  ActorRequest cmd;
  bool session_exit = false;

  while(!thread_exit_) {

    void * msg = NULL;
    size_t msg_len;

    LOG("waiting for message from channel..");

    /* wait for message */
    if(channel->sync_zerocopy_wait(&msg, &msg_len)==EINTR) {
      LOG("session thread exiting after EINTR");
      thread_exit_ = true;
      break;
    }

    LOG("session thread (%s) got message (len=%ld)",
        session_name.c_str(),
        msg_len);

    if(!ap_msg.ParsePartialFromArray(msg,msg_len)) {
      PERR("bad message.");
      assert(0);
      return;
    }

    if(ap_msg.type()==ACTOR_REQ) {
      cmd.ParseFromArray(msg,msg_len);
      if(cmd.operation() == ActorRequest::CLOSE_SESSION) {
        break;
      }
      else {
        PDBG("unhandled ActorRequest message type in actor.");
      }
    }

    LOG("calling message handler.");
      
    /* UP-call user-defined handler */
    ::google::protobuf::Message * response = 
        message_handler(ap_msg.type(), msg, msg_len, session_id);

    channel->free_msg_buff(msg);

    /* conditionally send reply */
    if(response) {

      LOG("sending response.");

      /* sent buffer must be large enough for response */
      size_t resp_len = response->ByteSize();
      void * resp_data = channel->alloc_msg_buff(resp_len);

      response->SerializeToArray(resp_data,resp_len);

      channel->async_zerocopy_respond(resp_data,resp_len);

      delete response;
    }      
    else {
      LOG("no response will be sent.");
    }
  }

  /* clean up */
  delete channel;

  LOG("IPC session thread (%s) exiting",session_name.c_str());
}


/** 
 * Entry point for session threads that are for network sessions
 * 
 * @param session_name Name of session
 * @param channel Network channel to which this thread is assigned
 * @param session_id Session identifier
 */
void Actor::network_session_thread_entry(std::string session_name, 
                                         NetworkChannel * channel, 
                                         uint32_t session_id) 
{
  using namespace ActorProtocol;

  /* set affinity of thread if configured */
  set_thread_affinity();

  assert(channel);
  LOG("session thread for session (%s)",session_name.c_str());

  /* call init for session */
  init_session_thread(session_id);

  Unknown ap_msg;
  ActorRequest cmd;

  while(!thread_exit_) {

    void * msg = NULL;
    size_t msg_len;

    LOG("waiting for message from network..");

    /* wait for message */
    if(channel->sync_wait(&msg, &msg_len) == EINTR) {
      // if(msg)
      //   channel->free_msg_buff(msg);
      continue;
    }

    LOG("net session thread (%s) got message (len=%ld)",
        session_name.c_str(),
        msg_len);
    
    assert(msg);
    if(!ap_msg.ParsePartialFromArray(msg,msg_len)) {
      PERR("bad message.");
      assert(0);
      return;
    }

    if(ap_msg.type()==ACTOR_REQ) {
      cmd.ParseFromArray(msg,msg_len);
      switch(cmd.operation()) {
      case ActorRequest::CLOSE_SESSION: 
        {
          channel->free_msg_buff(msg);
          continue; // no reply
        }
      case ActorProtocol::ActorRequest::ACTOR_SHUTDOWN:
        {
          LOG("actor received ACTOR_SHUTDOWN from remote connection");
          exit_bootstrap_thread();
          exit_session_threads();
          thread_exit_ = true; /* exit this thread */
          channel->free_msg_buff(msg);
          continue;
        }
      default: 
        {
          PDBG("unhandled ActorRequest message type (%d) in actor.", ap_msg.type());
          hexdump(msg,msg_len);
        }
      }
    }

    LOG("calling message handler.");
      
    /* UP-call user-defined handler */
    ::google::protobuf::Message * response = 
        message_handler(ap_msg.type(), msg, msg_len, session_id);

    channel->free_msg_buff(msg);

    /* conditionally send reply */
    if(response) {
      /* sent buffer must be large enough for response */
      size_t resp_len = response->ByteSize();
      void * resp_data = channel->alloc_msg_buff(resp_len);

      response->SerializeToArray(resp_data,resp_len);
      channel->sync_respond(resp_data,resp_len);

      delete response;
    }

  } // while loop 


  /* clean up */
  delete channel;
  LOG("net session thread (%s) exiting",session_name.c_str());
}


/** 
 * Each session create instantiates a new thread.  Because we are not
 * using a thread pool, the number of sessions does not scale and 
 * therefore you should avoid creating many many sessions.
 * 
 * @param endpoint Name of endpoint from which the session was created
 * @param session_id Session id used in the client-side create_session call
 * 
 * @return Name of new session
 */
std::string Actor::create_session_thread(const char * endpoint, uint32_t session_id) {
  
  if(thread_exit_) 
    assert(0); // called after shutdown?
    
  
  LOG("actor creating session thread (id=%u, endpoint=%s).", 
      session_id, endpoint);

  /* construct session name */
  std::string new_session_name = endpoint;

  uint32_t this_session_id;

  if(session_id > 0) {
    this_session_id = session_id;
  }
  else {
    // TODO: recycle session identifiers
    this_session_id = session_id_;
    session_id_++;
  }
  
  /*-----------*/
  /*    TCP    */
  /*-----------*/
  if(strncmp(endpoint,"tcp://",4)==0) {

    NetworkChannel * channel = new NetworkChannel(new_session_name, endpoint);
      
    /* create thread and add to session thread group */
    session_thread_group_.
      add(new boost::thread(&Actor::network_session_thread_entry,
                            this,new_session_name,channel,this_session_id));      
  }    
  /*---------------*/
  /*  default IPC  */
  /*---------------*/
  else { 

    /* default is to assume this is a IPC shared memory channel -------------------- */
    new_session_name += "_ipc_";
    new_session_name += boost::to_string(session_id_);
      
    /* important to create the channel first synchronously */
    Channel * channel = new SharedMemoryChannel(new_session_name.c_str(), 
                                                CHANNEL_MEMORY_SIZE);
    
    /* create thread and add to session thread group */
    session_thread_group_.
      add(new boost::thread(&Actor::session_thread_entry,
                            this,new_session_name,channel,this_session_id));      
  }

  return new_session_name;
}


/** 
 * Set thread affinity according to saved constructor parameters.  This is 
 * called by the threads themselves.
 * 
 */
void Actor::set_thread_affinity() {

  if(cpu_mask_.is_set()) {
    int rc;
      
    rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), cpu_mask_.cpu_set());
    if(rc != 0) {
      PWRN("unable to set thread affinity (rc=0x%x)",rc);
      cpu_mask_.dump();
    }
  }
}


/** 
 * Initialize boostrap thread and hence start the actor.  Usually called in the actor superclass constructor.
 * 
 * @param url End-point URL (shared memory channel) to bind to
 */
void Actor::initialize_actor(const char * endpoint) {

  /* create bootstrap thread */
  bootstrap_thread_ = 
    new boost::thread(&Actor::bootstrap_thread_entry,this,endpoint);
}


/** 
 * Create a new end point. Called by invocation on actor instance.
 * 
 * @param endpoint URL to attach session to.
 * 
 * @return Name of the endpoint (e.g,. after port allocation)
 */
std::string Actor::create_endpoint(const char * endpoint) {
  LOG("creating endpoint (%s)", endpoint);
  return create_session_thread(endpoint);
}


/** 
 * Flag session threads to exit and then join
 * 
 */
void Actor::exit_session_threads() {

  thread_exit_ = true;
  
  /* I am worried that a thread might get signal interrupted while it is
     holding a lock.  This is why I use unsafe iterator.  The bootstrap
     thread has been called before this.
  */
  session_thread_group_.for_each_unsafe([](boost::thread *t) 
  {
    if(t->get_id() != boost::this_thread::get_id()) { /* don't self kill or join */
      if(t->native_handle())
        pthread_kill(t->native_handle(), SIGUSR1);

      t->join();
    }
  });

  LOG("session threads joined OK.");
}

/** 
 * Join on both bootstrap and session threads
 * 
 */
void Actor::wait_for_exit() {

  session_thread_group_.for_each_unsafe([](boost::thread *t) 
  {
    t->join();
  });

  if(bootstrap_thread_)
    bootstrap_thread_->join();

  LOG("main thread joined bootstrap_thread");
}

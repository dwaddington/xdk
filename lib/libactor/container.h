#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include <stdint.h>
#include <google/protobuf/message.h>
#include <component/base.h>

#include <boost/thread.hpp>
#include <map>
#include <list>
#include <string>

#include "actor.h"
#include "container.pb.h"

class ContainerOperations
{
protected:
  std::list<Component::IBase *>             loaded_components_;
  std::map<Component::IBase *, std::string> locations_;
  std::map<std::string, Component::IBase *> instances_;
  boost::mutex                              lock_;
  //  ::Registry *                      registry_; /* local registry service */

public:

  /** 
   * Constructor
   * 
   */
  ContainerOperations();


  /** 
   * Destructor
   * 
   */
  virtual ~ContainerOperations();
  
  /** 
   * Load a component into the container
   * 
   * @param component_id UUID string for component
   * @param path Path to the DLL
   * 
   * @return S_OK on success
   */
  status_t load_component(std::string instance_id,
                          std::string component_id, 
                          std::string path);

  /** 
   * Perform a pair-wise bind
   * 
   * 
   * @return Number of remaining bindings
   */
  signed pairwise_bind();

  /** 
   * Bind two components uni-directionally.
   * 
   * @param left From component instance
   * @param right To component instance
   * 
   * @return True if binding succeeded.
   */
  signed bind(std::string left, std::string right);

  /** 
   * Bind two components uni-directionally according to a specific role.
   * 
   * @param left From component instance
   * @param right To component instance
   * @param id Role identifier for the binding
   * 
   * @return True if binding succeeded.
   */
  signed specified_bind(std::string left, std::string right, int id);

  /** 
   * Invokes operations on a component instance without compile time
   * information on the component.
   * 
   * @param instance_id Component instance identifier. 
   * @param operation Operation string.
   * @param result Result of operation.
   * 
   * @return S_OK on success.
   */
  status_t invoke(std::string instance_id,
                  std::string operation, 
                  std::string& result);


  /** 
   * Configure the pipeline via an XML configuration file.
   * TODO: add to actor protocol interface
   * 
   * @param xml XML configuration text
   * 
   * @return S_OK on success.
   */
  status_t xml_configure(std::string xml, std::string configuration_name);

  /** 
   * Teardown a pipeline, shutting down components and then deleting them.
   * 
   * 
   * @return S_OK on success, E_FAIL otherwise.
   */
  status_t teardown();

  /** 
   * Start pipeline
   * 
   * 
   * @return 
   */
  status_t start_pipeline();

  /** 
   * Stop pipeline
   * 
   * 
   * @return 
   */
  status_t stop_pipeline();

  /** 
   * Reset pipeline
   * 
   * 
   * @return 
   */
  status_t reset_pipeline();


private:

  /** 
   * Debugging function to show instances (on server console).
   * 
   */
  void dump_table();  

};


/** 
 * Container adds RPC to ContainerOperations
 * 
 * @param endpoint 
 * @param cpu_mask 
 * 
 * @return 
 */
class Container : public ContainerOperations,
                  public Actor
{

public:
  Container(const char * endpoint, uint64_t cpu_mask = 0);
  virtual ~Container();

  /** 
   * Main message handler
   * 
   * @param type 
   * @param data 
   * @param len 
   * 
   * @return 
   */
  ::google::protobuf::Message * 
  message_handler(uint32_t type, void * data, int len, uint32_t session_id);

  /** 
   * Used for classes inheriting Container class to provide custom handling.
   * 
   * @param type 
   * @param data 
   * @param len 
   * @param session_id 
   * 
   * @return 
   */
  virtual ::google::protobuf::Message * 
  custom_message_handler(uint32_t type, void * data, int len, uint32_t session_id) 
  { return NULL; }

};



/** 
 * Helper class for container client
 * 
 * @param container_url URL for container endpoint
 */
class ContainerClient
{
private:
  Connection conn_;
  Session * session_;

public:
  ContainerClient(const char * container_url) : conn_(container_url) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    session_ = conn_.create_session();
  }

  ~ContainerClient() {
    delete session_;
  }

  void signal_actor_shutdown() {
    conn_.signal_actor_shutdown();
  }

  status_t add_component(const char * id, const char * component_id, const char * path)
  { /* request that a component be added to the container */
    using namespace ContainerProtocol;
    
    ContainerProtocol::Request req;
    ContainerProtocol::Reply rep;
    
    req.set_type(CONTAINER_REQ);
    req.set_op(Request::ADD_COMPONENT);
    req.set_component_id(component_id);
    req.set_instance_id(id);
    req.set_path(path);
    
    session_->send_msg(req, rep);

    return rep.status();
  }

  int pairwise_bind() {
    using namespace ContainerProtocol;
    
    ContainerProtocol::Request req;
    ContainerProtocol::Reply rep;
    
    req.set_type(CONTAINER_REQ);
    req.set_op(Request::PAIRWISE_BIND);
    
    session_->send_msg(req, rep);

    return rep.remaining();
  }
};

#endif

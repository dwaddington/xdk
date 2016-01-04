#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/thread/lock_guard.hpp>
#include <boost/regex.hpp>
#include <common/logging.h>
#include <common/cycles.h>
#include <string>
#include <tinyxml.h>
#include <base64.h>

#include "container.h"
#include "actor.h"
#include "container.pb.h"

/** 
 * Constructor
 * 
 */
Container::Container(const char * endpoint, uint64_t cpu_mask) : Actor(cpu_mask) {
  initialize_actor(endpoint);
}

/** 
 * Destructor
 * 
 */
Container::~Container() {
}


/** 
 * Main protocol handler
 * 
 * @param type 
 * @param data 
 * @param len 
 * @param session_id 
 * 
 * @return 
 */
::google::protobuf::Message * 
Container::
message_handler(uint32_t type, void * data, int len, uint32_t session_id) {
  
  boost::lock_guard<boost::mutex> guard(lock_);

  ContainerProtocol::Reply * response = new ContainerProtocol::Reply();
  response->set_type(CONTAINER_REPLY);

  switch(type) {
  case CONTAINER_REQ: 
    {
      ContainerProtocol::Request request;
      if(!request.ParseFromArray(data,len)) {
        PERR("unexpected condition parsing ContainerProtocol::Request");
        response->set_status(E_FAIL);
        break;
      }

      if(!request.has_op())
        PERR("bad protocol; no op");

      PLOG("opcode=%d",request.op());

      switch(request.op()) {
      case ContainerProtocol::Request::CONFIGURE_PIPELINE:
        {
          PLOG("Received CONFIGURE_PIPELINE \n %s\n", request.xml().c_str());
          response->set_status(xml_configure(request.xml().c_str(),""));
          return response;
        }
      case ContainerProtocol::Request::TEARDOWN_PIPELINE:
        {
          PLOG("Received TEARDOWN_PIPELINE");
          response->set_status(teardown());
          return response;
        }
      case ContainerProtocol::Request::START_PIPELINE:
        {
          PLOG("Received START_PIPELINE");
          response->set_status(start_pipeline());
          return response;
        }
      case ContainerProtocol::Request::STOP_PIPELINE:
        {
          PLOG("Received STOP_PIPELINE");
          response->set_status(stop_pipeline());
          return response;
        }
      case ContainerProtocol::Request::RESET_PIPELINE:
        {
          PLOG("Received RESET_PIPELINE");
          response->set_status(reset_pipeline());
          return response;
        }
      case ContainerProtocol::Request::INVOKE_COMPONENT:
        {
          PLOG("Received INVOKE_COMPONENT");
          if(!request.has_operation()) {
            PERR("bad protocol: INVOKE_COMPONENT must have operation");
            response->set_status(E_FAIL);
            return response;
          }

          if(!request.has_instance_id()) {
            PERR("bad protocol: INVOKE_COMPONENT without instance id");
            response->set_status(E_FAIL);
            return response;
          }
          
          std::string res;          
          /* make invocation on specific component instance */
          response->set_status(invoke(request.instance_id(), 
                                      request.operation(), res));
          response->set_operation_result(res);
          return response;
        }
      case ContainerProtocol::Request::ADD_COMPONENT:
        {
          PLOG("Received ADD_COMPONENT request");

          std::string path = "";

          if(!request.has_instance_id()) {
            PERR("bad protocol: ADD_COMPONENT without instance id");
            response->set_status(E_FAIL);
            return response;
          }

          if(request.has_path()) {
            path = request.path(); 
          }
          response->set_status(load_component(request.instance_id(),
                                              request.component_id(),
                                              path));
          return response;
        }
      case ContainerProtocol::Request::PAIRWISE_BIND:
        {
          PLOG("Received PAIRWISE_BIND request");
          signed remaining = pairwise_bind();
          response->set_remaining(remaining);
          if(remaining < 0)
            response->set_status(remaining);
          else
            response->set_status(0);

          return response;
        }
      default:
        {
          PERR("bad protocol; unrecognized operation");
        }
      }
      break;
    } // end CONTAINER_REQ    
  default:
    /* call customer handler */
    ::google::protobuf::Message * response;
    response = custom_message_handler(type, data, len, session_id);
    if(response == NULL) {
      ContainerProtocol::Reply * error_response = new ContainerProtocol::Reply;
      error_response->set_status(E_FAIL);
      return error_response;
    }

    return response;
  }

  return NULL;
}



/** 
 * ContainerOperations
 * 
 */
ContainerOperations::
ContainerOperations() {
  // TODO: open registry
}

ContainerOperations::
~ContainerOperations() {
}



/** 
 * Load a component into the container.
 * 
 * @param instance_id Unique identifier used to manage the instance
 * @param component_id Universal identifier of the component 
 * @param path Path of the component's dynamically linked library.  If path string is empty, assume not specified.
 * 
 * @return S_OK on success.  E_NOT_FOUND missing file.
 */
status_t 
ContainerOperations::
load_component(std::string instance_id,
               std::string component_id, 
               std::string path)
{
  PLOG("container received CONTAINER_REQ message.");      
  PLOG("cid=%s",component_id.c_str());
  PLOG("path=%s",path.c_str());

  assert(!component_id.empty());
  assert(!path.empty());

  if(instances_.find(instance_id) != instances_.end()) {
    PWRN("instance identifier already taken");
    return E_INVAL;
  }

  /* check path */
  std::string confirmed_location;

  if(!path.empty()) {
    if(access( path.c_str(), F_OK ) == -1) {
      ERROR("specified file path (%s) does not exist.", path.c_str());
      assert(0);
      return E_NOT_FOUND;
    }
    else {
      confirmed_location = path;
    }
  }
  else {
    /* check COMPONENT_PATH environment variable */
    char * cpath = getenv("COMPONENT_PATH");
    if(cpath) {
      std::string file = cpath;
      file += path;
      if(access( path.c_str(), F_OK ) != -1) {
        confirmed_location = path;
      }
    }
    else {
      PERR("unable to find component.");
      return E_FAIL;
    }
  }

  PLOG("file location confirmed at %s", confirmed_location.c_str());

  /* convert UUID string */
  Component::uuid_t compid;
  if(compid.fromString(component_id)!=S_OK) {
    PERR("invalid UUID string");
    return E_INVAL;
  }

  /* load component and add to list */
  Component::IBase * comp = Component::load_component(confirmed_location,
                                                      compid);
  assert(comp);

  if(comp==NULL)
    return E_FAIL;

  {
    //    boost::lock_guard<boost::mutex> guard(lock_);
    loaded_components_.push_back(comp);
    locations_[comp] = confirmed_location;
    instances_[instance_id] = comp;
  }

  PLOG("component (%s) loaded OK", confirmed_location.c_str());
  dump_table();
  return S_OK;
}



/** 
 * Perform a bi-directional binding between two components.
 * 
 * @param left_inst First component
 * @param right_inst Second component
 * 
 * @return Number of bindigs remaining.
 */
signed 
ContainerOperations::
bind(std::string left_inst, 
     std::string right_inst)
{
  Component::IBase * left = instances_[left_inst];
  Component::IBase * right = instances_[right_inst];

  if(left == NULL || right == NULL) {
    PWRN("WARNING: try to bind an instance that is not found (%s,%s)",
         left_inst.c_str(),
         right_inst.c_str());
    assert(0);
    return -1;
  }

  signed remaining;
  remaining = left->bind(right);

  return remaining;
}

/** 
 * Bind two components uni-directionally according to a specific role.
 * 
 * @param left From component instance
 * @param right To component instance
 * @param id Role identifier for the binding
 * 
 * @return True if binding succeeded.
 */
signed 
ContainerOperations::
specified_bind(std::string left_inst, 
               std::string right_inst, 
               int id)
{
  Component::IBase * left = instances_[left_inst];
  Component::IBase * right = instances_[right_inst];

  if(left == NULL || right == NULL) {
    PWRN("WARNING: try to (specified) bind an instance that is not found (%s,%s)",
         left_inst.c_str(),
         right_inst.c_str());
    assert(0);
    return -1;
  }

  signed remaining;
  remaining = left->specified_bind(right, id);

  return remaining;
}


/** 
 * Perform a pair-wise bind
 * 
 * 
 * @return Number of remaining bindings. Return -1 on error.
 */
signed 
ContainerOperations::
pairwise_bind()
{
  signed remaining_bindings = 0;

  for(std::list<Component::IBase *>::iterator i=loaded_components_.begin();
      i != loaded_components_.end(); i++) {
    
      for(std::list<Component::IBase *>::iterator j=loaded_components_.begin();
          j != loaded_components_.end(); j++) {
        
        Component::IBase * left = *i;
        Component::IBase * right = *j;

        if(left == right) continue;
        
        assert(left);
        assert(right);

        signed r0 = left->bind(right);
        if(r0 < 0) {
          PERR("bind call on component failed (%d)",r0);
          return r0; /* if there is an error abort and return */
        }

        signed r1 = right->bind(left);
        if(r0 < 0) {
          PERR("bind call on component failed (%d)",r0);
          return r0; /* if there is an error abort and return */
        }
        
        remaining_bindings += r0 + r1;
      }
  }
  
  return remaining_bindings;
}


status_t 
ContainerOperations::
invoke(std::string instance_id,
       std::string operation, 
       std::string& result) {

  if(instances_.find(instance_id) == instances_.end()) {
    PERR("invalid instance (%s)", instance_id.c_str());
    return E_INVAL;
  }

  return instances_[instance_id]->invoke(operation, result);
}



/** 
 * Configure the pipeline via an XML configuration file.
 * TODO: add to actor protocol interface
 * 
 * @param xml XML configuration text
 * 
 * @return S_OK on success.
 */
status_t 
ContainerOperations::
xml_configure(std::string xml, std::string configuration_name)
{
  using namespace std;

  assert(!xml.empty());

  static TiXmlDocument doc;
  doc.Clear();
 
  if(!doc.Parse(xml.c_str(), 0)) {
    PERR("bad XML string for container configuration ((%s))",xml.c_str());
    assert(0);
  }

  TiXmlHandle doc_hdl(&doc); 
  TiXmlElement* root = doc_hdl.FirstChildElement("specification").ToElement();
  if(!root) {
    PERR("bad XML format");
    assert(0);
  }

 
  map<string,string> locations;

  /* parse locations - there may not be any */
  {
    TiXmlHandle parent(root);
    TiXmlElement* elem = parent.FirstChild("locations").ToElement();
    if(elem) {
      TiXmlHandle parent(elem);
      TiXmlElement* elem = parent.FirstChild("component").ToElement();
      while(elem) {

        assert(elem->Attribute("id") != NULL);
        assert(elem->Attribute("path") != NULL);

        PLOG("LOCATION: %s %s", elem->Attribute("id"), elem->Attribute("path"));
        /* store mapping */
        locations[elem->Attribute("id")] = elem->Attribute("path");
        elem = elem->NextSiblingElement();
      }
    }
  }

  /* teardown existing pipeline if there is one */
  if(!loaded_components_.empty()) {
    this->teardown();
  }

  bool auto_start = false;

  /* parse requested configuration */
  {
    bool conf_found = false;
    TiXmlHandle parent(root);
    TiXmlElement* elem = parent.FirstChild("configuration").ToElement();
    while (elem) {

      const char* name_attr = elem->Attribute("name");

      if (name_attr != nullptr && 
          name_attr == configuration_name || configuration_name == "") {
        conf_found = true;
        
        const char* start_attr = elem->Attribute("start");

        if(start_attr != nullptr && start_attr == std::string("true")) {
          PLOG("auto start on.");
          auto_start = true;
        }
        else {
          // auto_start = false if attribute 'start' not specified or set to
          // 'false'.
          PLOG("auto start off.");
        }

        TiXmlNode* parent = (TiXmlNode*) elem;

        /* iterate children */
        TiXmlNode* child = NULL;
        while((child = parent->IterateChildren(child))) {
          //PLOG("NODE (%s)",child->Value());

          TiXmlElement * elem = child->ToElement();
          if (elem == NULL) continue;

          /*--- instances - load_component invocations ---*/
          if(elem->ValueStr() == "instance") {
            string name = elem->Attribute("name");
            string id = elem->Attribute("id");

            if(name.empty() || id.empty()) {
              PWRN("invalid instance node");
              continue;
            }
            
            string path = locations[id];
            if(path.empty()) {
              /* TODO: try registry */
           
              if(path.empty()) {
                PWRN("unable to locate component (%s) library path", id.c_str());
                continue;
              }
              
            }
            PLOG("Trying to load component from registry info (%s)", path.c_str());

            this->load_component(name,id,path);
          }
          /*--- invocations ---*/
          else if(child->ValueStr() == "invoke") { 
            PLOG("executing invoke...");
            const char * name = elem->Attribute("name");
            const char * op = elem->Attribute("op");
            if(op==NULL || name==NULL) {
              PWRN("invalid invoke node");
              continue;
            }
            string opstr = op;
            /* unhash format, XML can't embed & */
            for (int i = 0; i < opstr.length(); ++i) {
              if (opstr[i] == '#')
                opstr[i] = '&';
            }
            PLOG("operation=%s(%s)",name,opstr.c_str());
            string result;
            this->invoke(string(name),opstr,result);
          }
          /*---- set_data_format ---*/
          else if(child->ValueStr() == "set_data_format") {
            
            const char * name = elem->Attribute("name");
            if(name==NULL) {
              PWRN("invalid set_data_format node");
              continue;
            }

            string opstr = "method=configure_data_format&format=xmlstring";
            const char * role = elem->Attribute("role");
            if(role) {
              opstr += "&role=";
              opstr += role;
            }

            opstr += "&xml=";

            TiXmlHandle parent(child);
            TiXmlElement* elem = parent.FirstChild("description").ToElement();
            assert(elem);

            TiXmlPrinter printer;
            elem->Accept(&printer);
            string xmlformat = printer.CStr();

            opstr += base64_encode(xmlformat);
            string result;
            this->invoke(string(name),opstr,result);
          }
          /*--- bindings ---*/
          else if(child->ValueStr() == "bidirectional_binding") {
            string from = elem->Attribute("from");
            string to = elem->Attribute("to");
            if(from.empty() || to.empty()) {
              PWRN("invalid binding node");
              continue;
            }
            signed remaining = this->bind(from,to);
            remaining += this->bind(to,from);
            PINF("bidirectional binding (%s-%s) %d remaining", from.c_str(), to.c_str(), remaining);
          }
          /*---- unidirectional bindings ---*/
          else if(child->ValueStr() == "unidirectional_binding") {
            string from = elem->Attribute("from");
            string to = elem->Attribute("to");
            if(from.empty() || to.empty()) {
              PWRN("invalid binding node");
              continue;
            }
            signed remaining = this->bind(from, to);
            PINF("unidirectional binding (%s-%s) %d remaining", from.c_str(), to.c_str(), remaining);
          }
          /*---- specified bindings ---*/
          else if(child->ValueStr() == "specified_binding") {

            if(!elem->Attribute("from")) {
              PERR("no 'from' attribute in specified binding");
              return E_FAIL;
            }
            string from = elem->Attribute("from");

            if(!elem->Attribute("to")) {
              PERR("no 'to' attribute in specified binding");
              return E_FAIL;
            }
            string to = elem->Attribute("to");

            if(!elem->Attribute("role_id")) {
              PERR("no 'role_id' attribute in specified binding");
              return E_FAIL;
            }
            string role = elem->Attribute("role_id");

            if(from.empty() || to.empty() || role.empty()) {
              PWRN("invalid specified binding node");
              continue;
            }

            int rid = std::stoi(role.c_str());

            signed remaining = this->specified_bind(from, to, rid);
            PINF("specified binding (%s-%s) (role=%d) %d remaining", 
                 from.c_str(), 
                 to.c_str(), 
                 rid,
                 remaining);
          }
          else {
            PWRN("unknown value (%s) in configuration", child->Value());
          }
        }

        if(auto_start)
          return start_pipeline();

        return S_OK;
      }
      elem = elem->NextSiblingElement();
    }

    if (!conf_found) {
      PWRN("unknown configuration '%s'.", configuration_name.c_str());
      return E_INVAL;
    }
  }

  return S_OK;
}


/** 
 * Teardown a pipeline, shutting down components and then deleting them.
 * 
 * 
 * @return S_OK on success, E_FAIL otherwise.
 */
status_t ContainerOperations::teardown()
{
  status_t s;

  /* shutdown all components */
  for(auto i : loaded_components_) {
    s = i->shutdown();
    if(s != S_OK && s != E_NOT_IMPL) {
      PWRN("teardown failed; component shutdown() returned error");
      return E_FAIL;
    }
  }

  /* unload components */
  for(auto i : loaded_components_) {
    assert(i->ref_count() == 1);
    i->release_ref(); // should cause unload
  }

  loaded_components_.clear();
  instances_.clear();

  return S_OK;
}


status_t ContainerOperations::start_pipeline()
{
  // TODO: ordering?
  status_t s;
  PINF("starting pipeline ...");

  for(auto i = loaded_components_.rbegin(); /* in reverse */
      i != loaded_components_.rend();
      ++i ) {
    s = (*i)->start();
    if(s == E_NOT_IMPL) {
      PWRN("warning component does not implement start");
    }

    if(s != S_OK && s != E_NOT_IMPL) {
      PERR("start pipeline failed");
      return E_FAIL;
    }
  }

  PLOG("start pipeline OK.");
  return S_OK;
}

status_t ContainerOperations::stop_pipeline()
{
  // TODO: ordering?
  status_t s;

  for(auto i : loaded_components_) {
    s = i->stop();
    if(s != S_OK && s != E_NOT_IMPL) {
      PWRN("stop pipeline failed");
      return E_FAIL;
    }
  }

  PLOG("stopped pipeline OK.");
  return S_OK;
}

status_t ContainerOperations::reset_pipeline()
{
  // TODO: ordering?
  status_t s = stop_pipeline();
  if(s!=S_OK) return s;

  for(auto i : loaded_components_) {
    s = i->start();
    if(s != S_OK && s != E_NOT_IMPL) {
      PWRN("reset pipeline failed");
      return E_FAIL;
    }
  }

  PLOG("reset pipeline OK.");
  return S_OK;
}


void ContainerOperations::dump_table()
{
  PINF("--- Container (%p) ---------------------------------------------------------------------------------", (void *) this);

  for(std::map<std::string, Component::IBase *>::iterator i=instances_.begin();
      i != instances_.end(); i++) {
    Component::IBase * ptr = i->second;
    std::string id = i->first;
    std::string location = locations_[ptr];
    PINF("%20s\t%-70s\t%p\t%u", id.c_str(), location.c_str(), (void*) ptr, ptr->ref_count());
  }
  PINF("----------------------------------------------------------------------------------------------------------------");
}


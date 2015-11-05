#include "hello_itf.h"
#include <common/logging.h>

/** 
 * Interface IHello implementation
 * 
 */
class IHello_impl : public IHello
{
public:
  void sayHello(unsigned x);
};


/** 
 * Definition of the component
 * 
 */
class MyComponent : public IHello_impl                    
{
public:  
  DECLARE_COMPONENT_UUID(0xae591c36,0x7988,0x11e3,0x9f1c,0xbc,0x30,0x5b,0xdc,0x75,0x4d);

public:
  void * query_interface(Component::uuid_t& itf_uuid) {
    if(itf_uuid == IHello::iid()) {
      return (void *) static_cast<IHello *>(this);
    }
    else 
      return NULL; // we don't support this interface
  };

  void unload() {
    PLOG("Unloading component.");
    delete this;
  }
  
  DUMMY_IBASE_CONTROL;
};

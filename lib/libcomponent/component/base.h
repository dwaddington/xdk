#ifndef __COMPONENT_BASE_H__
#define __COMPONENT_BASE_H__

#include <boost/atomic.hpp>
#include <assert.h>
#include <stdint.h>
#include <dlfcn.h>

#include "types.h"

#define DECLARE_UUID(name,f1,f2,f3,f4,f5,f6,f7) \
  const Component::uuid_t name = {f1,f2,f3,f4,{f5,f6,f7}};

#define DECLARE_INTERFACE_UUID(f1,f2,f3,f4,f5,f6,f7)              \
  static Component::uuid_t& uuid() {                              \
    static Component::uuid_t itf_uuid = {f1,f2,f3,f4,{f5,f6,f7}}; \
    return itf_uuid;                                              \
  }

#define DECLARE_COMPONENT_UUID(f1,f2,f3,f4,f5,f6,f7)                \
  Component::uuid_t component_id() {                                \
    static Component::uuid_t comp_uuid = {f1,f2,f3,f4,{f5,f6,f7}};  \
    return comp_uuid;                                               \
  }

namespace Component
{
  /** 
   * Standard UUID (Universal Unique Identifier) structure
   * 
   */
  struct uuid_t
  {
    uint32_t uuid0;
    uint16_t uuid1;
    uint16_t uuid2;
    uint16_t uuid3;
    uint16_t uuid4[3];
  };

  bool operator==(const Component::uuid_t& lhs, const Component::uuid_t& rhs);

  /** 
   * Base class. All components must inherit from this class.
   * 
   */
  class Base
  {
  private:
    boost::atomic<unsigned> _ref_count;
    void *                  _dll_handle;

  public:
    Base() : _dll_handle(NULL) {
      _ref_count++;
    }

    /** 
     * Pure virtual functions that should be implemented by all
     * components
     * 
     */
    virtual void * query_interface(Component::uuid_t& itf) = 0;
    virtual Component::uuid_t component_id(void) = 0;


    /** 
     * Reference counting
     * 
     */
    void add_ref() {
      _ref_count++;
    }

    void release_ref() {
      int val = _ref_count.fetch_sub(1) - 1;    
      assert(val >= 0);
      if(val == 0) {
        delete this;
        dlclose(_dll_handle);
      }
    }

    unsigned ref_count() { 
      return _ref_count.load();
    }

    void set_dll_handle(void * dll) {
      assert(dll);
      _dll_handle = dll;
    }
  };


  /** 
   * Called by the client to load the component from a DLL file
   * 
   */
  Base * load_component(const char * dllname);

};

#endif

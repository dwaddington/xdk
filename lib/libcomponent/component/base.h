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
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __COMPONENT_BASE_H__
#define __COMPONENT_BASE_H__

#include <boost/atomic.hpp>
#include <assert.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdio.h>
#include <common/types.h>
#include <string>
#include <sstream>
#include <iomanip>


#define DECLARE_UUID(name,f1,f2,f3,f4,f5,f6,f7) \
  const Component::uuid_t name = {f1,f2,f3,f4,{f5,f6,f7}};

#define DECLARE_INTERFACE_UUID(f1,f2,f3,f4,f5,f6,f7)              \
  static Component::uuid_t& iid() {                              \
    static Component::uuid_t itf_uuid = {f1,f2,f3,f4,{f5,f6,f7}}; \
    return itf_uuid;                                              \
  }

#define DECLARE_COMPONENT_UUID(f1,f2,f3,f4,f5,f6,f7)                \
  static Component::uuid_t component_id() {                                \
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

    std::string toString() {
      std::stringstream ss;
      ss << std::hex << std::setfill('0') << std::setw(8) << uuid0 << "-" 
         << std::setfill('0') << std::setw(4) << uuid1 << "-" 
         << std::setfill('0') << std::setw(4) << uuid2 << "-" 
         << std::setfill('0') << std::setw(4) << uuid3 << "-" 
         << std::setfill('0') << std::setw(4) << uuid4[0] 
         << std::setfill('0') << std::setw(4) << uuid4[1] 
         << std::setfill('0') << std::setw(4) << uuid4[2];
      return ss.str();
    }
  };

  bool operator==(const Component::uuid_t& lhs, const Component::uuid_t& rhs);

  /** 
   * Base class. All components must inherit from this class.
   * 
   */
  class IBase
  {
  private:
    boost::atomic<unsigned> _ref_count;
    void *                  _dll_handle;

  public:
    IBase() : _dll_handle(NULL), _ref_count(0) {
    }


    /** 
     * Pure virtual functions that should be implemented by all
     * components
     * 
     */
    virtual void * query_interface(Component::uuid_t& itf) = 0;

    /* optional unload */
    virtual void unload() {}


    /** 
     * Reference counting
     * 
     */
    virtual void add_ref() {
      _ref_count++;
    }

    virtual void release_ref() {
      int val = _ref_count.fetch_sub(1) - 1;    
      assert(val >= 0);
      if(val == 0) {
        this->unload(); /* call virtual unload function */
        dlclose(_dll_handle);
      }
    }

    virtual unsigned ref_count() { 
      return _ref_count.load();
    }

    virtual void set_dll_handle(void * dll) {
      assert(dll);
      _dll_handle = dll;
    }
  };

  /** 
   * Called by the client to load the component from a DLL file
   * 
   * @param dllname Name of dynamic library file
   * @param component_id Component UUID to load
   * 
   * @return Pointer to IBase interface
   */
  IBase * load_component(const char * dllname, Component::uuid_t component_id);

};



#endif

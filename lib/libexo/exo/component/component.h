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





#ifndef __EXO_COMPONENT_H__
#define __EXO_COMPONENT_H__

#include "../../libexo.h"
#include <stdint.h>
#include <dlfcn.h>

#define DECLARE_UUID(name,f1,f2,f3,f4,f5,f6,f7) \
  const Exokernel::uuid_t name = {f1,f2,f3,f4,{f5,f6,f7}};

#define DECLARE_INTERFACE_UUID(f1,f2,f3,f4,f5,f6,f7) \
  static Exokernel::uuid_t& uuid() {                               \
    static Exokernel::uuid_t itf_uuid = {f1,f2,f3,f4,{f5,f6,f7}}; \
    return itf_uuid;                                   \
  }

#define DECLARE_COMPONENT_UUID(f1,f2,f3,f4,f5,f6,f7) \
  Exokernel::uuid_t component_id() {                               \
    static Exokernel::uuid_t comp_uuid = {f1,f2,f3,f4,{f5,f6,f7}}; \
    return comp_uuid;                                   \
  }

namespace Exokernel
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

  bool operator==(const Exokernel::uuid_t& lhs, const Exokernel::uuid_t& rhs);

  /** 
   * Base class. All components must inherit from this class.
   * 
   */
  class Component_base
  {
  private:
    Exokernel::Atomic _ref_count;
    void *            _dll_handle;

  public:
    Component_base() : _dll_handle(NULL) {
      _ref_count.increment(); 
    }

    virtual ~Component_base() {
    }

    /** 
     * Pure virtual functions that should be implemented by all
     * components
     * 
     */
    virtual void * query_interface(Exokernel::uuid_t& itf) = 0;
    virtual Exokernel::uuid_t component_id(void) = 0;


    /** 
     * Reference counting
     * 
     */
    void add_ref() {
      _ref_count.increment();
    }

    void release_ref() {
      atomic_t val = _ref_count.decrement_and_fetch();    
      if(val == 0) {
        delete this;
        dlclose(_dll_handle);
      }
    }

    unsigned ref_count() { 
      return _ref_count.read();
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
  Component_base * load_component(const char * dllname);

}

#endif

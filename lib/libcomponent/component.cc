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

#include <stdint.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <component/base.h>
#include <common/errors.h>

namespace Component
{

  bool operator==(const Component::uuid_t& lhs, const Component::uuid_t& rhs) {
    return memcmp(&lhs, &rhs, sizeof(Component::uuid_t))==0;
  }

  /** 
    * Called by the client to load the component from a DLL file
    * 
    */
  IBase * load_component(const char * dllname, Component::uuid_t component_id) {

    void * (*factory_createInstance)(Component::uuid_t&);
    char * error;
    void * dll = dlopen(dllname,RTLD_NOW);

    if(!dll) {
      PERR("unable to load library (%s) - check dependencies with ldd tool.",dllname);
      return NULL;
    }

    dlerror();
    *(void **) (&factory_createInstance) = dlsym(dll,"factory_createInstance");

    if ((error = dlerror()) != NULL)  {
      PERR("Error: %s\n", error);
      return NULL;
    }

    IBase* comp = (IBase*) factory_createInstance(component_id);

    if(comp==NULL) {
      PERR("Error: factory create instance returned null.");
      return NULL;
    }

    comp->set_dll_handle(dll); /* record so we can call dlclose() */
    comp->add_ref();

    return comp;
  }


  /** 
   * Perform pairwise binding of components
   * 
   * @param components List of components to bind
   * 
   * @return S_OK on success.
   */
  status_t bind(std::vector<IBase *> components) {

    for(int i=0;i<components.size();i++) {
      assert(components[i]);
      for(int j=0;j<components.size();j++) {
        assert(components[j]);
        if(i==j) continue;
        if(components[i]->bind(components[j]) == -1) {
          PDBG("connect call in pairwise bind failed.");
          return E_FAIL;
        }
      }
    }

    return S_OK;
  }
}

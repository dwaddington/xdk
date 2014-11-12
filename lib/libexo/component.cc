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
#include "exo/component/component.h"

namespace Exokernel
{

  bool operator==(const Exokernel::uuid_t& lhs, const Exokernel::uuid_t& rhs) {
    return memcmp(&lhs, &rhs, sizeof(Exokernel::uuid_t))==0;
  }

  /** 
    * Called by the client to load the component from a DLL file
    * 
    */
  Component_base * load_component(const char * dllname) {
    void * (*factory_createInstance)();
    char * error;
    void * dll = dlopen(dllname,RTLD_LAZY);
    assert(dll);

    dlerror();
    *(void **) (&factory_createInstance) = dlsym(dll,"factory_createInstance");

    if ((error = dlerror()) != NULL)  {
      fprintf(stderr, "Error: %s\n", error);
      return NULL;
    }

    Component_base* comp = (Component_base*) factory_createInstance();
    comp->set_dll_handle(dll); /* record so we can call dlclose() */

    return comp;
  }
}

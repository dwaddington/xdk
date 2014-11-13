#include <stdint.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>

#include <component/base.h>


namespace Component
{

  bool operator==(const Component::uuid_t& lhs, const Component::uuid_t& rhs) {
    return memcmp(&lhs, &rhs, sizeof(Component::uuid_t))==0;
  }

  /** 
    * Called by the client to load the component from a DLL file
    * 
    */
  Base * load_component(const char * dllname) {
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

    Base* comp = (Base*) factory_createInstance();
    comp->set_dll_handle(dll); /* record so we can call dlclose() */

    return comp;
  }
}

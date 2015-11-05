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

#include <string.h>
#include <stdio.h>
#include <common/logging.h>

#include <component/base.h>
#include "dummy_block_device.h"

//////////////////////////////////////////////////////////////////////
// IDeviceControl interface
//
status_t IBlockDevice_impl::init_device(unsigned instance, const char * config = NULL) {
  PLOG("init_device(instance=%u)",instance);
  return S_OK;
}

Exokernel::Device * IBlockDevice_impl::get_device() {
  TRACE();
  return NULL;
}

status_t IBlockDevice_impl::shutdown_device() {
  TRACE();
  return S_OK;
}





extern "C" void * factory_createInstance(Component::uuid_t& component_id)
{
  if(component_id == DummyBlockDeviceComponent::component_id()) {
    printf("Creating 'DummyBlockDeviceComponent' component.\n");
    return static_cast<void*>(new DummyBlockDeviceComponent());
  }
  else return NULL;
}


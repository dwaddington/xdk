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

#ifndef __BLOCK_DEVICE_ITF__
#define __BLOCK_DEVICE_ITF__

#include <component/base.h>
#include <exo/device.h>

#include "device_itf.h"
#include "dataplane_itf.h"

/** 
 * Interface definition for IBlockDevice
 * 
 */
class IBlockDevice : public Component::IBase,
                     public IDeviceControl,
                     public IBlockData
{
public:
  DECLARE_INTERFACE_UUID(0x66d636bb,0x1cd1,0x427c,0xb0b6,0x7e,0x75,0xd9,0x10,0x9b,0x39);

};


#endif// __BLOCK_DEVICE_ITF__

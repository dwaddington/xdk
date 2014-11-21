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

#ifndef __EXOLIB_H__
#define __EXOLIB_H__

#include <stdio.h>

#include "exo/config.h"
#include "exo/errors.h"
#include "exo/rand.h"
#include "exo/thread.h"
#include "exo/memory.h"
#include "exo/pagemap.h"
#include "exo/sysfs.h"
#include "exo/device.h"
#include "exo/slab.h"
#include "exo/lockfree_queue.h"
#include "exo/locks.h"
#include "exo/topology.h"
#include "exo/irq_affinity.h"
#include "exo/numa_slab.h"
#include "exo/bitmap.h"

namespace Exokernel
{
  class Device;

  /** 
   * Query the number of active devices bound to the parasitic kernel
   * 
   * 
   * @return Number of bound devices
   */
  signed query_num_registered_devices();


  /** 
   * Open a device registered with the parasitic kernel
   * 
   * @param instance Instance counting from 0. Default param is 0
   * 
   * @return [new] allocated instance of Device. Client destroys.
   */
  Exokernel::Device * open_device(unsigned instance,
                                  unsigned vendor,
                                  unsigned device_id);

  

}



#endif // __EXOLIB_H__

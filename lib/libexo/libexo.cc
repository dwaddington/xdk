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

#include <stdio.h>
#include <dirent.h>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <fstream>
#include <dirent.h>

#include "libexo.h"

/** 
 * Query the number of active devices bound to the parasitic kernel
 * 
 * 
 * @return Number of bound devices, return < 0 on error
 */
signed Exokernel::query_num_registered_devices()
{
  /* 
     Devices that are registered with the parasitic kernel 
     are exposed through the /sysfs file system

     /sys/class/parasite/pkXXX where XXX starts from 0
  */
  
  /* open /sys/class/parasite directory */
  DIR *pdir;

  if(!(pdir = opendir("/sys/class/parasite/"))) {
    perror("Unable to open parasite dir (/sys/class/parasite)");
    printf("Module loaded?\n");
    return E_BAD_FILE;
  }

  struct dirent *entry;
  signed count = 0;
  while ((entry = readdir(pdir))) {
    if(strncmp(entry->d_name, "pk",2) == 0)
      count++;
  }
  closedir(pdir);
  
  return count;
}


Exokernel::Device * Exokernel::open_device(unsigned instance,
                                           unsigned vendor,
                                           unsigned device_id)
{
  return new Device(instance,vendor,device_id);
}




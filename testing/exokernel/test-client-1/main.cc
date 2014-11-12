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




#include <stdio.h>

#include "libexo.h"

int main()
{
  using namespace Exokernel;

  signed nd = query_num_registered_devices();
  printf("Got %d registered devices\n",nd);

  Exokernel::Device * dev = open_device(0,
                                        0x8086,  // vendor
                                        0x2922); // Qemu AHCI device
  //                                        0x5845); // Qemu NVM device
  printf("Device opened OK\n");

  /* dump PCI config space */
  dev->pci_config()->dump_pci_device_info();

  // /* dump some memory region info */
  // for(unsigned i=0;i<6;i++) {
  //   Exokernel::Device_sysfs::Pci_mapped_memory_region * r = dev->pci_memory_region(i);
  //   if(r) {
  //     r->dump_memory_region_info();

  //     /* example of how to read */
  //     if (i == 0) { // read first 32 bits of BAR 0 region 

  //       uint32_t reg = r->mmio_read32(3);
  //       PLOG("!!reg=%x",reg);

  //       for(unsigned i=0;i<20;i++) {
  //         uint32_t * p = (uint32_t *) r->mmap_ptr();
  //         uint32_t x = p[i];
  //         PLOG("read OK [%u] x=%x",i,x);
  //       }
  //     }
  //   }

  // }

  /* clean up */
  delete dev;

  return 0;
}

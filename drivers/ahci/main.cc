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
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libexo.h>
#include <common/dump_utils.h>
#include <common/cycles.h>

#include "ahci_device.h"

#define USE_AHCI_PORT 0

void do_basic_write_test(AHCI_uddk_device * dev, unsigned port, void * vbuff, addr_t pbuff)
{
  status_t hr = dev->port(port)->sync_fpdma_write(0, // slot 
                                                  0, // block
                                                  1, // count
                                                  pbuff);
  
  assert(hr == Exokernel::S_OK);

}


void do_basic_read_test(AHCI_uddk_device * dev, unsigned port, void * vbuff, addr_t pbuff)
{
  status_t hr = dev->port(port)->sync_fpdma_read(0, // slot 
                                                 0, // block
                                                 1, // count
                                                 pbuff);
  
  assert(hr == Exokernel::S_OK);

  hexdump((void *)vbuff, 512);
  PLOG("Block dump OK.");
}

void do_random_read_test(AHCI_uddk_device * dev, unsigned port, void * vbuff, addr_t pbuff, unsigned num_blocks)
{
  using namespace Exokernel;

  uint64_t capacity = dev->port(port)->capacity_in_blocks();

  init_genrand64(rdtsc());
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW,&start);

  for(unsigned b=0;b<num_blocks;b++) {
    uint64_t block_to_read = (genrand64_int64() % capacity);

    status_t hr = dev->port(port)->sync_fpdma_read(0, // slot 
                                                   block_to_read, // block
                                                   1, // count
                                                   pbuff);
    
    assert(hr == Exokernel::S_OK);    
  }
  clock_gettime(CLOCK_MONOTONIC_RAW,&end);
  
  double msec = (double) ((end.tv_sec - start.tv_sec) * 1000.0) + (double) ((end.tv_nsec - start.tv_nsec)/1000000);
  PLOG("time taken: %g milliseconds", msec);
  PLOG("rate: %g blocks/second", 1000.0 / (msec/(double)num_blocks));
}


int main()
{
  using namespace Exokernel;

  try {
    signed nd = query_num_registered_devices();
    printf("Got %d registered devices\n",nd);
    
    AHCI_uddk_device * dev = new AHCI_uddk_device();

    /* allocate some memory */    
    addr_t phys;
    byte * virt = (byte*) dev->alloc_dma_pages(512,(addr_t*)&phys); // allocate 2MB
    assert(phys);
    assert(virt);    
    memset(virt,0,512);


#if 1
    do_basic_read_test(dev,USE_AHCI_PORT,virt,phys);

    memset(virt,virt[0]+1,512);
    do_basic_write_test(dev,USE_AHCI_PORT,virt,phys);

    do_basic_read_test(dev,USE_AHCI_PORT,virt,phys);
#endif

#if 0
    do_random_read_test(dev,USE_AHCI_PORT,virt,phys,1000);
#endif
    
    printf("Cleaning up AHCI device.\n");
    delete dev;
  }
  catch(Exokernel::Exception e) {
    PLOG("top-level exception: %s",e.cause());
  }
   

  return 0;
}

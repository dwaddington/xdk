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
#include <sys/time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>

#include <component/base.h>
#include <common/cycles.h>
#include <exo/rand.h>

#include "nvme_drv_component.h"
#include "tests.h"
#include "mt_tests.cc"

/* very basic test */
void basic_test(IBlockDevice * itf)
{
  Exokernel::Device * dev = itf->get_device();

  addr_t phys = 0;
  void * p = dev->alloc_dma_pages(1,&phys);
  const off_t lba = 10;//max 781,422,766;
    
  memset(p,0xA,PAGE_SIZE);

//#define ORIG_SYNC_IO 
#ifdef ORIG_SYNC_IO 
  itf->sync_write_block(p, /* must be 512 byte aligned */
                        phys, 
                        10, // lba
                        1,  /* each block is 512 bytes */
                        1); // queue
#else
  io_descriptor_t* io_desc = (io_descriptor_t *)malloc(sizeof(io_descriptor_t));
  memset(io_desc, 0, sizeof(io_descriptor_t));
  io_desc->action = NVME_WRITE;
  io_desc->buffer_virt = p;
  io_desc->buffer_phys = phys;
  io_desc->offset = lba;
  io_desc->num_blocks = 1;
   
  itf->sync_io((io_request_t)io_desc, 1, 0);
#endif

  PLOG("======= DONE WRITE ========");

  memset(p,0,PAGE_SIZE);


#ifdef ORIG_SYNC_IO 
  itf->sync_read_block(p, /* must be 512 byte aligned */
                       phys, 
                       10, // lba
                       1,  /* each block is 512 bytes */
                       1); // queue
#else
  memset(io_desc, 0, sizeof(io_descriptor_t));
  io_desc->action = NVME_READ;
  io_desc->buffer_virt = p;
  io_desc->buffer_phys = phys;
  io_desc->offset = lba;
  io_desc->num_blocks = 1;

  itf->sync_io((io_request_t)io_desc, 1, 0);
#endif

  hexdump(p,512);

  dev->free_dma_pages(p);
}

int main()
{
  Exokernel::set_thread_name("nvme_drv-test-client");

  Component::IBase * comp = Component::load_component("./nvme_drv.so.1",
                                                      NVME_driver_component::component_id());

  IBlockDevice * itf = (IBlockDevice *) comp->query_interface(IBlockDevice::iid());

  itf->init_device(0);

  basic_test(itf);
  //(new mt_tests())->runTest(itf);

  itf->shutdown_device();

  PLOG("nvme-ssd driver test client complete.");
  return 0;
}


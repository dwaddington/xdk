#include <stdio.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <common/cycles.h>
#include <common/dump_utils.h>

#include "nvme_device.h"

void basic_block_read(NVME_device * dev, size_t num_blocks) {

  const unsigned qid = 1;
  assert(num_blocks < 256);

  Notify_object nobj;

  /* set up call back */
  dev->io_queue(qid)->callback_manager()->register_callback(1,
                                                            &Notify_object::notify_callback,
                                                            (void*)&nobj);

  addr_t phys = 0;
  void * p = Exokernel::Memory::alloc_pages(1,&phys);

  assert(p);
  assert(phys);

  uint16_t cid;
  for(unsigned i=0;i<num_blocks;i++) {

    cid = dev->block_async_read(qid,
                                phys,
                                i*512, /* offset */
                                1); /* num blocks */
    
    PLOG("Block read cid=%u",cid);
    
    hexdump(p,512);
  }
  nobj.set_when(cid);
  nobj.wait();
  PLOG("@@@@ Blocks read");

}


void basic_block_write(NVME_device * dev, size_t num_blocks) {

  //  Exokernel::Memory::huge_system_configure_nrpages(10);
  const unsigned qid = 1;
  assert(num_blocks < 256);

  Notify_object nobj;

  /* set up call back */
  dev->io_queue(qid)->callback_manager()->register_callback(1,
                                                            &Notify_object::notify_callback,
                                                            (void*)&nobj);

  addr_t phys = 0;
  void * p = Exokernel::Memory::alloc_pages(1,&phys);

  assert(p);
  assert(phys);

  uint16_t cid;
  for(unsigned i=0;i<num_blocks;i++) {

    /* write a value in */
    memset(p,i,512);

    cid = dev->block_async_write(qid,
                                 phys,
                                 i*512, /* offset */
                                 1);/* num blocks */
    
    PLOG("*** Block written cid=%u",cid);
  }
  nobj.set_when(cid);
  nobj.wait();
  PLOG("******** Blocks written!!!");
}

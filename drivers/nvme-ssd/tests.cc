#include <stdio.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <common/cycles.h>
#include <common/dump_utils.h>

#include "nvme_device.h"
#include "tests.h"


void basic_block_read(NVME_device * dev, off_t lba) {

  PLOG("running basic_block_read..");
  const unsigned qid = 1;

  Notify_object nobj;

  /* set up call back */
  dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
                                                            (void*)&nobj);

  addr_t phys = 0;
  void * p = dev->alloc_dma_pages(1,&phys);

  assert(p);
  assert(phys);

  uint16_t cid;

  cid = dev->block_async_read(qid,
                              phys,
                              lba, /* LBA */
                              1); /* num blocks */
  nobj.set_when(cid);
  nobj.wait();

  PLOG("expected block read complete.");
    
  hexdump(p,512);

  dev->free_dma_pages(p);
}

void flush_test(NVME_device * dev) {

  uint16_t cid;

  const unsigned qid = 1;
  Notify_object nobj;

  /* set up call back */
  dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
                                                            (void*)&nobj);

  PLOG("Issuing flush command..");
  cid = dev->io_queue(qid)->issue_flush();
  nobj.set_when(cid);
  nobj.wait();
  PLOG("Flushed OK.");
}

void basic_block_write(NVME_device * dev, off_t lba) {

  PLOG("running basic_block_write..");

  const unsigned qid = 1;
  Notify_object nobj;

  /* set up call back */
  dev->io_queue(qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
                                                            (void*)&nobj);

  unsigned num_data_pages = 4;
  unsigned num_data_blocks = (PAGE_SIZE * num_data_pages) / 512;

  PLOG("Allocated %d pages, %d blocks",num_data_pages, num_data_blocks);

  addr_t phys = 0;
  //  void * p = Exokernel::Memory::alloc_pages(num_data_pages,&phys);
  void * p = dev->alloc_dma_pages(num_data_pages,&phys);
  memset(p,0xfe,num_data_pages*PAGE_SIZE);
  assert(p);
  assert(phys);

  memset(p,0xea,512);


  uint16_t cid;

  //  for(unsigned i=0;i<num_blocks;i++) {
  cid = dev->block_async_write(qid,
                               phys,
                               lba, /* LBA */
                               1);/* num blocks */
    
  nobj.set_when(cid);
  nobj.wait();
  PLOG("******** Blocks written!!!");

  dev->free_dma_pages(p);
}

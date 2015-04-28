#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <common/utils.h>
#include <common/dump_utils.h>

#include "nvme_drv_component.h"
#include "nvme_device.h"
#include "nvme_types.h"

Exokernel::Spin_lock total_iops_lock;
static uint64_t total_iops  __attribute__((aligned(8))) = 0;
static bool blast_exit = false;

/** 
 * Simply blast Read requests at a given queue
 * 
 * @param itf 
 * @param queue 
 */
void blast_queue(IBlockDevice * itf, unsigned queue, off_t max_lba)
{
  PLOG("BLASTING Queue=%u ........", queue);
  Exokernel::Device * dev = itf->get_device();

  addr_t phys = 0;
  void * p = dev->alloc_dma_pages(1,&phys);
  const off_t lba = rdtsc() % (max_lba - 8);
    
  io_descriptor_t* io_desc = (io_descriptor_t *)malloc(sizeof(io_descriptor_t));
  
  io_desc->action = NVME_READ;
  io_desc->buffer_virt = p;
  io_desc->buffer_phys = phys;
  io_desc->offset = lba;
  io_desc->num_blocks = 1;

  while(!blast_exit) {
    itf->sync_io((io_request_t)io_desc, queue /*queue/port*/, 0 /* device */);
    total_iops_lock.lock();
    total_iops++;
    total_iops_lock.unlock();
  }

  hexdump(p,512);

  dev->free_dma_pages(p);
}

struct bt_param {
  IBlockDevice * itf;
  int queue;
  off_t max_lba;
};

void * blast_thread_entry(void* p) {
  bt_param * param = (bt_param *) p;

  blast_queue(param->itf, param->queue/*queue*/, param->max_lba);
  return NULL;
}

/** 
 * Main entry to create multiple blasting threads
 * 
 * @param itf 
 * @param max_lba 
 */
void blast(IBlockDevice * itf, off_t max_lba)
{
#define NUM_QUEUES 1
#define TIME_DURATION_SEC 10

  pthread_t threads[NUM_QUEUES];

  for(unsigned i=0;i<NUM_QUEUES;i++) {

    bt_param * param = new bt_param;
    param->queue = i+1;
    param->max_lba = max_lba;
    param->itf = itf;

    pthread_create(&threads[i],NULL,blast_thread_entry,(void*)param);
  }
  sleep(TIME_DURATION_SEC);

  /* signal threads to exit */
  blast_exit = true;

  /* join threads */
  for(unsigned i=0;i<NUM_QUEUES;i++) {
    void * retval;
    pthread_join(threads[i],&retval);
  }
  printf("Rate: %ld IOPS\n", total_iops / TIME_DURATION_SEC);
}

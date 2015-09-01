#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <common/utils.h>
#include <common/dump_utils.h>
#include <exo/sysfs.h>
#include <atomic>

#include "nvme_drv_component.h"
#include "nvme_device.h"
#include "nvme_types.h"

Exokernel::Spin_lock total_ops_lock;
static uint64_t total_ops __attribute__((aligned(8)));
static bool blast_exit = false;

class ReadBlasterThread : public Exokernel::Base_thread
{
private:
  IBlockDevice * itf_;
  unsigned queue_; 
  off_t max_lba_;

public:
  ReadBlasterThread(unsigned core, IBlockDevice * itf, unsigned queue, off_t max_lba) :
    queue_(queue),
    max_lba_(max_lba),
    itf_(itf),
    Exokernel::Base_thread(NULL, core) {
    PINF("Read blaster thread core (%u)", core);
  }

  void* entry(void* param) {
    blast_loop();
  }

  /** 
   * Simply blast Read requests at a given queue
   * 
   * @param itf 
   * @param queue 
   */
  void blast_loop()
  {
    const unsigned BATCH_SIZE = 256;
    Exokernel::Device * dev = itf_->get_device();
    addr_t phys = 0;

    void * p = dev->alloc_dma_pages(BATCH_SIZE,
                                    &phys,
                                    Exokernel::Device_sysfs::DMA_FROM_DEVICE);
    assert(p);


    io_descriptor_t* io_desc = (io_descriptor_t *) malloc(sizeof(io_descriptor_t) * BATCH_SIZE);
  
    for(unsigned b=0;b<BATCH_SIZE;b++) {
      io_desc[b].action = NVME_READ;
      io_desc[b].buffer_virt = p;
      io_desc[b].buffer_phys = phys;
      io_desc[b].num_blocks = 1;
    }

    uint64_t counter = 0;
    Notify *notify = new Notify_Async();

    while(!blast_exit) {

#if 1 // batch version
      for(unsigned b=0;b<BATCH_SIZE;b++) {
        io_desc[b].offset = genrand64_int64() % (max_lba_ - 8);
      }

      status_t st = itf_->async_io_batch((io_request_t *)io_desc, 
                                         BATCH_SIZE, 
                                         notify, 
                                         queue_, 0);

      total_ops+=BATCH_SIZE;

#else // non-batch version
      io_desc[0].offset = genrand64_int64() % (max_lba_ - 8);
      //      printf("reading offset %ld\n",io_desc[0].offset);
      itf_->async_io((io_request_t)&io_desc[0], 
                    notify, 
                    queue_ /*queue/port*/, 0 /* device */);
      total_ops++;
#endif

      if(total_ops % 100000 == 0)
        printf("Op count: %ld\n",total_ops);

    }

    // wait for batch?
    itf_->wait_io_completion(queue_); //wait for IO completion     

    dev->free_dma_pages(p);
  }

};


class VerifyBlasterThread : public Exokernel::Base_thread
{
private:
  IBlockDevice * itf_;
  unsigned queue_; 
  off_t max_lba_;

public:
  VerifyBlasterThread(unsigned core, IBlockDevice * itf, unsigned queue, off_t max_lba) :
    queue_(queue),
    max_lba_(max_lba),
    itf_(itf),
    Exokernel::Base_thread(NULL, core) {
    PINF("Verify blaster thread core (%u)", core);
  }

  void* entry(void* param) {
    blast_loop();
  }

  /** 
   * Simply blast Read requests at a given queue
   * 
   * @param itf 
   * @param queue 
   */
  void blast_loop()
  {
    const unsigned BATCH_SIZE = 256;
    Exokernel::Device * dev = itf_->get_device();
    addr_t phys = 0;
    unsigned long count = 0;

    void * p = dev->alloc_dma_pages(BATCH_SIZE,
                                    &phys,
                                    Exokernel::Device_sysfs::DMA_FROM_DEVICE);
    assert(p);


    io_descriptor_t* io_desc = (io_descriptor_t *) malloc(sizeof(io_descriptor_t) * BATCH_SIZE);
  
    for(unsigned b=0;b<BATCH_SIZE;b++) {
      io_desc[b].action = NVME_READ;
      io_desc[b].buffer_virt = p;
      io_desc[b].buffer_phys = phys;
      io_desc[b].num_blocks = 1;
    }

    uint64_t counter = 0;
    Notify *notify = new Notify_Async();

    while(!blast_exit) {

#if 1 // batch version
      for(unsigned b=0;b<BATCH_SIZE;b++) {
        io_desc[b].offset = genrand64_int64() % (max_lba_ - 8);
      }

      status_t st = itf_->async_io_batch((io_request_t *)io_desc, 
                                         BATCH_SIZE, 
                                         notify, 
                                         queue_, 0);

      total_ops+=BATCH_SIZE;

#else // non-batch version
      io_desc[0].offset = genrand64_int64() % (max_lba_ - 8);
      //      printf("reading offset %ld\n",io_desc[0].offset);
      itf_->async_io((io_request_t)&io_desc[0], 
                    notify, 
                    queue_ /*queue/port*/, 0 /* device */);
      total_ops++;
#endif

      if(total_ops % 100000 == 0)
        printf("Op count: %ld\n",total_ops);

    }

    // wait for batch?
    itf_->wait_io_completion(queue_); //wait for IO completion     

    dev->free_dma_pages(p);
  }

};


/** 
 * Main entry to create multiple blasting threads
 * 
 * @param itf 
 * @param max_lba 
 */
void read_blast(IBlockDevice * itf, off_t max_lba)
{
#define NUM_QUEUES 8
#define TIME_DURATION_SEC 10

  //  pthread_t threads[NUM_QUEUES];
  ReadBlasterThread * threads[NUM_QUEUES];

  for(unsigned i=0;i<NUM_QUEUES;i++) {

    threads[i] = new ReadBlasterThread((i*2)+21 /* core */,
                                   itf,
                                   i+1, /* queue */
                                   max_lba);
    threads[i]->start();
  }
  sleep(TIME_DURATION_SEC);

  /* signal threads to exit */
  blast_exit = true;

  /* join threads */
  for(unsigned i=0;i<NUM_QUEUES;i++) {
    threads[i]->join();
  }

  printf("Random Read Rate: %ld IOPS\n", total_ops / TIME_DURATION_SEC);
}


void verify_blast(IBlockDevice * itf, off_t max_lba)
{
#define NUM_QUEUES 8
#define TIME_DURATION_SEC 10

  //  pthread_t threads[NUM_QUEUES];
  VerifyBlasterThread * threads[NUM_QUEUES];

  for(unsigned i=0;i<NUM_QUEUES;i++) {

    threads[i] = new VerifyBlasterThread((i*2)+21 /* core */,
                                   itf,
                                   i+1, /* queue */
                                   max_lba);
    threads[i]->start();
  }
  sleep(TIME_DURATION_SEC);

  /* signal threads to exit */
  blast_exit = true;

  /* join threads */
  for(unsigned i=0;i<NUM_QUEUES;i++) {
    threads[i]->join();
  }

  printf("RW Rate: %ld IOPS\n", total_ops / TIME_DURATION_SEC);

}

#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <common/utils.h>
#include <common/dump_utils.h>

#include "nvme_drv_component.h"
#include "nvme_device.h"
#include "nvme_types.h"

Exokernel::Spin_lock total_ops_lock;
static uint64_t total_ops  __attribute__((aligned(8))) = 0;
static bool blast_exit = false;

class Notify_Async_Cnt : public Notify {
private:
  unsigned _id;
  bool _free_notify_obj;
  uint64_t *_cnt;
public:
  Notify_Async_Cnt(uint64_t *cnt) : _cnt(cnt), _id(0), _free_notify_obj(false) {}
  ~Notify_Async_Cnt() {}

  void wait() {
    PERR(" Should not be called !!");
    assert(false);
  }

  bool free_notify_obj() {
    return _free_notify_obj;
  }

  void action() {
    //(*_cnt)++;
    PLOG(" Notification called (id = %u) !!", _id);
  }
};

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

  const unsigned BATCH_SIZE = 8;    
  io_descriptor_t* io_desc = (io_descriptor_t *) malloc(sizeof(io_descriptor_t) * BATCH_SIZE);
  
  for(unsigned b=0;b<BATCH_SIZE;b++) {
    io_desc[b].action = NVME_READ;
    io_desc[b].buffer_virt = p;
    io_desc[b].buffer_phys = phys;
    io_desc[b].num_blocks = 8;
  }

  uint64_t counter = 0;
  Notify *notify = new Notify_Async_Cnt(&counter);

  while(!blast_exit) {

#if 1
    for(unsigned b=0;b<BATCH_SIZE;b++) {
      io_desc[b].offset = rdtsc() % (max_lba - 8);
    }
    status_t st = itf->async_io_batch((io_request_t *)io_desc, BATCH_SIZE, notify, queue, 0);
    total_ops_lock.lock();
    total_ops+=BATCH_SIZE;
    total_ops_lock.unlock();
#else
    io_desc[0].offset = rdtsc() % (max_lba - 8);
    itf->sync_io((io_request_t)&io_desc[0], queue /*queue/port*/, 0 /* device */);
    total_ops++;
#endif


    //    

    if(total_ops % 10000 == 0) {
      printf("Op count: %ld\n",total_ops);
    }
    //    total_ops_lock.unlock();    

    //    itf->wait_io_completion(queue); //wait for IO completion
  }



  //  dev->free_dma_pages(p);
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
#define NUM_QUEUES 16
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
  printf("Rate: %ld IOPS\n", total_ops / TIME_DURATION_SEC);
}

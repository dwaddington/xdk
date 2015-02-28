#include <stdio.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <common/cycles.h>
#include <common/dump_utils.h>
#include <component/base.h>
#include <exo/rand.h>

//random generator
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "nvme_drv_component.h"
#include "nvme_device.h"
#include "nvme_types.h"

//#define COUNT (1000000)
#define COUNT (409600)
#define IO_PER_BATCH (1)
#define NUM_QUEUES (2)
#define SLAB_SIZE (2048)
#define NUM_BLOCKS (8)

#define RANDOM_IO

class Read_thread : public Exokernel::Base_thread {
  private:
    IBlockDevice * _itf;
    NVME_device * _dev;
    unsigned _qid;
    Notify_object nobj;
    addr_t* _phys_array_local;
    void ** _virt_array_local;

    void read_throughput_test()
    {
      using namespace Exokernel;

      cpu_time_t start_ts = rdtsc();
      NVME_IO_queue * ioq = _dev->io_queue(_qid);
      PLOG("** READ THROUGHPUT AROUND Q:%p (_qid=%u)\n",(void *) ioq, _qid);

      unsigned long long mean_cycles = 0;
      unsigned long long total_cycles = 0;
      unsigned long long sample_count = 0;
      cpu_time_t prev_tsc, diff_tsc, cur_tsc;
      const cpu_time_t drain_tsc = (unsigned long long)get_tsc_frequency_in_mhz() * 1000000UL;
      //printf("drain_tsc = %lu\n", drain_tsc);
      prev_tsc = rdtsc();

      io_descriptor_t* io_desc = (io_descriptor_t *)malloc(IO_PER_BATCH * sizeof(io_descriptor_t));

      Notify *notify = new Notify_Async(false);

#ifdef RANDOM_IO
      //boost::mt19937 rng;
      boost::random::random_device rng;
      boost::random::uniform_int_distribution<> rnd_page_offset(1, 32*1024*1024); //max 781,422,768 sectors
#endif

      for(unsigned long i=0;i<COUNT;i++) {

        if(i%1000 == 0) printf("count = %lu (Q: %u)\n", i, _qid);

        cpu_time_t start = rdtsc();

        //Notify *notify = new Notify_Async(i, true);
        memset(io_desc, 0, IO_PER_BATCH * sizeof(io_descriptor_t));
        //prepare io descriptors
        unsigned npages_per_io = (NVME::BLOCK_SIZE)*NUM_BLOCKS/PAGE_SIZE;
        for(unsigned long j = 0; j < IO_PER_BATCH; j++) {
          io_desc[j].action = NVME_READ;
          unsigned idx = (i*IO_PER_BATCH+j)*npages_per_io % SLAB_SIZE;
          io_desc[j].buffer_virt = _virt_array_local[idx];
          io_desc[j].buffer_phys = _phys_array_local[idx];
#ifdef RANDOM_IO
          io_desc[j].offset = PAGE_SIZE * rnd_page_offset(rng);
#else
          io_desc[j].offset = PAGE_SIZE * npages_per_io * ( ((_qid-1)*COUNT*IO_PER_BATCH) + (i*IO_PER_BATCH+j) );
#endif
          io_desc[j].num_blocks = NUM_BLOCKS;

          PLOG("phys_addr[%lu][%lu] = 0x%lx, offset = %ld", i, j, io_desc[j].buffer_phys, io_desc[j].offset);
        }

        status_t st = _itf->async_io_batch((io_request_t*)io_desc, IO_PER_BATCH, notify, _qid);

        PLOG("sent %d blocks (%u pages) in iteration = %lu (Q:%u) \n", IO_PER_BATCH*NUM_BLOCKS, IO_PER_BATCH*npages_per_io, i, _qid);

        /* collect stats */
        cpu_time_t delta = rdtsc() - start;

        if(sample_count > 0) {
          total_cycles += delta;
          sample_count += (IO_PER_BATCH*NUM_BLOCKS);
          mean_cycles = total_cycles/sample_count;
        } else {
          total_cycles = delta;
          mean_cycles = delta;
          sample_count = IO_PER_BATCH*NUM_BLOCKS;
        }
      }

      _itf->wait_io_completion(_qid); //wait for IO completion

      cpu_time_t end_ts = rdtsc();

      PLOG("all sends complete (Q:%u).", _qid);

      ioq->dump_stats();

      PLOG("(QID:%u) cycles mean/iteration %llu (%llu IOPS)\n",_qid, mean_cycles, (((unsigned long long)get_tsc_frequency_in_mhz())*1000000)/mean_cycles);

    }


  public:
    Read_thread(IBlockDevice * itf, unsigned qid, core_id_t core, addr_t *phys_array_local, void **virt_array_local) : 
      _qid(qid), _itf(itf), Exokernel::Base_thread(NULL,core), _phys_array_local(phys_array_local), _virt_array_local(virt_array_local) {
        _dev = (NVME_device*)(itf->get_device());
      }

    void* entry(void* param) {
      assert(_dev);

      std::stringstream str;
      str << "rt-client-qt-" << _qid;
      Exokernel::set_thread_name(str.str().c_str());

      read_throughput_test();
    }

};

class mt_tests {

  int h; /*mem handler*/
  void* vaddr;
  /* set up a simple slab */
  addr_t phys_array[NUM_QUEUES][SLAB_SIZE];
  void * virt_array[NUM_QUEUES][SLAB_SIZE];
  Exokernel::Pagemap pm;

  private:
    void allocMem() {

      using namespace Exokernel;
      /* allocate slab of memory */
      int NUM_PAGES = NUM_QUEUES * (SLAB_SIZE+2);
      Memory::huge_system_configure_nrpages(NUM_PAGES);
      vaddr = Memory::huge_shmem_alloc(1,PAGE_SIZE * NUM_PAGES,0,&h);
      assert(vaddr);

      char * p = (char *) vaddr;
      for(unsigned i=0; i<NUM_QUEUES; i++) {
        for(unsigned j=0; j<SLAB_SIZE; j++) {
          virt_array[i][j] = p;
          phys_array[i][j] = pm.virt_to_phys(virt_array[i][j]);
          p+=PAGE_SIZE;
          assert(virt_array[i][j]);
          assert(phys_array[i][j]);
          //printf("[%d, %d]: virt = %p, phys = %p\n", i, j, virt_array[i][j], phys_array[i][j]);
        }
      }
      touch_pages(vaddr, PAGE_SIZE*NUM_PAGES); /* eager map */
      PLOG("Memory allocated OK!!");
    }

    void freeMem() {
      Exokernel::Memory::huge_shmem_free(vaddr,h);
      PLOG("Memory freed OK!!");
    }

  public:
    void runTest(IBlockDevice * itf) {
      //NVME_INFO("Issuing test read and write test...\n");

      allocMem();

      {
        Read_thread * thr[NUM_QUEUES];

#define N_QUEUE_MAP 8
        unsigned cq_map[N_QUEUE_MAP] = {4, 8, 12, 16, 20, 24, 28, 32};

        for(unsigned i=1;i<=NUM_QUEUES;i++) {
          assert(NUM_QUEUES <= N_QUEUE_MAP);

          printf("********** i = %u, cq_map = %u \n", i, cq_map[i-1]);

          thr[i-1] = new Read_thread(itf,
              i, //(2*(i-1))+1, /* qid */
              cq_map[i-1],
              //i+3, //*2-1); //1,3,5,7 /* core for read thread */
              phys_array[i-1],
              virt_array[i-1]
              );
        }

        struct rusage ru_start, ru_end;
        struct timeval tv_start, tv_end;
        gettimeofday(&tv_start, NULL);
        getrusage(RUSAGE_SELF,&ru_start);

        for(unsigned i=1;i<=NUM_QUEUES;i++)
          thr[i-1]->start();

        for(unsigned i=1;i<=NUM_QUEUES;i++) {
          PLOG("!!!!!!!!!!!!!! joined read thread %p",thr[i-1]);
          thr[i-1]->join();
        }

        PLOG("All read threads joined.");

        //Wait for IO completion
        //This is done inside each thread by calling io_suspend()

        getrusage(RUSAGE_SELF,&ru_end);
        gettimeofday(&tv_end, NULL);

        unsigned io_size_in_KB = (NVME::BLOCK_SIZE)*NUM_BLOCKS/1024; //xK
        assert((NVME::BLOCK_SIZE)*NUM_BLOCKS % 1024 == 0);
        uint64_t total_io = COUNT * IO_PER_BATCH * (io_size_in_KB/4) * NUM_QUEUES;
        printf("COUNT = %u, IO_PER_BATCH = %u, NUM_QUEUES = %u, total_io = %lu\n", (unsigned)COUNT, (unsigned)IO_PER_BATCH, (unsigned)NUM_QUEUES, total_io);

        double delta = ((ru_end.ru_utime.tv_sec - ru_start.ru_utime.tv_sec) * 1000000.0) +
          ((ru_end.ru_utime.tv_usec - ru_start.ru_utime.tv_usec));

        /* add kernel time */
        delta += ((ru_end.ru_stime.tv_sec - ru_start.ru_stime.tv_sec) * 1000000.0) +
          ((ru_end.ru_stime.tv_usec - ru_start.ru_stime.tv_usec));

        float usec_per_queue = delta/NUM_QUEUES;
        printf("getrusage: Time for %lu 4K reads is: %.5g sec\n", total_io, usec_per_queue / 1000000.0);
        float usec_per_io = usec_per_queue * 1.0 / total_io;
        printf("getrusage: Rate: %.2f KIOPS (%.2f usec per I/O)\n", total_io*1.0 / (usec_per_queue/1000), usec_per_io);

        PLOG("Voluntary ctx switches = %ld",ru_end.ru_nvcsw - ru_start.ru_nvcsw);
        PLOG("Involuntary ctx switches = %ld",ru_end.ru_nivcsw - ru_start.ru_nivcsw);
        PLOG("Page-faults = %ld",ru_end.ru_majflt - ru_start.ru_majflt);

        double gtod_delta = ((tv_end.tv_sec - tv_start.tv_sec) * 1000000.0) +
          ((tv_end.tv_usec - tv_start.tv_usec));
        double gtod_secs = (gtod_delta) / 1000000.0;
        printf("gettimeofday delta %.5g sec (rate=%.2f KIOPS)\n", gtod_secs, total_io*1.0 / (gtod_secs * 1000) );
      }
      freeMem();
    }
};

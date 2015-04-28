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

#define MAX_LBA (64*11*1024*1024) //= 738,197,504; max 781,422,768 sectors

#define COUNT (1024000 * 4)
#define NUM_QUEUES (2)
#define SLAB_SIZE (512)
#define IO_PER_BATCH (1)
#define NUM_BLOCKS (8)//(8*32)
#define NUMA_NODE (0)

#define TEST_RANDOM_IO  //otherwise, sequential io
#define TEST_IO_READ    //otherwise, write

class IO_thread : public Exokernel::Base_thread {
  private:
    IBlockDevice * _itf;
    NVME_device * _dev;
    unsigned _qid;
    Notify_object nobj;
    addr_t* _phys_array_local;
    void ** _virt_array_local;

    /**
     * A simple notification callback
     */
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
     * I/O Throughput measurement without considering batch
     */
    void io_throughput_test()
    {
      using namespace Exokernel;
      assert(IO_PER_BATCH == 1);

      NVME_IO_queue * ioq = _dev->io_queue(_qid);

      //slab size should be large enough, so that there is no overlap. only for testing purpose
      assert(SLAB_SIZE >= 4*(ioq->queue_length()));
      io_descriptor_t* io_desc = (io_descriptor_t *)malloc(SLAB_SIZE * sizeof(io_descriptor_t));

      const uint64_t N_PAGES = COUNT;
#ifdef TEST_RANDOM_IO
      boost::random::random_device rng;
      boost::random::uniform_int_distribution<> rnd_page_offset(1, MAX_LBA/NUM_BLOCKS-1); //PAGE offset
#endif

      uint64_t counter = 0;
      Notify *notify = new Notify_Async_Cnt(&counter);
      unsigned n_lba_partitions = round_up_log2(NUM_QUEUES);

      for(unsigned idx = 0; idx < N_PAGES; idx++) {

        if(idx%100000 == 0) printf("count = %u (Q: %u)\n", idx, _qid);

        io_descriptor_t* io_desc_ptr = io_desc + idx % SLAB_SIZE ;
        memset(io_desc_ptr, 0, sizeof(io_descriptor_t));

#ifdef TEST_IO_READ
        io_desc_ptr->action = NVME_READ;
#else /* Write */
        io_desc_ptr->action = NVME_WRITE;
#endif
        unsigned slab_idx_local = idx % SLAB_SIZE;
        io_desc_ptr->buffer_virt = _virt_array_local[slab_idx_local];
        io_desc_ptr->buffer_phys = _phys_array_local[slab_idx_local];
#ifdef TEST_RANDOM_IO
        io_desc_ptr->offset = rnd_page_offset(rng) * NUM_BLOCKS;
#else /* sequential IO */
        //This may be slower than random io, as io operations are not distributed in different SSD channels
        //Instead, use larger io size for sequential io to maximize the bandwidth
        io_desc_ptr->offset = ((uint64_t)(MAX_LBA/n_lba_partitions)*(_qid-1) + idx*NUM_BLOCKS) % MAX_LBA;
#endif
        assert(io_desc_ptr->offset < MAX_LBA);
        io_desc_ptr->num_blocks = NUM_BLOCKS;

        //memset(io_desc_ptr->buffer_virt, 0, PAGE_SIZE);
        //hexdump(io_desc_ptr->buffer_virt, 32);

        status_t st = _itf->async_io_batch((io_request_t*)io_desc_ptr, 1, notify, _qid);
      }
      _itf->wait_io_completion(_qid); //wait for IO completion

      PLOG("all sends complete (Q:%u).", _qid);

      unsigned block_size_in_K= (NVME::BLOCK_SIZE)*NUM_BLOCKS/1024;
      //assert(counter == COUNT);
      printf("** Total %uK I/O = %lu (Q: %u)\n", block_size_in_K, counter, _qid);
      free(io_desc);
      delete notify;
    }


    /**
     * Throuput measurement with considering batch submission
     */
    void io_throughput_test_batch()
    {
      using namespace Exokernel;

      NVME_IO_queue * ioq = _dev->io_queue(_qid);
      PLOG("** READ THROUGHPUT AROUND Q:%p (_qid=%u)\n",(void *) ioq, _qid);

      unsigned long long mean_cycles = 0;
      unsigned long long total_cycles = 0;
      unsigned long long sample_count = 0;

      //slab size should be large enough, so that there is no overlap. only for testing purpose
      assert(SLAB_SIZE > 4*(ioq->queue_length()));
      io_descriptor_t* io_desc = (io_descriptor_t *)malloc(SLAB_SIZE * sizeof(io_descriptor_t));

#ifdef TEST_RANDOM_IO
      boost::random::random_device rng;
      boost::random::uniform_int_distribution<> rnd_page_offset(1, 64*1024*1024-1); //offset based on page; 781,422,768 sectors(logical block) in total
#endif

      uint64_t counter = 0;
      Notify *notify = new Notify_Async_Cnt(&counter);
      //Notify *notify = new Notify_Async(false);
      unsigned npages_per_io = (NVME::BLOCK_SIZE)*NUM_BLOCKS/PAGE_SIZE;
      //unsigned io_req_idx = (_qid-1)*COUNT*IO_PER_BATCH;
      unsigned io_req_idx = 0;

      for(unsigned long i=0;i<COUNT;i++) {

        if(i%100000 == 0) printf("count = %lu (Q: %u)\n", i, _qid);

        cpu_time_t start = rdtsc();

        //prepare io descriptors
        io_descriptor_t* io_desc_base = io_desc + (io_req_idx%SLAB_SIZE);
        assert((io_req_idx % SLAB_SIZE) + IO_PER_BATCH - 1 < SLAB_SIZE);

        for(unsigned long j = 0; j < IO_PER_BATCH; j++) {
          io_descriptor_t* io_desc_ptr = io_desc_base + j;
          memset(io_desc_ptr, 0, sizeof(io_descriptor_t));

#ifdef TEST_IO_READ
          io_desc_ptr->action = NVME_READ;
#else /*WRITE*/
          io_desc_ptr->action = NVME_WRITE;
#endif
          unsigned slab_idx_local = (i*IO_PER_BATCH+j)*npages_per_io%SLAB_SIZE;
          //unsigned slab_idx_local = io_req_idx % SLAB_SIZE;
          io_desc_ptr->buffer_virt = _virt_array_local[slab_idx_local];
          io_desc_ptr->buffer_phys = _phys_array_local[slab_idx_local];
#ifdef TEST_RANDOM_IO
          assert(NUM_BLOCKS == 8);
          io_desc_ptr->offset = rnd_page_offset(rng) * (PAGE_SIZE/NVME::BLOCK_SIZE);
#else
          io_desc_ptr->offset = PAGE_SIZE * npages_per_io * io_req_idx;
          unsigned n_partitions = round_up_log2(NUM_QUEUES);
          io_desc_ptr->offset = ((uint64_t)(MAX_LBA/n_partitions)*(_qid-1) + io_req_idx*npages_per_io*NUM_BLOCKS) % MAX_LBA;
#endif
          assert(io_desc_ptr->offset < MAX_LBA);
          io_desc_ptr->num_blocks = NUM_BLOCKS;

          io_req_idx++;
        }

        status_t st = _itf->async_io_batch((io_request_t*)io_desc_base, IO_PER_BATCH, notify, _qid);

        PLOG("sent %d blocks (%u pages) in iteration = %lu (Q:%u) \n", IO_PER_BATCH*npages_per_io*NUM_BLOCKS, IO_PER_BATCH*npages_per_io, i, _qid);

#if 0
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
#endif
      }
      assert( io_req_idx == COUNT*IO_PER_BATCH);
      _itf->wait_io_completion(_qid); //wait for IO completion

      PLOG("all sends complete (Q:%u).", _qid);

      printf("** Total 4K I/O = %lu (Q: %u)\n", counter * npages_per_io * IO_PER_BATCH, _qid);
#if 0
      ioq->dump_stats();

      PLOG("(QID:%u) cycles mean/iteration %llu (%llu IOPS)\n",_qid, mean_cycles, (((unsigned long long)get_tsc_frequency_in_mhz())*1000000)/mean_cycles);
#endif
      free(io_desc);
      delete notify;
    }


  public:
    IO_thread(IBlockDevice * itf, unsigned qid, core_id_t core, addr_t *phys_array_local, void **virt_array_local) : 
      _qid(qid), _itf(itf), Exokernel::Base_thread(NULL,core), _phys_array_local(phys_array_local), _virt_array_local(virt_array_local) {
        _dev = (NVME_device*)(itf->get_device());
      }

    void* entry(void* param) {
      assert(_dev);

      std::stringstream str;
      str << "io-client-qt-" << _qid;
      Exokernel::set_thread_name(str.str().c_str());

      io_throughput_test();
      //io_throughput_test_batch();//v2
    }

};

#define SHMEM_HUGEPAGE_ALLOC 0
#define DMA_HUGEPAGE_ALLOC 0
#define DMA_PAGE_ALLOC 1

class mt_tests {

  int h; /*mem handler*/
  void* vaddr;
  /* set up a simple slab */
  addr_t phys_array[NUM_QUEUES][SLAB_SIZE];
  void * virt_array[NUM_QUEUES][SLAB_SIZE];
  Exokernel::Pagemap pm;
  std::vector<void *> hugepage_list;
  std::vector<void *> page_list;

  private:
#if (SHMEM_HUGEPAGE_ALLOC == 1)
    void allocMem(IBlockDevice * itf) {

      assert(NUM_BLOCKS == 8);
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

    void freeMem(IBlockDevice * itf) {
      Exokernel::Memory::huge_shmem_free(vaddr,h);
      PLOG("Memory freed OK!!");
    }
#elif (DMA_HUGEPAGE_ALLOC == 1)
    void allocMem(IBlockDevice * itf) {

      assert(NUM_BLOCKS == 8);
      using namespace Exokernel;
      Device * dev = itf->get_device();

      /* note Linux off-the-shelf is limited to 4MB contiguous memory */
      addr_t paddr;
      void* vaddr;
      char *p_v, *p_p;
      unsigned page_count= 0;
      const unsigned n_page_per_hugepage = HUGE_PAGE_SIZE/PAGE_SIZE;
      for(unsigned i=0; i<NUM_QUEUES; i++) {
        for(unsigned j=0; j<SLAB_SIZE; j++) {
          if(page_count % n_page_per_hugepage == 0) {
            vaddr = dev->alloc_dma_huge_pages(1, &paddr, 0, MAP_HUGETLB);
            hugepage_list.push_back(vaddr);
            p_v = (char *)vaddr;
            p_p = (char *)paddr;
            touch_pages(vaddr, HUGE_PAGE_SIZE); /* eager map */
          }
          virt_array[i][j] = p_v;
          phys_array[i][j] = (addr_t)p_p;
          assert(virt_array[i][j]);
          assert(phys_array[i][j]);
          //printf("[%d, %d]: virt = %p, phys = %lx\n", i, j, virt_array[i][j], phys_array[i][j]);

          p_v += PAGE_SIZE;
          p_p += PAGE_SIZE;
          page_count++;
        }
      }
      PLOG("Memory allocated OK!!");
    }

    void freeMem(IBlockDevice * itf) {
      using namespace Exokernel;
      Device * dev = itf->get_device();
      for(unsigned i = 0; i < hugepage_list.size(); i++) {
        dev->free_dma_huge_pages(hugepage_list[i]);
      }
      PLOG("Memory freed OK!!");
    }
#elif (DMA_PAGE_ALLOC == 1)
    void allocMem(IBlockDevice * itf) {
      using namespace Exokernel;
      Device * dev = itf->get_device();

      //assert(SLAB_SIZE == 1024);
      for(unsigned i=0; i<NUM_QUEUES; i++) {

        assert((NUM_BLOCKS * NVME::BLOCK_SIZE)%PAGE_SIZE == 0);
        unsigned n_pages_per_io = NUM_BLOCKS * NVME::BLOCK_SIZE / PAGE_SIZE;

#define NUM_PAGES_PER_ALLOC 1024
        void *p;
        char *p_char;
        addr_t phys_addr=0;
        int n_pages_left = 0;
        for(unsigned j=0; j<SLAB_SIZE; j++) {
          if(n_pages_left <= 0) {
            //there is problem if we alloc too many pages at once
            //so we alloc these pages in multiple times
            p = dev->alloc_dma_pages(NUM_PAGES_PER_ALLOC, &phys_addr, NUMA_NODE, 0); 
            page_list.push_back(p);
            memset(p, 0, PAGE_SIZE*NUM_PAGES_PER_ALLOC);

            p_char = (char*)p;
            n_pages_left = NUM_PAGES_PER_ALLOC;
          }
          virt_array[i][j] = p_char;
          phys_array[i][j] = phys_addr;
          p_char += PAGE_SIZE*n_pages_per_io;
          phys_addr += PAGE_SIZE*n_pages_per_io;
          //printf("[%d, %d]: virt = %p, phys = %lx\n", i, j, virt_array[i][j], phys_array[i][j]);
          n_pages_left -= n_pages_per_io;
        }
      }
      PLOG("Memory allocated OK!!");
    }
    void freeMem(IBlockDevice * itf) {
      using namespace Exokernel;
      Device * dev = itf->get_device();
      for(unsigned i = 0; i < page_list.size(); i++) {
        dev->free_dma_pages(page_list[i]);
      }
      PLOG("Memory freed OK!!");
    }
#else
    void allocMem(IBlockDevice * itf) {
      assert(false);
    }
    void freeMem(IBlockDevice * itf) {
      assert(false);
    }
#endif

  public:
    void runTest(IBlockDevice * itf) {
      //NVME_INFO("Issuing test read and write test...\n");

      const cpu_time_t tsc_per_sec = (unsigned long long)get_tsc_frequency_in_mhz() * 1000000UL;
      PLOG("tsc_per_sec = %lu", tsc_per_sec);

      allocMem(itf);

      {
        IO_thread * thr[NUM_QUEUES];

//#define N_QUEUE_MAP 8
//        unsigned sq_map[N_QUEUE_MAP] = {4, 8, 12, 16, 20, 24, 28, 32};

        Config config = ((NVME_device*)(itf->get_device()) )->config();
        assert(NUM_QUEUES <= config.num_io_queues());

        for(unsigned i = 1; i <= NUM_QUEUES; i++) {

          unsigned sq_core = config.get_sub_core_from_qid(i);
          printf("********** qid = %u, core = %u\n", i, sq_core);

          thr[i-1] = new IO_thread(itf,
              i,                         /* qid */
              sq_core,                   /* sq core */
              phys_array[i-1],
              virt_array[i-1]
              );
        }

        struct rusage ru_start, ru_end;
        struct timeval tv_start, tv_end;
        gettimeofday(&tv_start, NULL);
        getrusage(RUSAGE_SELF,&ru_start);
        cpu_time_t start_tsc = rdtsc();

        for(unsigned i=1;i<=NUM_QUEUES;i++)
          thr[i-1]->start();

        for(unsigned i=1;i<=NUM_QUEUES;i++) {
          PLOG("!!!!!!!!!!!!!! joined read thread %p",thr[i-1]);
          thr[i-1]->join();
        }

        PLOG("All read threads joined.");

        //Wait for IO completion
        //This is done inside each thread by calling wait_io_completion()

        cpu_time_t end_tsc = rdtsc();
        getrusage(RUSAGE_SELF,&ru_end);
        gettimeofday(&tv_end, NULL);

        assert((NVME::BLOCK_SIZE)*NUM_BLOCKS % 1024 == 0);
        unsigned io_size_in_KB = (NVME::BLOCK_SIZE)*NUM_BLOCKS/1024; //xK
        assert(io_size_in_KB % 4 == 0);
        uint64_t total_io = COUNT * IO_PER_BATCH * (io_size_in_KB/4) * NUM_QUEUES;
        printf("**********************\n");

        printf("COUNT = %u, IO_PER_BATCH = %u, NUM_QUEUES = %u, NUM_BLOCKS = %u, total_io = %lu\n", (unsigned)COUNT, (unsigned)IO_PER_BATCH, (unsigned)NUM_QUEUES, (unsigned)NUM_BLOCKS, total_io);

        /*use gettimeofday*/
        double gtod_delta = ((tv_end.tv_sec - tv_start.tv_sec) * 1000000.0) +
          ((tv_end.tv_usec - tv_start.tv_usec));
        double gtod_secs = (gtod_delta) / 1000000.0;
        printf("gettimeofday: \n\tRate = %.2f KIOPS (B/W: %.2fG/s), Time = %.5g sec (%.2f usec per I/O)\n", total_io*1.0 / (gtod_secs * 1000), total_io*1.0 / (gtod_secs * 1000)*4/1024, gtod_secs, gtod_delta/total_io);

        /*use rdtsc*/
        double tsc_secs = (end_tsc - start_tsc)*1.0 / tsc_per_sec;
        printf("rdtsc: \n\tRate = %.2f KIOPS (B/W: %.2fG/s), Time = %.5g sec (%.2f usec per I/O)\n", total_io*1.0 / (tsc_secs * 1000), total_io*1.0 / (tsc_secs * 1000)*4/1024, tsc_secs, tsc_secs*1000000/total_io);

        /*use getrusage*/
        double delta_user = ((ru_end.ru_utime.tv_sec - ru_start.ru_utime.tv_sec) * 1000000.0) +
          ((ru_end.ru_utime.tv_usec - ru_start.ru_utime.tv_usec));

        /* add kernel time */
        double delta_system = ((ru_end.ru_stime.tv_sec - ru_start.ru_stime.tv_sec) * 1000000.0) +
          ((ru_end.ru_stime.tv_usec - ru_start.ru_stime.tv_usec));

        double delta = delta_user + delta_system;
        float usec_per_queue = delta/NUM_QUEUES;
        float usec_per_io = usec_per_queue * 1.0 / total_io;
        printf("getrusage: \n\tRate = %.2f KIOPS, Time = %.5g sec (%.2f usec per I/O)(user: %.2f sec, system: %.2f sec)\n", total_io*1.0 / (usec_per_queue/1000), usec_per_queue / 1000000.0, usec_per_io, delta_user/NUM_QUEUES/1000000, delta_system/NUM_QUEUES/1000000);

        PLOG("Voluntary ctx switches = %ld",ru_end.ru_nvcsw - ru_start.ru_nvcsw);
        PLOG("Involuntary ctx switches = %ld",ru_end.ru_nivcsw - ru_start.ru_nivcsw);
        PLOG("Page-faults = %ld",ru_end.ru_majflt - ru_start.ru_majflt);

      }

      freeMem(itf);
    }
};

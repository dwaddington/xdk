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
//#include <common/utils.h>
#include <boost/atomic.hpp>

//random generator
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "nvme_drv_component.h"
#include "nvme_device.h"
#include "nvme_types.h"

#undef MAX_LBA
#undef COUNT_VERIFY
#undef NUM_QUEUES 
#undef SLAB_SIZE
#undef NUM_BLOCKS
#undef SHMEM_HUGEPAGE_ALLOC
#undef DMA_HUGEPAGE_ALLOC
#undef DMA_PAGE_ALLOC
#undef DMA_PAGE_ALLOC_SINGLE
#undef TEST_RANDOM_IO
#undef SCALE
#undef VERIFY_FILL
#undef ASYNC_FILL

#define MAX_LBA (97677846) //actually, max 781,422,768 sectors
#define COUNT_VERIFY (64)
#define NUM_QUEUES (1)
#define SLAB_SIZE (4096)
#define NUM_BLOCKS (8)

#define CONST_MARKER 0x7d

//#define MemBarrier() asm volatile("mfence":::"memory") //only compiler fence
#define MemBarrier()  atomic_thread_fence(boost::memory_order_seq_cst)


/********************************************************************
 * Verify correctness. We first write blocks (with markers) to SSD
 * then read blocks for verification. 
 * In particular, the callback is responsible for comparing content
 ********************************************************************/
class Verify_thread : public Exokernel::Base_thread {
  private:
    IBlockDevice * _itf;
    NVME_device * _dev;
    unsigned _qid;
    addr_t* _phys_array_local;
    void ** _virt_array_local;

    class Notify_Verify : public Notify {
      private:
        void* _p;
        uint8_t _content;
        unsigned _size;
        off_t _offset;
        off_t _page_offset;
        bool _isRead;

      public:
        Notify_Verify(off_t offset, off_t page_offset, void* p, uint8_t content, unsigned size, bool isRead) : _offset(offset), _page_offset(page_offset), _p(p), _content(content),_size(size), _isRead(isRead) {}
        ~Notify_Verify() {}

        /* the action of callback */
        void action() {
          PTEST("(%s) lba = %lu(0x%lx) page_offset = %lu(0x%lx) (marker: 0x%x%x), virt_p = %p", _isRead?"READ":"WRITE", _offset, _offset, _page_offset, _page_offset, 0xf & (_content >> 4), 0xf & _content, _p);

          for(unsigned i = 0; i<_size; i++) {
            if(((uint8_t*)_p)[i]!=_content) {
              PTEST("FAIL(%s) lba = %lu(0x%lx) page_offset = %lu(0x%lx) (marker: 0x%x%x), virt_p = %p", _isRead?"READ":"WRITE", _offset, _offset, _page_offset, _page_offset, 0xf & (_content >> 4), 0xf & _content, _p);
              hexdump(((uint8_t*)_p), _size);
              return;
            }
            assert(((uint8_t*)_p)[i]==_content);
          }
        }

        void wait() {}

        bool free_notify_obj() {
          return true;
        }

    };

    /**
     * Verification 
     */
    void verify()
    {
      using namespace Exokernel;

      assert(_qid == 1);
      assert(NUM_QUEUES == 1);
      assert(NUM_BLOCKS <= 8);

      NVME_IO_queue * ioq = _dev->io_queue(_qid);

      assert(SLAB_SIZE > 4*(ioq->queue_length()));
      io_descriptor_t* io_desc = (io_descriptor_t *)malloc(SLAB_SIZE * sizeof(io_descriptor_t));

      uint64_t N_PAGES = COUNT_VERIFY;
      assert(N_PAGES < MAX_LBA/8);

      /* use the same dma memory to fill the same content */
      memset(_virt_array_local[0], CONST_MARKER, NVME::BLOCK_SIZE * NUM_BLOCKS);
      MemBarrier();

      printf("SQ sleeping ........................\n");
      sleep(4);

#define SCALE 1
#define VERIFY_FILL
#ifdef VERIFY_FILL
      /* Fill pages into SSD first */
      PLOG("** Fill pages before verify");
      for(unsigned idx = 0; idx < N_PAGES; idx++) {

        /* construct io descriptor */
        io_descriptor_t* io_desc_ptr = io_desc + idx % SLAB_SIZE;
        memset(io_desc_ptr, 0, sizeof(io_descriptor_t));

        io_desc_ptr->action = NVME_WRITE;
        //unsigned buffer_idx = (2*idx+512)%SLAB_SIZE;
        //io_desc_ptr->buffer_virt = _virt_array_local[buffer_idx];
        //io_desc_ptr->buffer_phys = _phys_array_local[buffer_idx];
        
        /* use the same DMA memory -- write same data to all pages */
        io_desc_ptr->buffer_virt = _virt_array_local[0];
        io_desc_ptr->buffer_phys = _phys_array_local[0];
        io_desc_ptr->offset = idx * NUM_BLOCKS * SCALE;
        io_desc_ptr->num_blocks = NUM_BLOCKS;

        //uint8_t marker = (uint8_t)((io_desc_ptr->offset/NUM_BLOCKS) & 0xff);
        uint8_t marker = CONST_MARKER;

        //memset(io_desc_ptr->buffer_virt, marker, NVME::BLOCK_SIZE * NUM_BLOCKS);
        MemBarrier();

        /* push io descriptor to sub queue */
#define ASYNC_FILL
#ifdef ASYNC_FILL
        PLOG("** Async Fill");
        Notify *notify_fill = new Notify_Verify(io_desc_ptr->offset, io_desc_ptr->offset/NUM_BLOCKS, io_desc_ptr->buffer_virt, marker, NVME::BLOCK_SIZE * NUM_BLOCKS, false);

        status_t st = _itf->async_io_batch((io_request_t*)io_desc_ptr, 1, notify_fill, _qid);
        //status_t st = _itf->async_io((io_request_t)io_desc_ptr, notify_fill, _qid);
#else
        PLOG("** Sync Fill");
        /*sync write*/
        status_t st = _itf->sync_io((io_request_t)io_desc_ptr, _qid);
#endif
        //usleep(100);
        MemBarrier();
      }

      /* dump SQ/CQ info */
      ioq->dump_queue_info();

      printf("================= Waiting for IO completion ================\n");
      _itf->wait_io_completion(_qid); //wait for IO completion
      _itf->flush(1, _qid);
      sleep(2);

      ioq->dump_queue_info();
      printf("** All writes complete. Start to read (Q:%u).\n", _qid);
#endif // VERIFY_FILL

      /*=====================================================================*/

#ifdef TEST_RANDOM_IO
      boost::random::random_device rng;
      boost::random::uniform_int_distribution<> rnd_page_offset(1, N_PAGES-1); //max 781,422,768 sectors
#endif

      for(unsigned idx = 0; idx < N_PAGES; idx++) {

        io_descriptor_t* io_desc_ptr = io_desc + idx % SLAB_SIZE;
        memset(io_desc_ptr, 0, sizeof(io_descriptor_t));

        io_desc_ptr->action = NVME_READ;
        unsigned buffer_idx = (2*idx) % SLAB_SIZE;
        io_desc_ptr->buffer_virt = _virt_array_local[buffer_idx];
        io_desc_ptr->buffer_phys = _phys_array_local[buffer_idx];
        io_desc_ptr->offset = idx * NUM_BLOCKS * SCALE;
        io_desc_ptr->num_blocks = NUM_BLOCKS;

        memset(io_desc_ptr->buffer_virt, 0, PAGE_SIZE);
        //hexdump(io_desc_ptr->buffer_virt, 32);

        //uint8_t marker = (uint8_t)((io_desc_ptr->offset/NUM_BLOCKS) & 0xff);
        uint8_t marker = CONST_MARKER;
#define SYNC_READ
#ifndef SYNC_READ
        PLOG("** Random async read verification");
        Notify *notify_verify = new Notify_Verify(io_desc_ptr->offset, io_desc_ptr->offset/NUM_BLOCKS, io_desc_ptr->buffer_virt, marker, NVME::BLOCK_SIZE * NUM_BLOCKS, true);
        status_t st = _itf->async_io_batch((io_request_t*)io_desc_ptr, 1, notify_verify, _qid);
#else
        PLOG("** Sequential sync read verification");
        Notify *notify_verify = new Notify_Verify(io_desc_ptr->offset, io_desc_ptr->offset/NUM_BLOCKS, io_desc_ptr->buffer_virt, marker, NVME::BLOCK_SIZE * NUM_BLOCKS, true);
        status_t st = _itf->sync_io((io_request_t)io_desc_ptr, _qid);

        notify_verify->action();
        delete notify_verify;
#endif
      }
      _itf->wait_io_completion(_qid); //wait for IO completion

      printf("========= dump after read ========= \n");
      ioq->dump_queue_info();

      free(io_desc);
    }


    void read_data(off_t lba) 
    {
      printf("==================== READ SSD ==========================\n");
      using namespace Exokernel;
      assert(NUM_BLOCKS == 8);

      NVME_IO_queue * ioq = _dev->io_queue(_qid);

      io_descriptor_t* io_desc_ptr = (io_descriptor_t *)malloc(sizeof(io_descriptor_t));
      memset(io_desc_ptr, 0, sizeof(io_descriptor_t));

      io_desc_ptr->action = NVME_READ;
      io_desc_ptr->buffer_virt = _virt_array_local[1];
      io_desc_ptr->buffer_phys = _phys_array_local[1];
      io_desc_ptr->offset = lba;
      io_desc_ptr->num_blocks = NUM_BLOCKS;

      memset(io_desc_ptr->buffer_virt, 0, PAGE_SIZE);
      //hexdump(io_desc_ptr->buffer_virt, 32);

      Notify *notify_verify = new Notify_Verify(io_desc_ptr->offset, io_desc_ptr->offset/8, io_desc_ptr->buffer_virt, (io_desc_ptr->offset/8) & 0xff, NVME::BLOCK_SIZE * NUM_BLOCKS, true);
      //status_t st = _itf->async_io_batch((io_request_t*)io_desc_ptr, 1, notify_verify, _qid);
      status_t st = _itf->async_io_batch((io_request_t*)io_desc_ptr, 1, NULL, _qid);

      _itf->wait_io_completion(_qid); //wait for IO completion

      hexdump(io_desc_ptr->buffer_virt, 512*8);
      free(io_desc_ptr);
    }


  public:
    Verify_thread(IBlockDevice * itf, unsigned qid, core_id_t core, addr_t *phys_array_local, void **virt_array_local) : 
      _qid(qid), _itf(itf), Exokernel::Base_thread(NULL,core), _phys_array_local(phys_array_local), _virt_array_local(virt_array_local) {
        _dev = (NVME_device*)(itf->get_device());
      }

    void* entry(void* param) {
      assert(_dev);

      std::stringstream str;
      str << "rt-client-qt-" << _qid;
      Exokernel::set_thread_name(str.str().c_str());

      verify();
      
      //read_data(55*8);

      //for(unsigned i=0; i<256; i++)
      //{
      //  printf("======= page = %u =======\n", i);
      //  read_data(i*8);
      //}
    }

};

#define SHMEM_HUGEPAGE_ALLOC    0
#define DMA_HUGEPAGE_ALLOC      0
#define DMA_PAGE_ALLOC          1
#define DMA_PAGE_ALLOC_SINGLE   0

class verify_tests {

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

      for(unsigned i=0; i<NUM_QUEUES; i++) {
        addr_t phys_addr = 0;
        void *p = dev->alloc_dma_pages(SLAB_SIZE, &phys_addr);
        page_list.push_back(p);
        memset(p, 0, PAGE_SIZE*SLAB_SIZE);

        char *p_char = (char *)p;
        for(unsigned j=0; j<SLAB_SIZE; j++) {
          virt_array[i][j] = p_char;
          phys_array[i][j] = phys_addr;
          p_char += PAGE_SIZE;
          phys_addr += PAGE_SIZE;
          //printf("[%d, %d]: virt = %p, phys = %lx (%lx)\n", i, j, virt_array[i][j], phys_array[i][j], pm.virt_to_phys(virt_array[i][j]));
          //assert(phys_array[i][j] ==  pm.virt_to_phys(virt_array[i][j]));
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
#elif (DMA_PAGE_ALLOC_SINGLE == 1)
    void allocMem(IBlockDevice * itf) {
      using namespace Exokernel;
      Device * dev = itf->get_device();

#define N_SINGLE_PAGE 64
      assert(COUNT_VERIFY <= N_SINGLE_PAGE);
      for(unsigned i=0; i<NUM_QUEUES; i++) {
        for(unsigned j = 0; j<N_SINGLE_PAGE; j++) {
          addr_t phys_addr = 0;
          void *p = dev->alloc_dma_pages(1, &phys_addr);
          page_list.push_back(p);
          memset(p, 0, PAGE_SIZE);

          virt_array[i][j] = p;
          phys_array[i][j] = phys_addr;
          //printf("[%d, %d]: virt = %p, phys = %lx\n", i, j, virt_array[i][j], phys_array[i][j]);
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

      allocMem(itf);

      {
        Verify_thread * thr[NUM_QUEUES];

//#define N_QUEUE_MAP 8
//        unsigned sq_map[N_QUEUE_MAP] = {4, 8, 12, 16, 20, 24, 28, 32};

        Config config = ((NVME_device*)(itf->get_device()) )->config();
        assert(NUM_QUEUES <= config.num_io_queues());

        for(unsigned i = 1; i <= NUM_QUEUES; i++) {

          unsigned sq_core = config.get_sub_core_from_qid(i);
          printf("********** qid = %u, core = %u\n", i, sq_core);

          thr[i-1] = new Verify_thread(itf,
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
      }

      freeMem(itf);
    }
};

#undef MAX_LBA
#undef COUNT_VERIFY
#undef NUM_QUEUES 
#undef SLAB_SIZE
#undef NUM_BLOCKS
#undef SHMEM_HUGEPAGE_ALLOC
#undef DMA_HUGEPAGE_ALLOC
#undef DMA_PAGE_ALLOC
#undef DMA_PAGE_ALLOC_SINGLE
#undef TEST_RANDOM_IO
#undef SCALE
#undef VERIFY_FILL
#undef ASYNC_FILL

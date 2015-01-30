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
#include <common/cycles.h>
#include <exo/rand.h>

#include "nvme_device.h"
#include "tests.h"

#define COUNT (1000000)
#define NUM_QUEUES (1)
#define SLAB_SIZE (1000)
#define NUM_BLOCKS (8)

Exokernel::Pagemap pm;

void basic_block_read(NVME_device * dev, off_t lba);
void basic_block_write(NVME_device * dev, off_t lba);
void flush_test(NVME_device * dev);


class Read_thread : public Exokernel::Base_thread
{
private:
  NVME_device * _dev;
  unsigned _qid;
  Notify_object nobj;


  void read_throughput_test()
  {
    using namespace Exokernel;

    byte r;

#if 0
    /* set higher priority */
    {
      struct sched_param params;
      params.sched_priority = sched_get_priority_max(SCHED_FIFO);   
      pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);
    }
#endif


    /* set up a simple slab */
    addr_t phys_array[SLAB_SIZE];
    void * virt_array[SLAB_SIZE];

    /* allocate slab of memory */
    Memory::huge_system_configure_nrpages(1000);
    int h;
    virt_array[0] = Memory::huge_shmem_alloc(1,PAGE_SIZE * SLAB_SIZE,0,&h);
    assert(virt_array[0]);

    char * p = (char *) virt_array[0];
    for(unsigned i=0;i<SLAB_SIZE;i++) {
      virt_array[i] = p;
      phys_array[i] = pm.virt_to_phys(virt_array[i]);
      p+=PAGE_SIZE;
      assert(virt_array[i]);
      assert(phys_array[i]);
    }
    touch_pages(virt_array[0],PAGE_SIZE*SLAB_SIZE); /* eager map */
    PLOG("Memory allocated OK!!");


    cpu_time_t start_ts = rdtsc();
    NVME_IO_queue * ioq = _dev->io_queue(_qid);
    PLOG("** READ THROUGHPUT AROUND Q:%p (_qid=%u)\n",(void *) ioq, _qid);

    unsigned long long mean_cycles = 0;
    unsigned long long sample_count = 0;

    _dev->io_queue(_qid)->callback_manager()->register_callback(&Notify_object::notify_callback,
                                                                (void*)&nobj);

    unsigned long threshold = _dev->io_queue(_qid)->queue_length() * 0.8;
    PLOG("threshold = %lu", threshold);
    atomic_t send_id = 0;
    uint16_t cid;
    for(unsigned long i=0;i<COUNT;i++) {
      
      cpu_time_t start = rdtsc();
      cid = _dev->block_async_read(_qid,
                                   phys_array[i%SLAB_SIZE],                             
                                   (i*512)+(_qid*COUNT), /* offset */
                                   NUM_BLOCKS); /* num blocks */
      
      PLOG("sent (Q:%u) %lu...",_qid,send_id);
      /* collect stats */
      cpu_time_t delta = rdtsc() - start;

      if(sample_count > 0) {
        mean_cycles = ((mean_cycles * sample_count)+delta)/(sample_count+1);
        sample_count++;
      }
      else {
        mean_cycles = delta;
      }

      if(i%100000==0) PLOG("(_QID:%u) i=%lu", _qid, i);     
      if(i==COUNT) break;

      /* wait for bursts of IOPs - don't do this too late */
      if((i % threshold == 0) && (i > 0)) {
        atomic_t c;
        
        nobj.set_when(send_id); /* set wake up at send_id */
        nobj.wait();
      } 


      send_id++;
    }
    PLOG("all sends complete.");

    // /* wait for last command */
    // uint16_t c;
    // while((c = nobj.wait()) != cid) {
    // }
    // PLOG("end c = %x",c);
    
    
    ioq->dump_stats();

    PLOG("(QID:%u) cycles mean/iteration %llu (%llu IOPS)\n",_qid, mean_cycles, (((unsigned long long)get_tsc_frequency_in_mhz())*1000000)/mean_cycles);
    cpu_time_t end_ts = rdtsc();


    //    Memory::huge_shmem_free(virt_array[0],h);
  }


public:
  Read_thread(NVME_device * dev, unsigned qid, core_id_t core) : 
    _qid(qid), _dev(dev), Exokernel::Base_thread(NULL,core) {
  }

  void* entry(void* param) {
    assert(_dev);

    std::stringstream str;
    str << "rt-client-qt-" << _qid;
    Exokernel::set_thread_name(str.str().c_str());

    read_throughput_test();
  }


};



int main(int argc, char * argv[])
{
  NVME_device* dev;
  Exokernel::set_thread_name("nvme_drv-main");

  PLOG("PID=%u",getpid());
  PLOG("System Clock HZ = %lu",(unsigned long)get_tsc_frequency_in_mhz() * 1000000UL);
  
  try {
    dev = new NVME_device("config.xml");
  }
  catch(Exokernel::Exception e) {
    NVME_INFO("EXCEPTION: error in NVME device initialization (%s) \n",e.cause());
    asm("int3");
  }
  catch(Exokernel::Fatal e) {
    NVME_INFO("FATAL: error in NVME device initialization (%s) \n",e.cause());
    asm("int3");
  }
  catch(...) {
    NVME_INFO("EXCEPTION: error in NVME device initialization (unknown exception) \n");
    asm("int3");
  }


  if(argc > 1) {
    NVME_INFO("****** Round1 : STARTING SINGLE BLOCK R/W TESTING ********\n");
    flush_test(dev);
    basic_block_write(dev,2);
    basic_block_read(dev,2);
    NVME_INFO("****** Round1 : TESTING COMPLETE  ********\n");

    NVME_INFO("****** Round2 : STARTING RANDOM BLOCK R/W TESTING ********\n");
    TestBlockWriter tbw(dev,2); /* test using queue 2 */
    unsigned NUM_TEST_WRITES = 20;
    for(unsigned i=0;i<NUM_TEST_WRITES;i++) {
      uint8_t value = (uint8_t) i;
      off_t lba = genrand64_int64() % 1024;
      tbw.write_and_verify(lba,value);
    }
    NVME_INFO("****** Round2 : TESTING COMPLETE  ********\n");
    
  }


#if 0
  NVME_INFO("Issuing test read and write test...");
  
  {

    Read_thread * thr[NUM_QUEUES];

    for(unsigned i=1;i<=NUM_QUEUES;i++) {
      thr[i-1] = new Read_thread(dev,
                                 i, //(2*(i-1))+1, /* qid */
                                 i+3); //*2-1); //1,3,5,7 /* core for read thread */

    }

    struct rusage ru_start, ru_end;
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
    getrusage(RUSAGE_SELF,&ru_start);

    for(unsigned i=1;i<=NUM_QUEUES;i++)
      thr[i-1]->start();

    for(unsigned i=1;i<=NUM_QUEUES;i++) {
      PLOG("!!!!!!!!!!!!!! joined read thread %d",thr[i-1]);
      thr[i-1]->join();
    }

    PLOG("All read threads joined.");

    getrusage(RUSAGE_SELF,&ru_end);
    gettimeofday(&tv_end, NULL);

    double delta = ((ru_end.ru_utime.tv_sec - ru_start.ru_utime.tv_sec) * 1000000.0) +
      ((ru_end.ru_utime.tv_usec - ru_start.ru_utime.tv_usec));

    /* add kernel time */
    delta += ((ru_end.ru_stime.tv_sec - ru_start.ru_stime.tv_sec) * 1000000.0) +
      ((ru_end.ru_stime.tv_usec - ru_start.ru_stime.tv_usec));

    float usec_seconds_for_count = delta/NUM_QUEUES;    
    PLOG("Time for 1M 4K reads is: %.5g sec",usec_seconds_for_count / 1000000.0);
    float usec_each = usec_seconds_for_count / ((float)COUNT);
    PLOG("Rate: %.5f IOPS/sec", (1000000.0 / usec_each)/1000.0);

    PLOG("Voluntary ctx switches = %ld",ru_end.ru_nvcsw - ru_start.ru_nvcsw);
    PLOG("Involuntary ctx switches = %ld",ru_end.ru_nivcsw - ru_start.ru_nivcsw);
    PLOG("Page-faults = %ld",ru_end.ru_majflt - ru_start.ru_majflt);

    double gtod_delta = ((tv_end.tv_sec - tv_start.tv_sec) * 1000000.0) +
      ((tv_end.tv_usec - tv_start.tv_usec));
    double gtod_secs = (gtod_delta/NUM_QUEUES) / 1000000.0;
    PLOG("gettimeofday delta %.2g sec (rate=%.2f KIOPS)",
         gtod_secs,
         1000.0 / gtod_secs);

  }
#endif
  
  NVME_INFO("done tests.\n");
  sleep(100);
  dev->destroy();
  delete dev;

  return 0;
}

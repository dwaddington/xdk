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
#include "mt_tests.cc"


void basic_block_read(NVME_device * dev, off_t lba);
void basic_block_write(NVME_device * dev, off_t lba);
void flush_test(NVME_device * dev);


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

#if 0
    NVME_INFO("****** Round3 : TESTING MULTI-THREADED ********\n");
    mt_tests::runTest(dev);
    NVME_INFO("****** Round3 : TESTING COMPLETE - MULTI-THREADED ********\n");
#endif
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

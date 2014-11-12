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







/*
  Author(s):
  @author Daniel Waddington (d.waddington@samsung.com)
  @date: Oct 1, 2012
*/

#include <printf.h>
#include <rand.h>

#include "ext2fs.h"
#include "ext2fs_file.h"
#include "measure.h"

using namespace Ext2fs;
using namespace OmniOS;

typedef uint64_t timestamp_t;


#define CPU_FREQ_MHZ 3392ULL // 3.40 GHz
#define CYCLES_PER_SECOND (CPU_FREQ_MHZ * 1000ULL * 1000ULL)
#define CYCLES_PER_MS 3392000ULL

//#define TOTAL_READS 1000000ULL
#define TOTAL_READS 1000ULL

void test_read_random_blocks(File * fp, uint64_t max_bytes)
{
  printf("File is %llu MBytes\n",max_bytes / (1024ULL*1024ULL));
  printf("TEST READ RANDOM BLOCKS OK.\n");
  printf("TOTAL_READS = %llu\n",TOTAL_READS);
  uint64_t max_block_num = max_bytes / 512;
  printf("Max block    %llu\n",max_block_num);
  char * tmp = (char *) malloc(512);

  /* START TIMESTAMP */
  timestamp_t st = rdtsc();

  /* MEASURED TEST AREA */
  for(unsigned long i=0;i<TOTAL_READS;i++) {
    uint64_t next_block = genrand64_int64() % max_block_num;
    fp->read(next_block * 512, 512, tmp);
  }
  
  /* END TIMESTAMP */
  timestamp_t end = rdtsc();
  printf("Random tests: (%u ms)\n",(unsigned)((double)(end-st)/(double)CYCLES_PER_MS));
  double seconds = (end-st)/((double)CYCLES_PER_SECOND);
  //  printf("seconds = %f\n",seconds);
  double kilo_bytes = (double)(TOTAL_READS * 512) / (double)(1024);
  printf("Read: %u KB\n",(unsigned) kilo_bytes);
  printf("Throughput: %u KB/s\n",((unsigned) (kilo_bytes/seconds)));
  assert(end > st);
  free(tmp);

}


#if 0
  printf("sizeof(uint64_t) = %d\n",sizeof(uint64_t));
  printf("sizeof(unsigned long) = %d\n",sizeof(unsigned long));
  printf("Opening file [%s]\n",filename);
  FILE * fp = fopen(filename,"r");
  assert(fp != NULL);

  fseeko(fp,0L, SEEK_END);
  uint64_t max_bytes = ftello(fp);
  printf("File is %llu MBytes\n",max_bytes / (1024ULL*1024ULL));
    
  uint64_t max_block_num = max_bytes / 512;
  printf("Max block    %llu\n",max_block_num);
  char * tmp = (char *) malloc(512);

  timestamp_t st = rdtsc();

  /* MEASURED TEST AREA */
  for(unsigned long i=0;i<TOTAL_READS;i++) {
    uint64_t next_block = genrand64_int64() % max_block_num;
    //    printf("next_block = %llu\n",next_block);
    int r = fseeko(fp,next_block*512, SEEK_SET);
    assert(r==0);
    size_t s = fread(tmp,1,512,fp);
    assert(s==512);
  }
 
  /* END */
  timestamp_t end = rdtsc();
  printf("Random tests: (%g ms)\n", (double)(end-st)/(double)CYCLES_PER_MS);
  double seconds = (end-st)/((double)CYCLES_PER_SECOND);
  //  printf("seconds = %f\n",seconds);
  double kilo_bytes = (double)(TOTAL_READS * 512) / (double)(1024);
  printf("Read: %g MB\n",kilo_bytes);
  printf("Throughput: %g MB/s\n",kilo_bytes/seconds);
  assert(end > st);
  free(tmp);
  fclose(fp)
#endif

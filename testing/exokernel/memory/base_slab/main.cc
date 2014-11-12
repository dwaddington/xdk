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
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <assert.h>
#include <stdlib.h>
#include <list>

#include "libexo.h"
#include <private/__base_slab_allocator.h>
#include <exo/pagemap.h>
#include <exo/acpi.h>


void test_acpi()
{
  Exokernel::ACPI acpi;

  Exokernel::SRAT_table * srat = acpi.open_table_SRAT();
  srat->dump();

  delete srat;
}

void test_numa_allocator()
{
  using namespace Exokernel::Memory;
  using namespace Exokernel;
  Pagemap pm;

  int num_nodes = numa_num_configured_nodes();
  for(unsigned i=0;i<num_nodes;i++) {

    void * p = numa_alloc_onnode(4096,i);
    assert(p);
    memset(p,1,4096);
    printf("Node %d: virtual=%p phys=0x%lx\n",
           i, p, pm.virt_to_phys(p));
  }
  //  NUMA_basic_allocator alloc(0);
}

void test_spans()
{
  std::list<void *> allocation_list;

  using namespace Exokernel::Memory;

  NUMA_basic_allocator alloc(0); //numa_node_of_cpu(0));
  Slab_metadata_allocator<PAGE_SIZE*4, 4> test_span(sizeof(int),&alloc);

  void * p;
  bool init = false;
  unsigned num_allocations = 0;
  unsigned iteration = 0;
  for(;iteration < 100000; iteration++) {

    for(unsigned i=0;i<random() % 20;i++) {
      p=test_span.span_next_free(init);
      if(!p) {
        break;
      }
    
      *((int*)p) = (int) i;
      allocation_list.push_back(p);
      //    printf("allocated at %p\n",p);
      num_allocations++;
    }
    if(!p) break;

    {
      unsigned to_free = random() % 10;
      if(to_free > allocation_list.size()) to_free=allocation_list.size();
      
      while(to_free) {
        void * p = allocation_list.front();
        allocation_list.pop_front();
        if(random() % 2 == 0) {
          allocation_list.push_back(p);
          continue;
        }
        assert(test_span.span_free(p)==Exokernel::S_OK);
        to_free --;
        num_allocations--;
      }
    }
  }
  printf("%u iterations complete.\n",iteration);

  while(!allocation_list.empty()) {
    void * p = allocation_list.front();
    allocation_list.pop_front();
    assert(test_span.span_free(p) == Exokernel::S_OK);
  }
  printf("OK. span test complete.\n");
}


int main()
{
  Exokernel::Memory::dump_numa_info();

  test_acpi();
  //  test_spans();
  test_numa_allocator();


  return 0;
}

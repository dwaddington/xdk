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
#include "libexo.h"
#include <fstream>
#include <exo/memory.h>


int main()
{
  PLOG("starting test.");

  using namespace Exokernel;

  const size_t BLOCK_SIZE = 2048;
  const size_t NUM_BLOCKS = 1001;
  const size_t actual_size = round_up_page(Memory::Fast_slab_allocator::actual_block_size(BLOCK_SIZE) * NUM_BLOCKS);
  
  /* allocate some memory */
  addr_t space_p = 0;
  void * space_v = Exokernel::Memory::alloc_page(&space_p);
  assert(space_v);

  PLOG("allocated slab space (virt=%p)(phys=%p)",space_v, (void*) space_p);

  Memory::Fast_slab_allocator * fsa = 
    new Memory::Fast_slab_allocator(space_v, space_p,
                                    actual_size,
                                    BLOCK_SIZE,
                                    0, // alignment
                                    0, // id
                                    99); // debug id

  PLOG("Fast_slab_allocator ctor OK.");

  void * allocations[1000];
  assert(fsa);
  {
    PLOG("beginning allocations ...");

    /* test fast slab allocator out */

    for(unsigned i=0;i<NUM_BLOCKS;i++) {
      void * p = fsa->alloc();
      assert(p);
      allocations[i] = p;
      memset(p,i % 255, BLOCK_SIZE);
      if(i%100==0 && i > 0) 
        PLOG("allocated %u blocks", i);
    }    

    for(unsigned i=0;i<NUM_BLOCKS;i++) {
      unsigned char * p = (unsigned char *) allocations[i];
      for(unsigned j=0;j<BLOCK_SIZE;j++) {
        if(p[j]!=i%255) {
          PLOG("data error p[%u]=%d != %d",j,p[j],i%255);
        }
      }
      if(i%100==0 && i > 0) 
        PLOG("checked %u blocks", i);
    }    

  }  
              
  Exokernel::Memory::free_page(space_v);

  PLOG("Slab allocator test OK!\n");

  return 0;
}

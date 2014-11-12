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
#include <fstream>
#include <assert.h>

#include "libexo.h"

#define TEST_SIZE HUGE_PAGE_SIZE


Exokernel::Pagemap pm;
void *p, *q;
addr_t q_p;

bool gexit = false;

int main(int argc, char * argv[])
{
  if(argc == 1) {
    Exokernel::Memory::huge_system_configure_nrpages(128);
    PLOG("configure 128 huge pages");

    q = Exokernel::Memory::huge_malloc(TEST_SIZE,&q_p);
    assert(q);
    printf("Huge malloc v=%p p=%p\n",q,(void*) q_p);  
  
    memset(q,0xE,TEST_SIZE);
    Exokernel::hexdump(q,20);

    printf("Press a key to exit.\n");
    getchar();
    
    Exokernel::Memory::huge_free(q);
  }
  else {
    addr_t phys;
    sscanf(argv[1],"%lx",&phys);
    printf("Got phys:%p\n",(void*) phys);
    void * mq = Exokernel::Memory::huge_mmap(TEST_SIZE,phys);
    printf("mq=%p\n",(void*) mq);
    printf("got mq=%p (phys=0x%lx)\n",mq,pm.virt_to_phys(mq));

    Exokernel::hexdump(mq,20);
    memset(mq,0xD,TEST_SIZE);
    Exokernel::hexdump(mq,20);
  }
  
  return 0;
}

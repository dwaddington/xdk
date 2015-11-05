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

Exokernel::Pagemap pm;

void do_huge_alloc()
{
  using namespace Exokernel::Memory;

  addr_t phys;
  void * p = huge_malloc(MB(2)*4);
  printf("huge_malloc returned:%p\n",p);
  strcpy((char*)p,"hello");

  printf("virt=%p phys=0x%lx\n",p,phys);
  pm.dump_page_flags(pm.page_flags(p));
  huge_free(p);
}

void do_page_alloc()
{
  using namespace Exokernel::Memory;
  addr_t phys = 0;
  void * p = alloc_page(&phys);

  PLOG("Allocated 8 pages vaddr=%p paddr=%p",p,(void*)phys);
  memset(p,0,PAGE_SIZE * 8);
  
  //  pm.dump_page_flags(pm.page_flags(p));

  free_page(p);
  PLOG("Free pages OK.");
}

void do_alloc_dma_memory()
{
  std::ofstream ofs;
  ofs.open("/sys/class/parasite/pk0/dma_page_alloc");
  
  ofs << "4 0\n";
}

int main(int argc, char * argv[])
{
  PLOG("test client: pid(%x)", getpid());

  PLOG("configuring system # huge pages");
  Exokernel::Memory::huge_system_configure_nrpages(10);

  do_huge_alloc();
  do_page_alloc();
  do_alloc_dma_memory();

  return 0;
}

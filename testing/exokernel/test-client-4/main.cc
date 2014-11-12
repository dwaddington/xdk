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
#include <errno.h>

void * alloc_dma_pages(size_t num_pages, addr_t * phys_addr, unsigned numa_node = -1) 
{
  /* allocate physical memory */
  {
    try {
      std::fstream fs;
      std::string n = "/sys/class/parasite/pk0";
          
      n += "/dma_page_alloc";
        
      std::stringstream sstr;
      fs.open(n.c_str());          
      sstr << num_pages << " " << numa_node << std::endl;
      fs << sstr.str();

      /* reset file pointer and read allocation results */
      fs.seekg(0);
      int owner, numa, order;
      addr_t paddr;

      if(!(fs >> std::hex >> owner >> std::ws >> numa >> std::ws >> order >> std::ws >> std::hex >> paddr)) {
        if(errno == 34) 
          PDBG("DMA allocation order too high for kernel.");
        else
          PDBG("unknown error in dma_page_alloc (%d)",errno);
      }


      assert(paddr > 0);
      *phys_addr = paddr;
      printf("phys=%lx\n",paddr);

      /* do mmap into virtual address space */
      void * p;
      {
        int fd;
        fd = open("/dev/parasite",O_RDWR);
        if(!fd)
          panic("could not open /dev/parasite");
          
        PLOG("performing mmap..");
        p = mmap(NULL,
                 num_pages*PAGE_SIZE, 
                 PROT_READ|PROT_WRITE,// prot
                 MAP_SHARED, // flags
                 fd,
                 paddr);
          
        PLOG("mmap complete.");
        close(fd);
        //p = Exokernel::map_phys_memory(paddr, num_pages * PAGE_SIZE);
      }

      assert(p);
      return p;
    }
    catch(...) {
      throw Exokernel::Fatal(__FILE__,__LINE__,"unexpected condition in dma_page_alloc");
    }
  }
}

int main()
{
  addr_t phys;
  /* allocate physical DMA memory */
  byte * virt = (byte*) alloc_dma_pages(2,(addr_t*)&phys);
  
  /* test memory */
  memset(virt,0,PAGE_SIZE * 2);
  
  PLOG("DMA page test OK!");

  return 0;
}

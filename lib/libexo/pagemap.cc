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
  Authors:
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#include <iostream>
#include "exo/pagemap.h"
#include "exo/memory.h"
/** 
 * Read in the current memory regions from /proc/self/maps.
 * 
 * @param range_list [out] vector of range_t elements
 */
void Exokernel::Pagemap::read_regions(std::vector<range_t> &range_list)
{
  std::ifstream ifs;
  ifs.open("/proc/self/maps");

  for (std::string line; std::getline(ifs, line); ) {

    std::stringstream istr(line);
    addr_t from, to;
    char dummy;
    istr >> std::hex >> from;
    istr >> dummy;
    istr >> std::hex >> to;

    range_list.push_back(range_t(from,to));
  }

  ifs.close();
}

void Exokernel::Pagemap::dump_self_region_info()
{
  std::ifstream ifs;
  ifs.open("/proc/self/maps");

  std::cout << std::endl;
  for (std::string line; std::getline(ifs, line); ) {    
    std::cout << line << std::endl;
  }

  ifs.close();
}

/** 
 * Look up the physical Page Frame Number from the virtual address
 * 
 * @param vaddr Virtual address 
 * 
 * @return Page Frame Number if present
 */
uint64_t Exokernel::Pagemap::__get_page_frame_number(void * vaddr) {

  using namespace std;

  size_t pages_in_range;

  /* check vaddr exists */
  vector<range_t> range_list;
  read_regions(range_list);
  for (unsigned i=0;i<range_list.size();i++) {

    pages_in_range = (range_list[i].end - range_list[i].start) / _page_size;

    /* check if this is our range */
    if ((((addr_t)vaddr) >= range_list[i].start) 
        && (((addr_t)vaddr) < range_list[i].end)) {
      PLOG("found %lx in range %lx-%lx",vaddr,range_list[i].start,range_list[i].end);
      goto found;
    }
  }

  PERR("virt_to_phys: invalid virtual address.");
  return 0;
      
 found:

  /* now open up the pagemap entry */
  off_t offset = 0;
  uint64_t entry = 0;

  /* each entry is a page; an entry is 64 bits. */
  offset = ((addr_t)vaddr >> PAGE_SHIFT) * sizeof(uint64_t);

  int rc = pread(_fd_pagemap,(void*)&entry,8,offset);
  assert(rc == 8);

  if(entry & (1UL << 55)) { PLOG("page soft-dirty"); }
  if(entry & (1UL << 61)) { PLOG("page is shared-anon or file-page"); }
  if(entry & (1UL << 62)) { PLOG("page is swapped"); }
  if(entry & (1UL << 63)) { PLOG("page is present"); }

  /* Page Frame Number is bit 0-54 */
  return (entry & 0x7fffffffffffffULL);
}

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
  Copyright (C) 2013, Daniel G. Waddington <d.waddington@samsung.com>
*/

#ifndef __EXO_PAGEMAP_H__
#define __EXO_PAGEMAP_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <assert.h>
#include <unistd.h>
#include <iostream>

#include <common/types.h>
#include <common/logging.h>
#include <common/utils.h>

namespace Exokernel
{
  class Pagemap
  {
  private:
    int _fd_pagemap;
    int _fd_kpageflags;
    int _page_size;

    enum page_flag_t {
      PAGE_FLAG_LOCKED=(1ULL << 0),
      PAGE_FLAG_ERROR=(1ULL << 1),
      PAGE_FLAG_REFERENCED=(1ULL << 2),
      PAGE_FLAG_UPTODATE=(1ULL << 3),
      PAGE_FLAG_DIRTY=(1ULL << 4),
      PAGE_FLAG_LRU=(1ULL << 5),
      PAGE_FLAG_ACTIVE=(1ULL << 6),
      PAGE_FLAG_SLAB=(1ULL << 7),
      PAGE_FLAG_WRITEBACK=(1ULL << 8),
      PAGE_FLAG_RECLAIM=(1ULL << 9),
      PAGE_FLAG_BUDDY=(1ULL << 10),
      /* kernel > 2.6.31 */
      PAGE_FLAG_MMAP=(1ULL << 11),
      PAGE_FLAG_ANON=(1ULL << 12),
      PAGE_FLAG_SWAPCACHE=(1ULL << 13),
      PAGE_FLAG_SWAPBACKED=(1ULL << 14),
      PAGE_FLAG_COMPOUND_HEAD=(1ULL << 15),
      PAGE_FLAG_COMPOUND_TAIL=(1ULL << 16),
      PAGE_FLAG_HUGE=(1ULL << 17),
      PAGE_FLAG_UNEVICTABLE=(1ULL << 18),
      PAGE_FLAG_HWPOISON=(1ULL << 19),
      PAGE_FLAG_NOPAGE=(1ULL << 20),
      PAGE_FLAG_KSM=(1ULL << 21),
      PAGE_FLAG_THP=(1ULL << 22),
    };
    
    struct range_t {
      addr_t start;
      addr_t end;

      range_t(addr_t s, addr_t e) : start(s), end(e) {}
    };

    /** 
     * Read in the current memory regions from /proc/self/maps.
     * 
     * @param range_list [out] vector of range_t elements
     */
    static void read_regions(std::vector<range_t> &range_list);


    /** 
     * Look up the physical Page Frame Number from the virtual address
     * 
     * @param vaddr Virtual address 
     * 
     * @return Page Frame Number if present
     */
    uint64_t __get_page_frame_number(void * vaddr);



  public:

    /** 
     * Constructor; use calling process id
     * 
     */
    Pagemap() {
      _fd_pagemap = open("/proc/self/pagemap",O_RDONLY);
      assert(_fd_pagemap != -1);

      _fd_kpageflags = open("/proc/kpageflags",O_RDONLY);
      assert(_fd_kpageflags);

      _page_size = getpagesize();
    }

    ~Pagemap() {
      close(_fd_pagemap);
      close(_fd_kpageflags);
    }


    /** 
     * Translate virtual to physical address for the current process.
     * 
     * 
     * @return 
     */
    addr_t virt_to_phys(void * vaddr) {
      
      uint64_t pfn = __get_page_frame_number(vaddr);
      return (pfn << PAGE_SHIFT) + (((addr_t) vaddr) % _page_size);
    }

    /** 
     * Return the page flags for a given physical page
     * 
     * @param vaddr Virtual address mapped to desired physical page
     * 
     * @return 64-bit page flags
     */
    page_flag_t page_flags(void * vaddr) {

      uint64_t pfn = __get_page_frame_number(vaddr);
      uint64_t entry = 0;

      int rc = pread(_fd_kpageflags,(void*)&entry,8,pfn*8);
      if(rc < 0) {
        perror("page_flags pread failed: did you run as root?");
        return (page_flag_t) 0;
      }

      return (page_flag_t) entry;
    }

    /** 
     * Return the size of the file
     * 
     * 
     * @return Size of the file in bytes
     */
    size_t file_size() {
      struct stat buf;
      fstat(_fd_pagemap, &buf);
      return buf.st_size;
    }

    /** 
     * Dump page map information for the calling process
     * 
     */
    void dump_self_region_info();

    /** 
     * Dump some debugging information from /proc/self/maps
     * 
     */
    void dump() {

      using namespace std;
      size_t s = file_size();
      printf("Pagemap size:%ld bytes\n",s);

      /* retrieve the list of ranges from /proc/self/maps */
      vector<range_t> range_list;
      read_regions(range_list);

      for(vector<range_t>::iterator i = range_list.begin(); i != range_list.end(); i++) {
        struct range_t r = *i;
        while (r.start != r.end) {
          printf("PAGE: 0x%lx\n",r.start);
          r.start += 0x1000; // 4K          
        }
      }
    }

    /** 
     * Dump out page flag debugging info
     * 
     * @param f 64-bit flag
     */
    void dump_page_flags(page_flag_t f) {
      if(f & PAGE_FLAG_LOCKED) PLOG("pageflag:locked");
      if(f & PAGE_FLAG_ERROR) PLOG("pageflag:error");
      if(f & PAGE_FLAG_REFERENCED) PLOG("pageflag:referenced");
      if(f & PAGE_FLAG_UPTODATE) PLOG("pageflag:uptodate");
      if(f & PAGE_FLAG_DIRTY) PLOG("pageflag:dirty");
      if(f & PAGE_FLAG_LRU) PLOG("pageflag:lru");
      if(f & PAGE_FLAG_ACTIVE) PLOG("pageflag:active");
      if(f & PAGE_FLAG_SLAB) PLOG("pageflag:slab");
      if(f & PAGE_FLAG_WRITEBACK) PLOG("pageflag:writeback");
      if(f & PAGE_FLAG_RECLAIM) PLOG("pageflag:reclaim");
      if(f & PAGE_FLAG_BUDDY) PLOG("pageflag:buddy");
      if(f & PAGE_FLAG_MMAP) PLOG("pageflag:mmap");
      if(f & PAGE_FLAG_ANON) PLOG("pageflag:anon");
      if(f & PAGE_FLAG_SWAPCACHE) PLOG("pageflag:swapcache");
      if(f & PAGE_FLAG_SWAPBACKED) PLOG("pageflag:swapbacked");
      if(f & PAGE_FLAG_COMPOUND_HEAD) PLOG("pageflag:compound head");
      if(f & PAGE_FLAG_COMPOUND_TAIL) PLOG("pageflag:compound tail");
      if(f & PAGE_FLAG_HUGE) PLOG("pageflag:huge");
      if(f & PAGE_FLAG_UNEVICTABLE) PLOG("pageflag:unevictable");
      if(f & PAGE_FLAG_HWPOISON) PLOG("pageflag:hwpoison");
      if(f & PAGE_FLAG_NOPAGE) PLOG("pageflag:nopage");
      if(f & PAGE_FLAG_KSM) PLOG("pageflag:ksm");
      if(f & PAGE_FLAG_THP) PLOG("pageflag:thp");
    }
  };
}

#endif // __EXO_PAGEMAP_H__

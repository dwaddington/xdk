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
#include <assert.h>
#include <unistd.h>
#include <fstream>

#include "libexo.h"

#define ITERATIONS 10000000

Exokernel::Lockfree::Queue<unsigned long> * GQ;
bool go_flag = false;

class Pusher : public Exokernel::Base_thread
{
public:
  void* entry(void* param)
  {
    while(!go_flag);

    /* quiver */
    { unsigned cycles = ((unsigned)Exokernel::rdtsc()) & 0xFF; while(cycles-- > 0) cpu_relax(); }

    for(unsigned i=1;i<ITERATIONS;i++) {
      GQ->push(i);
    }
  }
};

class Popper : public Exokernel::Base_thread
{
public:
  void* entry(void* param)
  {
    while(!go_flag);
    
    for(unsigned i=1;i<ITERATIONS;i++) {
    retry:
      unsigned v = GQ->pop();
      if(v==0) {
        cpu_relax();
        goto retry;
      }

      /* quiver */
      { unsigned cycles = ((unsigned)Exokernel::rdtsc()) & 0xFF; while(cycles-- > 0) cpu_relax(); }
        
      assert(v==i);
      if(v % 1000 == 0)
        PLOG("popped %u",v);
    }

  }
};


void test_lockfree_queue()
{
  GQ = new Exokernel::Lockfree::Queue<unsigned long>();
  Pusher pusher;
  Popper popper;
  go_flag = true;
  pusher.join();
  popper.join();
}

void test_meta_data_allocator()
{
  Exokernel::Memory::Basic_metadata_allocator mda;
  
  void * p[100];
  for(unsigned i=0;i<100;i++) {
    p[i] = mda.alloc(10 * i, 0);
    memset(p[i],i,10*i);    
  }
}

int main()
{
  PLOG("starting test.");
  //test_lockfree_queue();
  test_meta_data_allocator();
  PLOG("done OK.");
  return 0;
}

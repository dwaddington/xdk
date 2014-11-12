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
#include <assert.h>

Exokernel::Pagemap pm;
void *p, *q;
addr_t q_p;

void test_malloc()
{
  p = malloc(4096 * 2);
  assert(p);
  printf("Malloc result p = %p\n",p);
}

void test_huge_malloc()
{  
  q = Exokernel::Memory::huge_malloc(32,&q_p);
  assert(q);
  printf("Huge malloc v=%p p=%p\n",q,(void*) q_p);  
}


int main()
{
  test_malloc();
  test_huge_malloc();

  pm.dump_self_region_info();

  printf("malloc'ed memory:\n");
  pm.dump_page_flags(pm.page_flags(p));

  printf("huge malloc'ed memory:\n");
  pm.dump_page_flags(pm.page_flags(q));
  
  Exokernel::Memory::huge_free(q);
  
  return 0;
}

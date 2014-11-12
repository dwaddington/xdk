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
	  @author Daniel G. Waddington ($Author: dwaddington $)
*/

#ifndef __RDTSC_H__
#define __RDTSC_H__

#if defined(__i386__)

inline uint64_t rdtsc() {
  uint32_t lo, hi;
  /* We cannot use "=A", since this would use %rax on x86_64 */
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return (uint64_t)hi << 32 | lo;
}

#elif defined(__x86_64__)


static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#elif defined(__powerpc__)


static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

#endif

// #if defined(__x86_64__) || defined(__i386__)

// /** 
//  * Returns the frequency of the timestamp counter in Hz.
//  * 
//  * 
//  * @return uint64_t frequency in ticks/sec
//  */
// uint64_t get_tsc_frequency()
// {
//   const int runs = 100;
  
//   uint64_t begin = 0LL;
//   uint64_t end = 0LL;
//   double cycles = 0LL;
//   uint64_t total_cycles = 0LL;
//   int i;

//   for(i = 0; i < runs; i++)
//     {
//       __asm__ volatile ("rdtsc" : "=A" (begin));
                
//       usleep(1000000); 
                
//       __asm__ volatile ("rdtsc" : "=A" (end));
                
//       cycles = end - begin;
//       total_cycles += cycles;
//     }
        
//   //average over runs
//   cycles = (double) total_cycles / (double) runs;
        
//   return (uint64_t) (cycles / (double) 1000000);
// }

// #endif

#endif // __RDTSC_H__

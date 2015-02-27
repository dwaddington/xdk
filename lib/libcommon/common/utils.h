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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>

#include "cpu_bitset.h"
#include "types.h"

#include <numa.h>

#define INLINE inline __attribute__((always_inline))

#ifndef SUPPRESS_NOT_USED_WARN
#define SUPPRESS_NOT_USED_WARN __attribute__((unused))
#endif

#define PAGE_SIZE (4096UL)
#define HUGE_PAGE_SIZE (2 * 1024 * 1024UL)
#define HUGE_MAGIC 0x0fabf00dUL


#if defined(__cplusplus)
extern "C" 
#endif
void panic(const char *format, ...) __attribute__((format(printf, 1, 2)));

#ifndef assert
#ifdef CONFIG_BUILD_DEBUG
#define assert(X) if(!(X)) panic("assertion failed at %s:%d", __FILE__,__LINE__);
#else
#define assert(X)
#endif
#endif


#if defined(__x86_64__)
#define mb()  asm volatile("mfence":::"memory")
#define rmb() asm volatile("lfence":::"memory")
#define wmb() asm volatile("sfence" ::: "memory")
#elif defined(__x86_32__)
#define mb()  asm volatile("lock; addl $0,0(%%esp)", "mfence", (0*32+26))
#define rmb() asm volatile("lock; addl $0,0(%%esp)", "lfence", (0*32+26))
#define wmb() asm volatile("lock; addl $0,0(%%esp)", "sfence", (0*32+25))
#else
#error Memory barriers not implemented
#endif

#define REDUCE_KB(X) (X >> 10)
#define REDUCE_MB(X) (X >> 20)
#define REDUCE_GB(X) (X >> 30)

#define KB(X) (X << 10)
#define MB(X) (X << 20)
#define GB(X) (((unsigned long)X) << 30)


///////////////////////////////////////////////////////////////////////////////
// Macros for performance optimization
///////////////////////////////////////////////////////////////////////////////

#define likely(x)    __builtin_expect (!!(x), 1)
#define unlikely(x)  __builtin_expect (!!(x), 0)

/** 
 * Forward align a pointer
 * 
 * @param p Pointer to check
 * @param alignment Alignment in bytes
 * 
 * @return Aligned pointer
 */
INLINE static 
void * align_pointer(void * p, unsigned alignment) {
  return (void*)  ((((mword_t)p) & ~((mword_t)alignment - 1UL)) + alignment);
}

/** 
 * Check pointer alignment
 * 
 * @param p Pointer to check
 * @param alignment Alignment in bytes
 * 
 * @return 
 */
INLINE static 
bool check_aligned(void * p, unsigned alignment) { 
  return  (!(((unsigned long)p) & (alignment - 1UL) )); 
}

/** 
 * Checks whether or not the number is power of 2. 
 */
INLINE static 
bool is_power_of_two(unsigned long x) { 
  return (x != 0) && ((x & (x - 1UL)) == 0);
}

/** 
 * Round up to nearest 4MB aligned
 * 
 */
INLINE addr_t round_up_superpage(addr_t a) {
  /* round up to 4MB super page */
  if((a & ((addr_t)0x3fffff))==0) return a;
  else return (a & (~((addr_t)0x3fffff))) + MB(4);
}      

/** 
 * Round up to nearest 4K aligned
 * 
 * @param a 
 * 
 * @return 
 */
INLINE addr_t round_up_page(addr_t a) {
  /* round up to 4K page */
  if((a & ((addr_t)0xfff))==0) return a;
  else return (a & (~((addr_t)0xfff))) + KB(4);
}      

/** 
 * Round up to 2^N bytes
 * 
 */
INLINE addr_t round_up_log2(addr_t a) {
  int clzl = __builtin_clzl(a);
  int fsmsb = ((sizeof(addr_t) * 8) - clzl);
  if((((addr_t)1) << (fsmsb-1)) == a)
    fsmsb--;  

  return (((addr_t)1) << fsmsb);
}




typedef struct timeval timestamp;

/** Returns current system time. */
INLINE static timestamp now() {
  timestamp t;

  /** 
   * Beware this is doing a system call.
   * 
   */
  gettimeofday(&t, 0);
  return t;
}

/** 
 * Returns the difference between two timestamps in seconds. 
 * @param t1 one timestamp.
 * @param t2 the other timestamp.
 * @return the difference the two timestamps.
 */
INLINE static  
double operator-(const timestamp& t1, const timestamp& t2) {
  return (double)(t1.tv_sec  - t2.tv_sec) + 1.0e-6f * (t1.tv_usec - t2.tv_usec);
}


INLINE unsigned min(unsigned x, unsigned y) {
  return x < y ? x : y;
}

INLINE unsigned max(unsigned x, unsigned y) {
  return x > y ? x : y;
}

/** 
 * Touch memory at huge (2MB) page strides
 * 
 * @param addr Pointer to memory to touch
 * @param size Size of memory in bytes
 */
void touch_huge_pages(void * addr, size_t size);

/** 
 * Touch memory at 4K page strides
 * 
 * @param addr Pointer to memory to touch
 * @param size Size of memory in bytes
 */
void touch_pages(void * addr, size_t size);

/** 
 * Get the cpu is from numa cpu mask
 * 
 * @param mask Cpu bitmask from numa node
 * @param n The nth cpu in the numa node
 * @return The global cpu id
 */
INLINE unsigned get_cpu_id(struct bitmask * mask, unsigned n) {
  uint64_t cpumask = *(mask->maskp);
  unsigned curr_pos = 0;
  unsigned remaining = n;

  do {
    if (cpumask & 1) {
      remaining --;
      if (remaining == 0) break;
    }
    cpumask = cpumask >> 1;
    curr_pos ++;
  } while (remaining != 0);

  return curr_pos;
}


/** 
 * Determines the actual system thread affinities from logical (consecutive)
 * affinities for a given NUMA node. 
 * @param logical_affinities bitset with the logical thread affinities for the
 *                           given NUMA node. 
 * @param numa_node ID of the NUMA node. 
 * @return bitset with actual system affinities. 
 */
Cpu_bitset 
get_actual_affinities(const Cpu_bitset& logical_affinities, 
                      const int numa_node);


#define cpu_relax() asm volatile("pause\n": : :"memory")

#endif // __EXO_UTILS_H__

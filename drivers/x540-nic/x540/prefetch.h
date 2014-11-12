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








#ifndef _PREFETCH_H_
#define _PREFETCH_H_

/**
 * @file
 *
 * Prefetch operations.
 *
 * This file defines an API for prefetch macros / inline-functions,
 * which are architecture-dependent. Prefetching occurs when a
 * processor requests an instruction or data from memory to cache
 * before it is actually needed, potentially speeding up the execution of the
 * program.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prefetch a cache line into all cache levels.
 * @param p
 *   Address to prefetch
 */
static inline void exo_prefetch0(volatile void *p)
{
	asm volatile ("prefetcht0 %[p]" : [p] "+m" (*(volatile char *)p));
}

/**
 * Prefetch a cache line into all cache levels except the 0th cache level.
 * @param p
 *   Address to prefetch
 */
static inline void exo_prefetch1(volatile void *p)
{
	asm volatile ("prefetcht1 %[p]" : [p] "+m" (*(volatile char *)p));
}

/**
 * Prefetch a cache line into all cache levels except the 0th and 1th cache
 * levels.
 * @param p
 *   Address to prefetch
 */
static inline void exo_prefetch2(volatile void *p)
{
	asm volatile ("prefetcht2 %[p]" : [p] "+m" (*(volatile char *)p));
}

#ifdef __cplusplus
}
#endif

#endif /* _PREFETCH_H_ */

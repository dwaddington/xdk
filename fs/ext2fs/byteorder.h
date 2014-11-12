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







/** @addtogroup libc
 * @{
 */
/** @file
 */

#ifndef LIBC_BYTEORDER_H_
#define LIBC_BYTEORDER_H_

#include <stdint.h>

#if defined(i386) 
#define __LE__
#else
#error Unknown endian.
#endif

#if !(defined(__BE__) ^ defined(__LE__))
	#error The architecture must be either big-endian or little-endian.
#endif

#ifdef __BE__

#define uint16_t_le2host(n)  (uint16_t_byteorder_swap(n))
#define uint32_t_le2host(n)  (uint32_t_byteorder_swap(n))
#define uint64_t_le2host(n)  (uint64_t_byteorder_swap(n))

#define uint16_t_be2host(n)  (n)
#define uint32_t_be2host(n)  (n)
#define uint64_t_be2host(n)  (n)

#define host2uint16_t_le(n)  (uint16_t_byteorder_swap(n))
#define host2uint32_t_le(n)  (uint32_t_byteorder_swap(n))
#define host2uint64_t_le(n)  (uint64_t_byteorder_swap(n))

#define host2uint16_t_be(n)  (n)
#define host2uint32_t_be(n)  (n)
#define host2uint64_t_be(n)  (n)

#else

#define uint16_t_le2host(n)  (n)
#define uint32_t_le2host(n)  (n)
#define uint64_t_le2host(n)  (n)

#define uint16_t_be2host(n)  (uint16_t_byteorder_swap(n))
#define uint32_t_be2host(n)  (uint32_t_byteorder_swap(n))
#define uint64_t_be2host(n)  (uint64_t_byteorder_swap(n))

#define host2uint16_t_le(n)  (n)
#define host2uint32_t_le(n)  (n)
#define host2uint64_t_le(n)  (n)

#define host2uint16_t_be(n)  (uint16_t_byteorder_swap(n))
#define host2uint32_t_be(n)  (uint32_t_byteorder_swap(n))
#define host2uint64_t_be(n)  (uint64_t_byteorder_swap(n))

#endif

#define htons(n)  host2uint16_t_be((n))
#define htonl(n)  host2uint32_t_be((n))
#define ntohs(n)  uint16_t_be2host((n))
#define ntohl(n)  uint32_t_be2host((n))

static inline uint64_t uint64_t_byteorder_swap(uint64_t n)
{
	return ((n & 0xff) << 56) |
	    ((n & 0xff00) << 40) |
	    ((n & 0xff0000) << 24) |
	    ((n & 0xff000000LL) << 8) |
	    ((n & 0xff00000000LL) >> 8) |
	    ((n & 0xff0000000000LL) >> 24) |
	    ((n & 0xff000000000000LL) >> 40) |
	    ((n & 0xff00000000000000LL) >> 56);
}

static inline uint32_t uint32_t_byteorder_swap(uint32_t n)
{
	return ((n & 0xff) << 24) |
	    ((n & 0xff00) << 8) |
	    ((n & 0xff0000) >> 8) |
	    ((n & 0xff000000) >> 24);
}

static inline uint16_t uint16_t_byteorder_swap(uint16_t n)
{
	return ((n & 0xff) << 8) |
	    ((n & 0xff00) >> 8);
}

#endif

/** @}
 */

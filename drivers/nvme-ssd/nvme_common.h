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

#ifndef __NVME_COMMON_H__
#define __NVME_COMMON_H__

#include "nvme_registers.h"

#define NVME_VERBOSE

#ifdef NVME_VERBOSE
void NVME_INFO(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else 
#define NVME_INFO(...)
#endif


//#define NVME_LOOP_VERBOSE
#ifdef  NVME_LOOP_VERBOSE
#define NVME_PRINT(f, a...)  fprintf( stdout, "[NVME]: %s:" f "\n",  __func__ , ## a)
#else
#define NVME_PRINT(f, a...)
#endif

#define NVME_LOOP_LABEL( condition, needStop, label )         \
{                                                             \
  unsigned long long attempts = 0;                            \
  while( condition ) {                                        \
    attempts++;                                               \
    if(attempts%1000000 == 0){                                \
      NVME_PRINT("attempts = %llu (%s)", attempts, label);    \
    }                                                         \
    if(needStop && attempts > 1000000000ULL) {                \
      NVME_PRINT("Timed out !!!!!");                          \
      assert(false);                                          \
    }                                                         \
  }                                                           \
}

#define NVME_LOOP( condition, needStop )           \
        NVME_LOOP_LABEL( condition, needStop, "")



///////////////////////////////////////////////////////////////////////////////
// Macros for performance optimization
///////////////////////////////////////////////////////////////////////////////

#define _likely(x)    __builtin_expect (!!(x), 1)
#define _unlikely(x)  __builtin_expect (!!(x), 0)

////////////////////
// Ring Bell Burst
////////////////////

#define SQ_MAX_BATCH_TO_RING (1)
#define CQ_MAX_BATCH_TO_RING (1)
#define US_PER_RING (100) /* Ring every ~100us */



#endif

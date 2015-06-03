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

#ifndef __COMP_TYPES_H__
#define __COMP_TYPES_H__

#include <stdint.h>

typedef int                     status_t;
//typedef uint64_t                cpu_mask_t;

typedef unsigned long           addr_t;
typedef unsigned long           umword_t;
typedef signed long             mword_t;

typedef uint32_t                addr32_t;
typedef unsigned long long int  addr64_t;

typedef unsigned char           byte;
typedef uint16_t                word;
typedef uint32_t                dword;
typedef void *                  handle_t;
typedef uint32_t                core_id_t;

typedef uint64_t                atomic_t;

typedef uint64_t                cpu_time_t;

typedef int                     numa_node_t;


namespace Component
{
  typedef void * interface_t;
  typedef void * device_handle_t;
  typedef void * allocator_handle_t;
  typedef void * pkt_buffer_t;
  typedef void * arg_t;
  typedef void * params_config_t;
  typedef unsigned allocator_t;
  typedef unsigned state_t;
}
  typedef void * io_request_t;
  typedef void * notify_t;
  typedef void * config_t;
#endif

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






#ifndef __NVME_TYPES_H__
#define __NVME_TYPES_H__

typedef unsigned vector_t;
typedef unsigned core_id_t;

namespace NVME
{
  typedef enum { 
    QTYPE_SUB=0x0,
    QTYPE_COMP=0x1,
  } queue_type_t;

  enum {
    BLOCK_SIZE=512,
  };
}

typedef void (*notify_callback_t)(unsigned, void *);
typedef unsigned callback_handle_t;

/* IO descriptor */
enum ACTION {
  NVME_READ = 0,
  NVME_WRITE
};

typedef struct {
  ACTION    action;
  void*     buffer_virt;
  addr_t    buffer_phys;
  off_t     offset;
  size_t    num_blocks;
  unsigned  port;
}__attribute__((aligned(64))) io_descriptor_t;


#endif // __NVME_TYPES_H__

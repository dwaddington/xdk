/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/*
  Authors:
  Copyright (C) 2015, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#include "common.h"

extern struct list_head g_pkdevice_list;

/** 
 * Check to ensure that the specified physical memory
 * belongs to the callng process (and that the memory)
 * was allocated by PK
 * 
 * @param addr Physical address being requested
 * 
 * @return 1 if valid, 0 otherwise
 */
struct pk_dma_area * get_owned_dma_area(addr_t addr)
{
  struct pk_device * d;
  int curr_task_pid = task_pid_nr(current);

  // TODO: list locking

  /* iterate over pk_devices */
  list_for_each_entry(d,&g_pkdevice_list,list) {

    /* iterate over DMA area list */
    struct list_head * p;

    list_for_each(p, &d->dma_area_list_head) {
      struct pk_dma_area * dma_area;
      dma_area = list_entry(p, struct pk_dma_area, list);
      if(dma_area) {
        PLOG("DMA entry: %x 0x%llx",dma_area->owner_pid, dma_area->phys_addr);
        if ((dma_area->owner_pid == curr_task_pid) &&
            (dma_area->phys_addr == addr)) 
          return dma_area; /* bingo */
      }
    }
  }
  return NULL;
}

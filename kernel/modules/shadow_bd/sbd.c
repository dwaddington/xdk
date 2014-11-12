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






#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#include "sbd.h"
#include "partition.h"

#define SBD_DEVICE_SIZE 1024 /* sectors */
/* So, total device size = 1024 * 512 bytes = 512 KiB */

/* Array where the disk stores its data */
static u8 *dev_data;

int sbd_init(void)
{
	dev_data = vmalloc(SBD_DEVICE_SIZE * SBD_SECTOR_SIZE);
	if (dev_data == NULL)
		return -ENOMEM;
	/* Setup its partition table */
	copy_mbr_n_br(dev_data);
	return SBD_DEVICE_SIZE;
}

void sbd_cleanup(void)
{
	vfree(dev_data);
}

void sbd_write(sector_t sector_off, u8 *buffer, unsigned int sectors)
{
	memcpy(dev_data + sector_off * SBD_SECTOR_SIZE, buffer,
		sectors * SBD_SECTOR_SIZE);
}
void sbd_read(sector_t sector_off, u8 *buffer, unsigned int sectors)
{
	memcpy(buffer, dev_data + sector_off * SBD_SECTOR_SIZE,
		sectors * SBD_SECTOR_SIZE);
}

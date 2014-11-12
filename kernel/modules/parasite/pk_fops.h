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
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __PK_FOPS_H__
#define __PK_FOPS_H__

#include <linux/types.h>
#include <linux/device.h>

#include "pk.h"

extern const struct file_operations pk_fops;
extern struct cdev *pk_cdev;

status_t fops_init(void);
status_t fops_cleanup(void);

#define PK_IOCTL_DMA_ALLOC _IOW('U', 200, int)

#endif // __PK_FOPS_H__

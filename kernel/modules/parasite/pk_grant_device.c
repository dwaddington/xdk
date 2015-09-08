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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>

/** 
 * Device grant means assigning a device to a particular process if
 * the device has not already been assigned.  This will control
 * access to the device PCI config space and IO memory space.
 * 
 */

static ssize_t pk_grant_device_store(struct device * dev,
                                     struct device_attribute *attr, 
                                     const char * buf,
                                     size_t count)
{
  return count;
}

static ssize_t pk_grant_device_show(struct device *dev,
                                    struct device_attribute *attr, 
                                    char *buf)
{
  return sprintf(buf, "%s\n", "yo! grant!!");
}

/** 
 * Device attribute declaration
 * 
 */
DEVICE_ATTR(grant_device, S_IRUGO | S_IWUGO, pk_grant_device_show, pk_grant_device_store);

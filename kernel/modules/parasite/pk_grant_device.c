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
#include <linux/sched.h>
#include <linux/pci.h>

#include "pk.h"

extern struct list_head g_pkgrant_list;
extern spinlock_t g_pkgrant_list_lock;
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
  int domain, bus, devfn, uid, op;
  struct pci_dev * pcidev;

  /* can only be called by root/sudo user */
  if(get_current_user()->uid.val != 0) {
    PERR("call to grant device (store) as non-root user");
    return -EINVAL;
  }

  if(sscanf(buf,"%x %x %x %d %d", 
            &domain, 
            &bus, 
            &devfn,
            &uid,
            &op) != 4) {
    PERR("invalid call to grant device (store). params <domain> <bus> <fn> <uid> <op>");
    return -EINVAL;
  }

  /* locate PCI device */
  pcidev = pci_get_domain_bus_and_slot(domain,bus,devfn);
  if(pcidev == NULL) {
    PERR("unable to find device (domain=%x bus=%x fn=%x)",domain,bus,devfn);
    return -EINVAL;
  }


  if(op == 0) { // grant device

    /* if this device is already in the grant list, return -EINVAL */
    {
      struct pk_device_grant * grant;
      struct list_head *p, *safetmp;
    
      spin_lock(&g_pkgrant_list_lock);

      /* check that it is not in the list already */
      list_for_each_safe(p, safetmp, &g_pkgrant_list) {
        grant = list_entry(p, struct pk_device_grant, list);
        if(grant->pci_dev == pcidev) {
          PERR("device already assigned");
          spin_unlock(&g_pkgrant_list_lock);
          return -EINVAL;
        }
      }
      {
        struct pk_device_grant * new_grant = kzalloc(sizeof(struct pk_device_grant), 
                                                     GFP_KERNEL);
        BUG_ON(new_grant == NULL);
        list_add(&new_grant->list, &g_pkgrant_list);
      }

      spin_unlock(&g_pkgrant_list_lock);
    }
  }
  else if(op == 1) { // un-grant device

    struct pk_device_grant * grant;
    struct list_head *p, *safetmp;

    // scan list and remove on match
    spin_lock(&g_pkgrant_list_lock);

    list_for_each_safe(p, safetmp, &g_pkgrant_list) {
      grant = list_entry(p, struct pk_device_grant, list);
      if(grant->pci_dev == pcidev) {
        list_del(p);
        PLOG("device ungrant OK.");
        kfree(grant); // free memory
        spin_unlock(&g_pkgrant_list_lock);
        return count;
      }
    }
    PERR("cannot find device grant entry to ungrant");
    spin_unlock(&g_pkgrant_list_lock);

    return -EINVAL;
  }
  else { // unknown op
    PLOG("invalid op param to grant device call.");
  }

  return count;
}


static ssize_t pk_grant_device_show(struct device *dev,
                                    struct device_attribute *attr, 
                                    char *buf)
{  
  return sprintf(buf, "uid:%u %s\n", 
                 get_current_user()->uid.val,
                 "yo! grant!!");
}

/** 
 * Device attribute declaration
 * 
 */
DEVICE_ATTR(grant_device, S_IRUGO | S_IWUGO, pk_grant_device_show, pk_grant_device_store);

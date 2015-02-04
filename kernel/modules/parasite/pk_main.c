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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>

/* #include <linux/uio_driver.h> */

#include "pk_fops.h"
#include "pk.h"

#define DEVICE_NAME "parasite"
#define PARASITIC_KERNEL_VER "0.1"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel G. Waddington");
MODULE_DESCRIPTION("Parasitic Kernel Module");

struct class * parasite_class;


/* attributes for each pkXXX device created - defined in pk_class.c */
extern struct pci_driver pci_driver;
extern struct device_attribute dev_attr_name;
extern struct device_attribute dev_attr_version;
extern struct device_attribute dev_attr_pci;
extern struct device_attribute dev_attr_irq;
extern struct device_attribute dev_attr_irq_mode;
extern struct device_attribute dev_attr_dma_mask;
extern struct device_attribute dev_attr_dma_page_alloc;
extern struct device_attribute dev_attr_dma_page_free;
extern struct device_attribute dev_attr_msi_alloc;
extern struct device_attribute dev_attr_msi_cap;

static ssize_t  pk_detach_store(struct class *class, 
                                struct class_attribute *attr,
                                const char *buf, size_t count);
static status_t major_init(void);
static void     major_cleanup(void);
static int      get_minor(struct pk_device *dev);
void            free_minor(struct pk_device *dev);

static int          pk_major;
static DEFINE_MUTEX(minor_lock);
static DEFINE_IDR(pk_idr);

LIST_HEAD(g_pkdevice_list);

struct proc_dir_entry * pk_proc_dir_root;

static inline struct page *__last_page(void *addr, unsigned long size)
{
  return virt_to_page(addr + (PAGE_SIZE << get_order(size)) - 1);
}

/** 
 * Called by the probe function to register a new 
 * device.  Probe should have done the right
 * checks on the device at this point.
 * 
 * @param pci_dev 
 * 
 * @return 
 */
struct pk_device * sysfs_class_register_device(struct pci_dev * pci_dev)
{
	struct pk_device *pkdev;
	int ret = 0;

  if(!pci_dev)
    return NULL;

  /* allocate memory for new pk_device */
	pkdev = kzalloc(sizeof(*pkdev), GFP_KERNEL);
	if (!pkdev) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

  /* init pk_device structure */
  memset((void*)pkdev,0,sizeof(struct pk_device));

  INIT_LIST_HEAD(&pkdev->dma_area_list_head);
  spin_lock_init(&pkdev->dma_area_list_lock);

  INIT_LIST_HEAD(&pkdev->list);
	init_waitqueue_head(&pkdev->irq_wait);
  pkdev->magic = PK_MAGIC;
  pkdev->irq_mode = IRQ_MODE_NONMASKING;
  
  /* populate class device structure */
  pkdev->pci_dev = pci_dev;
  PLOG("(*) %u:%u\n",pci_dev->bus->number,pci_dev->bus->primary);

	ret = get_minor(pkdev);
	if (ret)
		goto err_get_minor;

  /* set up name */
  snprintf(pkdev->name,32,"pk%d",pkdev->minor);

  /* create new device with sysfs */
  BUG_ON(parasite_class == NULL);
	pkdev->dev = device_create(parasite_class, /* class to which this device belongs */
                             NULL, // no parent
                             MKDEV(pk_major, pkdev->minor), 
                             pkdev, // drvdata call back data
                             "pk%d", pkdev->minor); /* device name */
  BUG_ON(pkdev->dev == NULL);


	if(IS_ERR(pkdev->dev)) {
		PERR("device_create: device register failed");
		ret = PTR_ERR(pkdev->dev);
		goto err_device_create;
	}

  /* create device attributes */
  device_create_file(pkdev->dev, &dev_attr_name);
  device_create_file(pkdev->dev, &dev_attr_version);
  device_create_file(pkdev->dev, &dev_attr_pci);
  device_create_file(pkdev->dev, &dev_attr_irq);
  device_create_file(pkdev->dev, &dev_attr_irq_mode);
  device_create_file(pkdev->dev, &dev_attr_dma_mask);
  device_create_file(pkdev->dev, &dev_attr_dma_page_alloc);
  device_create_file(pkdev->dev, &dev_attr_dma_page_free);
  device_create_file(pkdev->dev, &dev_attr_msi_alloc);
  device_create_file(pkdev->dev, &dev_attr_msi_cap);

  /* create procfs dir */
  ASSERT(pk_proc_dir_root!=NULL);

  PLOG("creating /proc/parasite/%s entry.",pkdev->name);
  pkdev->msi_proc_dir_root = proc_mkdir(pkdev->name,pk_proc_dir_root);

  /* finally add to list of pk_devices */
  list_add(&pkdev->list,&g_pkdevice_list);

	return pkdev;

err_device_create:
	free_minor(pkdev);
err_get_minor:
	kfree(pkdev);
err_kzalloc:
	return NULL;
}


/** 
 * Called to free MSI vectors and memory resources associated with a pk_device instance
 * 
 * @param d Pointer to pk_device instance
 */
void pk_device_cleanup(struct pk_device * pkdev)
{
  struct list_head * p, * safetmp;
  int msi;
  
  ASSERT(pkdev);
  ASSERT(pkdev->msi_proc_dir_root);

  PDBG("cleaning up pk_device @ %p",(void*) pkdev);

  msi = pkdev->msi_support;

  /* remove /proc/parasite/pkXXX/NNN entries */
  if (msi & 0x2) {

    unsigned i;
    char entry_name[16];

    for(i = 0; i < pkdev->msi_proc_dir_entry_num ; i++) {
      sprintf(entry_name,"msix-%d",pkdev->msix_entry[i].vector);

      /* remove msix-XXX from /proc/parasite/pkNNN/ */
      PLOG("removing entry [%s]",entry_name);
      remove_proc_entry(entry_name, pkdev->msi_proc_dir_root);

      /* unhook irq handler */
      PLOG("freeing MSI-X IRQ vector %d", pkdev->msix_entry[i].vector);
      free_irq(pkdev->msix_entry[i].vector, (void*)(&pkdev->msi_irq_wait_queue[i]));
    }
    /* remove /proc/parasite/pkNNN */
    PLOG("removing (%s) entry in /proc/parasite", pkdev->name);
    remove_proc_entry(pkdev->name, pk_proc_dir_root);

    /* release MSIX vectors */
    //pci_msi_off(pkdev->pci_dev);
    pci_disable_msix(pkdev->pci_dev);
    pci_disable_device(pkdev->pci_dev);

    pkdev->msi_proc_dir_entry_num = 0;
    pkdev->msix_entry_num = 0;
  }
  else if(msi & 0x1) {

    unsigned i, base = pkdev->pci_dev->irq;
    char entry_name[16];
    
    for(i = 0; i < pkdev->msi_proc_dir_entry_num ; i++) {
      sprintf(entry_name,"msi-%d",base+i);
      /* remove msi-XXX from /proc/parasite/pkNNN/ */
      PLOG("removing entry [%s]",entry_name);
      remove_proc_entry(entry_name, pkdev->msi_proc_dir_root);

      /* unhook irq handler */
      PLOG("free MSI-X IRQ vector %d", pkdev->msix_entry[i].vector);
      free_irq(base+i,(void*)(&pkdev->msi_irq_wait_queue[i]));
    }

    /* remove /proc/parasite/pkNNN */
    remove_proc_entry(pkdev->name, pk_proc_dir_root);

    /* release MSI vectors */
    //    pci_msi_off(pkdev->pci_dev);
    pci_disable_msi(pkdev->pci_dev);
    pci_disable_device(pkdev->pci_dev);

    pkdev->msi_proc_dir_entry_num = 0;
    pkdev->msi_entry_num = 0;
  }


  PLOG("/proc entries removed.");

  /* free DMA areas */
  list_for_each_safe(p, safetmp, &pkdev->dma_area_list_head) {

    struct pk_dma_area * area = list_entry(p, struct pk_dma_area, list);
    ASSERT(area);

    PDBG("freeing DMA area %p (pages=%d)", 
         (void*) area->phys_addr, 1 << area->order);
    
    /* clear reserved bit on DMA pages before we free */
    {
      struct page * page = area->p;
      int i;      
      unsigned num_pages = (1 << area->order);
      
      for(i = 0; i < num_pages; i++) {
        ClearPageReserved(page);
        page++; // yes, they are contiguous
      }
    }

    /* remove from list and free memory */
    __free_pages(area->p, area->order);
    list_del(p);

    /* free kernel memory for pk_dma_area */
    kfree(area);
  }

  /* free minor device */
  free_minor(pkdev);

  PLOG("pk_device_cleanup complete.");
}


void release_all_devices(void)
{
  struct pk_device * pkdev, * safetmp;
  list_for_each_entry_safe(pkdev, safetmp, &g_pkdevice_list, list) {
    
    PLOG("removing device (%s)",pkdev->name);   

    pk_device_cleanup(pkdev);

    pci_clear_master(pkdev->pci_dev);
    pci_release_regions(pkdev->pci_dev);
    pci_disable_device(pkdev->pci_dev);

    PLOG("removing from g_pkdevice_list");
    list_del(&pkdev->list); // can't do this in list_for_each_entry_safe
    kfree(pkdev);
  }

  PLOG("release_all_devices OK.");
}


/** 
 * Called when the module is unloaded.
 * 
 */
static void major_cleanup(void)
{
  PDBG("cleaning up driver.");

  release_all_devices();

  /* remove /proc/parasite */
  remove_proc_entry(DEVICE_NAME, NULL);

  /* /dev/ device clean up */
  misc_cleanup();

  /* 
	 * Unregister the device 
	 */
	unregister_chrdev(pk_major, DEVICE_NAME);
  if(parasite_class) {
    class_destroy(parasite_class);
  }
}

/** 
 * Called when the module is loaded.
 * 
 * 
 * @return 
 */
static status_t major_init(void)
{
  static const char name[] = DEVICE_NAME;
  struct cdev *cdev = NULL;
  dev_t pk_device = 0;
  
  int rc = alloc_chrdev_region(&pk_device,
                               0,
                               MAX_DEVICES,
                               name);

  if(rc < 0)
    return E_FAIL;
  
  cdev = cdev_alloc();
  if(!cdev) {
    unregister_chrdev_region(pk_device, MAX_DEVICES);
    return E_FAIL;
  }

  cdev->ops = &pk_fops;
  cdev->owner = THIS_MODULE;

  kobject_set_name(&cdev->kobj, "%s", name);

  rc = cdev_add(cdev, pk_device, MAX_DEVICES);
  ASSERT(rc==0);
  
  pk_major = MAJOR(pk_device);
  pk_cdev = cdev;

  /* create proc dir entry */
  pk_proc_dir_root = proc_mkdir(DEVICE_NAME, NULL);    
  ASSERT(pk_proc_dir_root);

  PLOG("major init complete.");
  return S_OK;
}

/*------------------------------------------------------------*/
/* Show and store methods for files in '/sys/class/parasite/' */
/*------------------------------------------------------------*/

static ssize_t pk_class_version_show(struct class *class,
                                     struct class_attribute *attr, char *buf)
{
  return sprintf(buf, "%s\n", PARASITIC_KERNEL_VER);
}

/** 
 * Dettach a device from the parasitic kernel
 * echo "pk0" > /sys/class/parasite/detach 
 * 
 * @param class 
 * @param attr 
 * @param buf 
 * @param count 
 * 
 * @return 
 */
static ssize_t pk_detach_store(struct class *class, 
                               struct class_attribute *attr,
                               const char *buf, size_t count) 
{
  char tmp[count];
  struct pk_device * pkdev, * safetmp;

  BUG_ON(count < 1);
  memcpy(tmp,buf,count);
  tmp[count - 1] = '\0';

  PLOG("detach request (%s)", tmp);

  list_for_each_entry_safe(pkdev, safetmp, &g_pkdevice_list, list) {

    if(strcmp(pkdev->name, tmp)==0) {
      PLOG("removing device (%s)",pkdev->name);

      pk_device_cleanup(pkdev);

      /*   PLOG("minor device %d cleaned up OK.",d->minor); */
      pci_clear_master(pkdev->pci_dev);
      pci_release_regions(pkdev->pci_dev);
      pci_disable_device(pkdev->pci_dev);

      list_del(&pkdev->list); // can't do this in list_for_each_entry_safe
      kfree(pkdev);
      break;
    }
  }
  
  return count;
}



static struct class_attribute pk_version =
  __ATTR(version, S_IRUGO, pk_class_version_show, NULL);

static struct class_attribute pk_detach = 
  __ATTR(detach, S_IWUSR, NULL, pk_detach_store);


/** 
 * Write handler for /sys/class/parasite/new_id, which is
 * non-root priviledged command point for adding a device
 * to the PCI system and triggering a probe that gets 
 * ultimately probed by the parasitic module.
 *
 * TODO: at some point we need some secure access control or
 * may be a first-come-first serve policy
 * 
 * @param class Class structure
 * @param attr Attributes for class
 * @param buf User-level buffer
 * @param count Size of user-level buffer
 * 
 * @return Number of bytes successfully written
 */
ssize_t pk_new_id_store(struct class *class, 
                        struct class_attribute *attr,
                        const char *buf, size_t count)
{
  // TODO: add access control
  //

  struct pci_driver *pdrv = &pci_driver; //to_pci_driver(&pci_driver);
  const struct pci_device_id *ids = pdrv->id_table;
	__u32 vendor, device, subvendor=PCI_ANY_ID,
		subdevice=PCI_ANY_ID, devclass=0, devclass_mask=0;
	unsigned long driver_data=0;
	int fields=0;
	int retval;

	fields = sscanf(buf, "%x %x %x %x %x %x %lx",
			&vendor, &device, &subvendor, &subdevice,
			&devclass, &devclass_mask, &driver_data);

	if (fields < 2) {
    PLOG("bad param on new_id call");
		return -EINVAL;
  }

	/* Only accept driver_data values that match an existing id_table
	   entry */
	if (ids) {
		retval = -EINVAL;
		while (ids->vendor || ids->subvendor || ids->class_mask) {
			if (driver_data == ids->driver_data) {
				retval = 0;
				break;
			}
			ids++;
		}
		if (retval)	/* No match */ {
      PLOG("no match");
			return retval;
    }
	}

	retval = pci_add_dynid(pdrv, vendor, device, subvendor, subdevice,
                         devclass, devclass_mask, driver_data);
	if (retval)
		return retval;
	return count;
}

/* allow non-root call of /sys/class/parasite/new_id which
   attaches a device to the parasitic kernel 
*/
static struct class_attribute pk_new_id = 
  __ATTR(new_id, S_IWUSR|S_IWOTH, NULL, pk_new_id_store);


/** 
 * Class registration.  Enters the class 'parasite' in /sys/class filesystem
 * 
 * 
 * @return 
 */
status_t class_init(void)
{
	int ret;

  ret = major_init();
  if(ret!=0) 
    return ret;

	/* create class instance (only one) */
  if(IS_ERR(parasite_class = class_create(THIS_MODULE,"parasite"))) 
    goto err_class_register;

  /* set attributes for class */
  ret = class_create_file(parasite_class, &pk_version);
  if(ret)
    goto err_class_create_file;

  ret = class_create_file(parasite_class, &pk_detach);
  if(ret)
    goto err_class_create_file;

  ret = class_create_file(parasite_class, &pk_new_id);
  if(ret)
    goto err_class_create_file;


	return S_OK;

err_class_create_file:
  class_destroy(parasite_class);

err_class_register: 
  major_cleanup();

	return ret;
}


/** 
 * Get minor device number (i.e., instance of a controlled device)
 * 
 * @param idev 
 * 
 * @return 
 */
static int get_minor(struct pk_device *dev)
{
	int retval = -ENOMEM;
	int id;

	mutex_lock(&minor_lock);
	if (idr_pre_get(&pk_idr, GFP_KERNEL) == 0)
		goto exit;

	retval = idr_get_new(&pk_idr, dev, &id);
	if (retval < 0) {
		if (retval == -EAGAIN)
			retval = -ENOMEM;
		goto exit;
	}

	if (id < MAX_DEVICES) {
		dev->minor = id;
	} 
  else {
		dev_err(dev->dev, "too many pk devices\n");
		retval = -EINVAL;
		idr_remove(&pk_idr, id);
	}

exit:
	mutex_unlock(&minor_lock);
	return retval;
}

void free_minor(struct pk_device *pkdev)
{
  PLOG("freeing minor device.");

	mutex_lock(&minor_lock);
	idr_remove(&pk_idr, pkdev->minor); 
	mutex_unlock(&minor_lock); 

#if 0
  /* delete device attributes */
  device_remove_file(pkdev->dev, &dev_attr_name);
  device_remove_file(pkdev->dev, &dev_attr_version);
  device_remove_file(pkdev->dev, &dev_attr_pci);
  device_remove_file(pkdev->dev, &dev_attr_irq);
  device_remove_file(pkdev->dev, &dev_attr_dma_mask);
  device_remove_file(pkdev->dev, &dev_attr_dma_page_alloc);
  device_remove_file(pkdev->dev, &dev_attr_dma_page_free);
  device_remove_file(pkdev->dev, &dev_attr_msi_alloc);
  device_remove_file(pkdev->dev, &dev_attr_msi_cap);
#endif

  /* clean up device created with device_create API */
  device_destroy(parasite_class, MKDEV(pk_major, pkdev->minor));
}




static int __init pk_init(void)
{
  int rc;

  PLOG("PARASITIC KERNEL loaded.\n");

  /* register base /sys/class/parasite - there is only one class instance.
   */  
  rc = class_init();

  if(rc!=0) {
    PLOG("failed class init.(rc=%d)",rc);
    return rc;
  }

  /* register as PCI device, enabling probe via
     echo "8086 100e" > /sys/bus/pci/drivers/parasite/new_id
  */
  pci_init();

  /* initialize a misc device in /dev/ */
  misc_init();

	return 0;
}

static void __exit pk_exit(void)
{
  pci_cleanup();

  major_cleanup();

  printk("[PK]: PARASITIC KERNEL unloaded OK.\n");
}

module_init(pk_init)
module_exit(pk_exit)
MODULE_LICENSE("GPL v2");

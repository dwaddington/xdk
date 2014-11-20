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
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>

#include "pk.h"

extern struct list_head g_pkdevice_list;

struct cdev *pk_cdev;
const struct file_operations pk_fops;

static int fops_open(struct inode *inode, struct file *filep);
static int fops_release(struct inode *inode, struct file *filep);
//static long fop_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int fops_mmap(struct file *file, struct vm_area_struct *vma);


/** 
 * Command misc device
 * 
 */
static struct miscdevice pk_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "parasite",
	.fops = &pk_fops,
  .mode = S_IRUGO | S_IWUGO,
};


/* static struct file_operations uio_dma_fops = { */
/* 	.owner	 = THIS_MODULE, */
/* 	.llseek  = no_llseek, */
/* 	.read	 = uio_dma_read, */
/* 	.write	 = uio_dma_write, */
/* 	.poll	 = uio_dma_poll, */
/* 	.open	 = uio_dma_open, */
/* 	.release = uio_dma_close, */
/* 	.mmap	 = uio_dma_mmap, */
/* 	.unlocked_ioctl = uio_dma_ioctl, */
/* }; */

const struct file_operations pk_fops = {
  .owner          = THIS_MODULE,
  .open           = fops_open, 
  .release        = fops_release,
  //  .unlocked_ioctl = fop_ioctl,
  .mmap	          = fops_mmap,
};


static void pk_dma_vm_open(struct vm_area_struct *vma)
{
}

static void pk_dma_vm_close(struct vm_area_struct *vma)
{
}

static int pk_dma_vm_fault(struct vm_area_struct *area, 
                        struct vm_fault *fdata)
{
  return VM_FAULT_SIGBUS;
}

static struct vm_operations_struct pk_dma_mmap_fops = {
  .open   = pk_dma_vm_open,
  .close  = pk_dma_vm_close,
  .fault  = pk_dma_vm_fault
};

/** 
 * Register the module as a misc device.  Memory services etc. are provided
 * through a top-level ioctl service.
 * 
 */
void misc_init()
{
  /* create misc device */
  if (misc_register(&pk_miscdev))
    panic("misc_register failed unexpectedly.");

  PLOG("Registered misc device OK.");
}

void misc_cleanup()
{
  misc_deregister(&pk_miscdev);
}


static int fops_open(struct inode *inode, struct file *filep) 
{
  return 0;
}

static int fops_release(struct inode *inode, struct file *filep)
{
  return 0;
}

/** 
 * Basic sanity checking to ensure that mmap calls applied through
 * /dev/parasite are a.) allocated by the PK and b.) belong to the
 * calling process.
 *
 * TODO: add some size checking and list safety
 * 
 * @param addr Physical address being requested
 * 
 * @return 1 if valid, 0 otherwise
 */
static int __verify_phys_owner(addr_t addr)
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
        PLOG("DMA entry: %x 0x%lx",dma_area->owner_pid, dma_area->phys_addr);
        if ((dma_area->owner_pid == curr_task_pid) &&
            (dma_area->phys_addr == addr)) 
          return 1; /* bingo */
      }
    }

  }
  return 0;
}

static int fops_mmap(struct file *file, struct vm_area_struct *vma)
{
  unsigned long start  = vma->vm_start;
  unsigned long size   = vma->vm_end - vma->vm_start;

  //  unsigned long offset = vma->vm_pgoff * PAGE_SIZE;

  /* PDBG("file->private_data = %p",file->private_data); */
  /* PDBG("fops_mmap: start = %lx ; size = %lx ; offset = %lx", start, size, offset); */

  //vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); /* disable cache */
  //vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); /* cache write combine */

  /* check this area was allocated by the parasitic kernel and
     also check the owner pid
  */

  // TOFIX:
  // !!! For the moment switch off. Since I want to use this for device mapping 
  // we need to add checks to see if the memory belongs to this device.
  //  if(!__verify_phys_owner(offset))
  //    return -EINVAL;

  {
    unsigned long pfn = vma->vm_pgoff; //(offset >> PAGE_SHIFT);
    //    struct mm_struct *mm = current->mm;

    /* hold mm semaphore for the process */
    //    down_write(&mm->mmap_sem);

    if (remap_pfn_range(vma, 
                        start, 
                        pfn, 
                        size,
                        vma->vm_page_prot)) {
      PLOG("remap_pfn_range failed.");
      //      up_write(&mm->mmap_sem);
      return -EIO;
    }
    //    up_write(&mm->mmap_sem);
  }

  vma->vm_ops = &pk_dma_mmap_fops;
                
  return 0;
}


status_t fops_init(void)
{
  /* if(fops_major_init()!=S_OK) { */
  /*   PERR("major init failed."); */
  /*   return E_FAIL; */
  /* } */
  return S_OK;
}

status_t fops_cleanup(void)
{
  //  fops_major_cleanup();
  return S_OK;
}

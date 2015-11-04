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

#include "common.h"
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


static int fops_mmap(struct file *file, struct vm_area_struct *vma)
{
  if(vma->vm_end < vma->vm_start)
    return -EINVAL;

  //  unsigned long offset = vma->vm_pgoff * PAGE_SIZE;

  /* PDBG("file->private_data = %p",file->private_data); */

  //vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); /* disable cache */
  //vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); /* cache write combine */

  /* check this area was allocated by the parasitic kernel and
     also check that it is owned by the current task
  */
  {
    addr_t phys = vma->vm_pgoff << PAGE_SHIFT;
    struct pk_dma_area * area = get_owned_dma_area(phys);
    if(!area) {
      PWRN("DMA area (%p) is not owned by caller nor is it shared",(void *) phys);
      return -EPERM;
    }
  }

  if (remap_pfn_range(vma, 
                      vma->vm_start,
                      vma->vm_pgoff, // passed through physical address 
                      vma->vm_end - vma->vm_start,
                      vma->vm_page_prot)) {
    PLOG("remap_pfn_range failed.");
    return -EAGAIN;
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

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

#ifndef __PK_H__
#define __PK_H__

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

#define MAX_DEVICES (1U << MINORBITS)
#define MAX_MSI_VECTORS_PER_DEVICE 32
#define MAX_MSIX_VECTORS_PER_DEVICE 32
#define MAX_PORT_REGIONS 5
#define MAX_MEMORY_MAPPED_IO_REGIONS 5

#define CONFIG_DEBUG

#ifdef CONFIG_DEBUG
#define PDBG(f, a...)	printk( "[PK]: %s:" f "\n",  __func__ , ## a);
#else
#define PDBG(f, a...)
#endif

#define PINF(f, a...)	printk( KERN_INFO "[PK]: %s:" f "\n",  __func__ , ## a);
#define PWRN(f, a...)	printk( KERN_INFO "[PK]: %s:" f "\n",  __func__ , ## a);
#define PLOG(f, a...)	printk("[PK]: " f "\n",  ## a);
#define PERR(f, a...)	printk( KERN_ERR "[PK]: ERROR %s:" f "\n",  __func__ , ## a);

#define ASSERT(x) if(!(x)) { printk("ASSERT: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); panic("Assertion failed!"); }

typedef int status_t;
typedef unsigned long addr_t;

enum {
  S_OK = 0,
  E_FAIL = -1,
};

enum {
  DMA_AREA_FLAG_SHARED_ALL = 0x0,
};

/** 
 * Types
 * 
 */
struct pk_dma_area {
  struct list_head list;
  void *    p;
  int              node_id;
  unsigned         order;
  dma_addr_t       phys_addr;
  int              owner_pid;
  unsigned         flags;
};

#define PK_MAGIC 0xfeeb1e00

struct pk_device;

struct interrupt_wait_queue_t {
  wait_queue_head_t  q;
  struct pk_device * pkd;
  unsigned irq;            /* IRQ vector associated with this Q */
  unsigned signalled;      /* used by the wait event mechanism */
};

typedef enum { 
  IRQ_MODE_NONMASKING=1,
  IRQ_MODE_MASKING=2,
} irq_mode_t;

/** 
 * Main data structure for a device instance in the parasitic kernel
 * 
 */
struct pk_device {
  u32                            magic;
  char                           name[16];
  struct device	*                dev; 
  struct kobject *               ioctl_obj;
  struct pci_dev *               pci_dev;
	int			                       minor;
	wait_queue_head_t	             irq_wait;
  int                            irq_wait_flag;
  irq_mode_t                     irq_mode;
  struct list_head               list;
  unsigned                       memory_mapped_io_region_count;
  struct memory_region *         memory_mapped_io_regions[MAX_MEMORY_MAPPED_IO_REGIONS];
  struct list_head               dma_area_list_head;
  spinlock_t                     dma_area_list_lock;
  int                            msi_support;
  union {
    unsigned                     msix_entry_num;
    unsigned                     msi_entry_num;
  };
  struct msix_entry *            msix_entry;
  struct proc_dir_entry *        msi_proc_dir_root;
  struct proc_dir_entry *        msi_proc_dir_entry[MAX_MSI_VECTORS_PER_DEVICE];
  unsigned                       msi_proc_dir_entry_num;
  struct interrupt_wait_queue_t  msi_irq_wait_queue[MAX_MSI_VECTORS_PER_DEVICE];
  int                            exit_flags;
};

#define CHECK_MAGIC(X) ASSERT(X->magic == PK_MAGIC)

/*
struct proc_dir_entry *proc_create_data(const char *name,
	mode_t mode, struct proc_dir_entry *parent,
	const struct file_operations *proc_fops, void *data)
*/

//irqreturn_t (*irq_handler_t)(int, void *);

// struct pk_info {
//   struct pk_device *     pk_dev;
//   const char *           name;
//   const char *           version;
//   struct memory_region   mem[MAX_MEMORY_MAPPED_IO_REGIONS];
//   struct port_region     port[MAX_PORT_REGIONS];
//   /* long                    irq; */
//   /* unsigned long           irq_flags; */
//   /* void                    *priv; */
//   /* irqreturn_t (*handler)(int irq, struct uio_info *dev_info); */
//   /* int (*mmap)(struct uio_info *info, struct vm_area_struct *vma); */
//   /* int (*open)(struct uio_info *info, struct inode *inode); */
//   /* int (*release)(struct uio_info *info, struct inode *inode); */
//   /* int (*irqcontrol)(struct uio_info *info, s32 irq_on); */
// };


struct pk_device * sysfs_class_register_device(struct pci_dev * pci_dev);

/** 
 * PCI functions
 * 
 */
void pci_init(void);
void pci_cleanup(void);

void misc_init(void);
void misc_cleanup(void);



#endif // __PK_H__

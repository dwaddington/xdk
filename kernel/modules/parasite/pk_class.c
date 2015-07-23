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
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/mmzone.h>
#include <linux/delay.h>
#include <linux/msi.h>
#include <linux/highmem.h>
#include <linux/cred.h> /* see https://www.kernel.org/doc/Documentation/security/credentials.txt */

#include "pk.h"
#include "pk_fops.h"
#include "config.h"
#include "common.h"

extern struct proc_dir_entry * pk_proc_dir_root;
extern void pk_device_cleanup(struct pk_device * pkdev);

static ssize_t show_name(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
  return sprintf(buf, "XDK parasitic driver\n");
}


static ssize_t show_version(struct device *dev, 
                            struct device_attribute *attr,  
                            char *buf) 
{
  /* struct pk_device *idev = dev_get_drvdata(dev); */
  /* return sprintf(buf, "%s\n", idev->info->version); */
  return sprintf(buf, "0.1\n");
}


/** 
 * This attribute (pci) is used so that user-space processes can easily
 * get hold of the /sys/bus/pci/... entry for the device.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * 
 * @return 
 */
static ssize_t show_pci(struct device *dev,
                        struct device_attribute *attr, 
                        char *buf)
{
  struct pci_dev * pcidev;
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);

  if(!pkdev) 
    goto error;

  pcidev = pkdev->pci_dev;
  if(!pcidev || !pcidev->bus) 
    goto error;

  return sprintf(buf, "/sys/bus/pci/devices/%04u:%02x:%02x.%x\n",
                 //                 pci_is_pcie(pcidev) ? "_express" : "",
                 pci_domain_nr(pcidev->bus),
                 pcidev->bus->number,
                 PCI_SLOT(pcidev->devfn),
                 PCI_FUNC(pcidev->devfn));

 error:
  return sprintf(buf, "error: %s\n",__FUNCTION__);
}


/** 
 * Show call for irq attribute.  Client does a blocking read on this
 * to wait for an interrupt.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * 
 * @return 
 */
static ssize_t wait_irq(struct device *dev,
                        struct device_attribute *attr, 
                        char *buf)
{
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);

  if(!pkdev) 
    goto error;

  wait_event_interruptible(pkdev->irq_wait, pkdev->irq_wait_flag != 0);
  pkdev->irq_wait_flag = 0;

  PDBG("interrupt (0x%x) fired and handled. Returning OK! code.",pkdev->pci_dev->irq);
  
  /* return the IRQ number */
  return sprintf(buf, "%d\n", pkdev->pci_dev->irq);

 error:
  return sprintf(buf, "error: %s\n",__FUNCTION__);
}


/*--------------------------------------------------------*/

/** 
 * Attribute used to get the current DMA mask for the device.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * 
 * @return 
 */
static ssize_t dma_mask_show(struct device *dev,
                             struct device_attribute *attr, 
                             char *buf)
{
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;

  return sprintf((char*) buf, "%llx\n", pkdev->pci_dev->dma_mask);

 error:
  PERR("dev_get_drvdata returned a NULL pointer.");
  return 0;
}

/** 
 * Attribute used to set the DMA mask for the device.  This is 
 * important to ensure the appropriate DMA memory is allocated. 
 * The exokernel driver should call this with the appropriate 
 * mask for the device.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * @param count 
 * 
 * @return 
 */
static ssize_t dma_mask_store(struct device * dev,
                              struct device_attribute *attr, 
                              const char * buf,
                              size_t count)
{
  u64 new_mask = 0ULL;
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);
  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;
  
  if (sscanf(buf,"%llx",&new_mask)!=1)
    return -EINVAL;

  if (pci_set_dma_mask(pkdev->pci_dev,new_mask)!=0) {
    PWRN("pci_set_dma_mask failed. invalid mask?");
    return -EIO;
  }
  else if (pci_set_consistent_dma_mask(pkdev->pci_dev, new_mask)) {
    PWRN("Cannot set consistent DMA mask\n");
    return -EIO;
  }

  PLOG("DMA mask set to: 0x%llx \n",new_mask);
  return count;

 error:
  PERR("dma_mask_store failed unexpectedly.");
  return -EINVAL;
}


#define UNLOCK_DMA_AREA_LIST spin_unlock(&pkdev->dma_area_list_lock)
#define LOCK_DMA_AREA_LIST   spin_lock(&pkdev->dma_area_list_lock)


static inline struct page *__last_page(void *addr, unsigned long size)
{
  return virt_to_page(addr + (PAGE_SIZE << get_order(size)) - 1);
}

/*--------------------------------------------------------*/
/** 
 * dma_alloc [in] function.  Expects string "N M" where N is number 
 * of pages and M is NUMA node id.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * 
 * @return 
 */
static ssize_t dma_alloc_store(struct device * dev,
                               struct device_attribute *attr, 
                               const char * buf,
                               size_t count)
{
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);

  PLOG("pkdev=%p", pkdev);
  unsigned long num_pages = 0;
  int node_id = 0, order = 0;

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;
  
  if (sscanf(buf,"%lu %d",&num_pages, &node_id)!=2)
    return -EINVAL; 

  if(node_id < 0)
    node_id = numa_node_id(); // current node id
  
  /* can only allocate 2^N, calcuate N */
  order = (sizeof(unsigned long)*8) - __builtin_clzl((unsigned long)num_pages);
  if((1UL << (order-1)) == num_pages) 
    order --;

  /* check for limit of kernel, 4MB normally.  CMA may be an alternative  */
  if(order >= MAX_ORDER) {
    PLOG("dma_alloc_store requested (order=%u) more than kernel max order of (%d)",
         order,
         MAX_ORDER-1);
    return -ERANGE;
  }

  {
    // Note: we don't use the DMA-MAPPINGS API due to lack of
    // NUMA support.
    //    dma_alloc_coherent(struct device *dev, size_t size,
    //
    void * new_mem;
    int gfp;
    struct pk_dma_area * pk_area = kmalloc(sizeof(struct pk_dma_area),GFP_KERNEL);

    if (pk_area==NULL) 
      return -ENOMEM;

    gfp = GFP_KERNEL;
    if (pkdev->pci_dev->dma_mask < DMA_BIT_MASK(64)) {
      if (pkdev->pci_dev->dma_mask < DMA_BIT_MASK(32))
        gfp |= GFP_DMA;
      else
        gfp |= GFP_DMA32;
    }
           
    /* allocate NUMA-aware memory */
    //    new_mem = alloc_pages_node(node_id, gfp, order);

    dma_addr_t handle;
    struct device * devptr = &pkdev->pci_dev->dev;
    new_mem = dma_alloc_coherent(devptr,
                                 (1ULL << order) * PAGE_SIZE,
                                 &handle,
                                 gfp);

    if(new_mem == NULL) {
      PLOG("unable to alloc requested pages.");
      kfree(pk_area);
      return -ENOMEM;
    }
    /* else { */
    /*   memset(new_mem, 0xC, (1ULL << order) * PAGE_SIZE); */
    /*   PLOG("DMA memory set for testing"); */
    /* } */

    pk_area->p = new_mem;
    pk_area->node_id = node_id;
    pk_area->order = order;
    pk_area->flags = 0;
    pk_area->owner_pid = task_pid_nr(current); /* later for use with capability model */

#ifdef USE_IOMMU
#error "Don't use this!"
    /* set up DMA permissions in IO-MMU */
    pk_area->phys_addr = pci_map_page(pkdev->pci_dev,
                                      new_mem,
                                      0,/* offset */
                                      (PAGE_SIZE << order),
                                      DMA_BIDIRECTIONAL);
    
    BUG_ON(pci_dma_mapping_error(pkdev->pci_dev, pk_area->phys_addr)!=0);
#else
    pk_area->phys_addr = dma_to_phys(devptr, handle);
#endif
    

    //    BUG_ON(!page_mapped(new_mem));
    
#if 0
    /* prevent pages being swapped out */
    {
      struct page * page = new_mem;
      void * p = page_address(page);
      struct page * last;
      last = __last_page(p, num_pages * PAGE_SIZE);
      for (; page <= last; page++) {
        SetPageReserved(page);
        SetPageDirty(page);
      }
    }
#endif

    /* store new allocation */
    LOCK_DMA_AREA_LIST;
    list_add(&pk_area->list, &pkdev->dma_area_list_head);
    UNLOCK_DMA_AREA_LIST;

    /* testing purposes */
    PDBG("module allocated %lu pages at (phys=%llx) (owner=%x) (order=%d)",
         num_pages,
         virt_to_phys(new_mem),
         pk_area->owner_pid,
         pk_area->order
         );
    //    __free_pages(new_mem, num_pages);
    
  }
  

  return count; // OK

 error:
  PERR("dev_get_drvdata returned a NULL pointer.");
  return -EIO;
}

/** 
 * Show the current pages that are allocated for the calling
 * process.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * 
 * @return 
 */
static ssize_t dma_alloc_show(struct device *dev,
                              struct device_attribute *attr, 
                              char *buf)
{
  unsigned total_chars = 0;
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata((struct device *)dev);
  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;

  {
    struct list_head * p;
    struct pk_dma_area * area;
    unsigned num_chars;
    char tmp[128];
    int curr_task_pid = task_pid_nr(current);

    buf[0] = '\0';

    LOCK_DMA_AREA_LIST;
    
    list_for_each(p, &pkdev->dma_area_list_head) {

      area = list_entry(p,struct pk_dma_area, list);

      /* only show for current process */
      if(area->owner_pid != curr_task_pid)
        continue;
      
      num_chars = sprintf((char *)tmp,"0x%x %d %u 0x%llx\n",             
                          area->owner_pid,
                          area->node_id,
                          area->order,
                          area->phys_addr);             

      if ((total_chars + num_chars) > PAGE_SIZE) {
        PWRN("dma_alloc_show: too many DMA entries to iterate");
      }
      else {
        strcat(buf,tmp);
        total_chars = total_chars + num_chars;
      }
    }

    UNLOCK_DMA_AREA_LIST;
  }

  return total_chars;

 error:
  PERR("dev_get_drvdata returned a NULL pointer.");
  return -EIO;
}

#if 0
static inline void my_free_pages_check(struct page *page)
{
  if(page_mapped(page))
    printk("!!!!! PAGE NOT READY: still mapped !!!!!\n");
  
  if(page->mapping != NULL)
    printk("!!!!! PAGE NOT READY: page->mapping != NULL !!!!!\n");

  if(page_count(page) != 0)
    printk("!!!!! PAGE NOT READY: page_count(page) != 0 (but is %d)(mapcount=%d) !!!!!\n",
           page_count(page),page_mapcount(page));
  
  if (    
          (page->flags & (
                          1 << PG_lru     |
                          1 << PG_private |
                          1 << PG_locked  |
                          1 << PG_active  |
                          1 << PG_reclaim |
                          1 << PG_slab    |
                          1 << PG_swapcache |
                          1 << PG_writeback )))
    printk("!!!!! PAGE NOT READY: bad flags !!!!!\n");
    

  if (PageDirty(page))
    ClearPageDirty(page);
}
#endif

/** 
 * Used to free allocated DMA pages.  Frees all pages from the given physical address.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * @param count 
 * 
 * @return 
 */
static ssize_t dma_free_store(struct device * dev,
                              struct device_attribute *attr, 
                              const char * buf,
                              size_t count)
{
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);
  addr_t phys_addr = 0;
  bool free_all = false;

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;

  /* string is of the form "<address>" */
  if (sscanf(buf,"0x%lx",&phys_addr) != 1) {
    if (strncmp(buf,"*",1)==0) 
      free_all = true;
  }

  /* scan through list to find matching entry */
  {
    struct list_head * p, * safetmp;
    struct pk_dma_area * area;
    int curr_task_pid = task_pid_nr(current);

    LOCK_DMA_AREA_LIST;

    list_for_each_safe(p, safetmp, &pkdev->dma_area_list_head) {

      area = list_entry(p,struct pk_dma_area, list);

      if (area->owner_pid != curr_task_pid) 
        continue;

      /* remove from list and free memory */
      if (free_all || (area->phys_addr == phys_addr)) {


        PDBG("Freeing DMA page (0x%lx).",phys_addr);
#if 0
        /* clear reserved bit */
        {
          struct page * page = area->p;
          struct page * last;
          last = virt_to_page(p + (PAGE_SIZE << get_order(area->order)) - 1);
          
          for (; page <= last; page++)
            ClearPageReserved(page);
        }
#endif
        /* decrement ref count and free page */
	//        atomic_dec(&area->p->_count);

#ifdef USE_IOMMU
        /* unmap from DMA subsystem */
        pci_unmap_page(pkdev->pci_dev,
                       area->phys_addr,
                       (PAGE_SIZE << area->order),
                       DMA_BIDIRECTIONAL);
#endif

        /* free memory */
        //        __free_pages(area->p,get_order(area->order));
        dma_free_coherent(&pkdev->pci_dev->dev, 
                          (1ULL << area->order)*PAGE_SIZE,
                          area->p,
                          area->phys_addr);

        /* remove from list */
        list_del(p);
       
        /* free kernel memory for pk_dma_area */
        kfree(area);

        if (!free_all) 
          break;

      }
    }
    UNLOCK_DMA_AREA_LIST;

  }

  return count;

 error:
  PERR("dev_get_drvdata returned a NULL pointer.");
  return -EIO;
}



/** 
 * Called as READ on /pkXX/msix_alloc to show currently allocated MSI-X vectors
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * 
 * @return 
 */
static ssize_t msi_alloc_show(struct device *dev,
                              struct device_attribute *attr, 
                              char *buf)
{ 
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;

  {
    char tmp[256];
    int i;
    unsigned num_chars = 0, total_chars = 0;

    /* return one vector per line */
    buf[0] = '\0';
    
    // MSI-X
    if(pkdev->msi_support & 0x2) {

      for (i = 0;i < pkdev->msix_entry_num; i++) {

        num_chars = sprintf((char*) tmp,"%u\n",pkdev->msix_entry[i].vector);
        
        if((total_chars + num_chars) > PAGE_SIZE) {
          PWRN("total chars exceeds page size.");
          return -EIO;
        }
        
        strcat(buf,tmp);
      }
    }
    // MSI
    else if(pkdev->msi_support & 0x1) {

      for (i = 0;i < pkdev->msi_entry_num; i++) {

        num_chars = sprintf((char*) tmp,"%u\n",pkdev->pci_dev->irq+i);

        if((total_chars + num_chars) > PAGE_SIZE) {
          PWRN("total chars exceeds page size.");
          return -EIO;
        }
        
        strcat(buf,tmp);
      }
    }
    else {
      PDBG("invalid call on msi_alloc_show, neither MSI or MSI-X are supported.");
    }
  }

  return sprintf(buf,"%s",buf); 

 error:
  PERR("dev_get_drvdata returned a NULL pointer.");
  return -EIO;
}


/** 
 * Thread IRQ handler for MSI-X interrupts
 * 
 * @param irq 
 * @param dev 
 * 
 * @return 
 */
static irqreturn_t msi_threaded_irq_handler(int irq, void * cookie)
{
  struct interrupt_wait_queue_t * wq = (struct interrupt_wait_queue_t *) cookie;
  BUG_ON(wq==NULL);
  BUG_ON(wq->pkd==NULL);

  /* check IRQ mode */
  if(wq->pkd->irq_mode == IRQ_MODE_MASKING) {
    disable_irq(wq->irq);
  }

  wq->signalled = 1;
  wake_up_interruptible_all(&wq->q);  

  return IRQ_HANDLED;
}


/** 
 * Primary IRQ handler for MSI-X interrupts
 * 
 * @param irq 
 * @param dev 
 * 
 * @return 
 */
static irqreturn_t msi_primary_irq_handler(int irq, void * dev)
{
  /* TODO: we are supposed to see if the device has 
     interrupted but this is hard without doing something
     driver specific which is in userland anyhow.
     Return IRQ_NONE if this is not ours.  For the moment
     we disable IRQF_SHARED 
  */  

  return IRQ_WAKE_THREAD;
}


/** 
 * Called as a read on /proc/parasite/pkNNN/XXX to wait for interrupt.
 * 
 * @param fp 
 * @param u 
 * @param size 
 * @param offset 
 * 
 * @return 
 */
ssize_t msi_proc_read(struct file * fp, char __user * u, size_t size, loff_t * offset)
{
  int rc;
  char tmp[16];
  
  {
    struct interrupt_wait_queue_t * iwq;
    /* different kernel version have different macros - not sure what exact version! */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,6,0)
    iwq = (struct interrupt_wait_queue_t *)(long)PDE_DATA(fp->f_path.dentry->d_inode);    
#else
    iwq = (struct interrupt_wait_queue_t *)(long)PDE(fp->f_path.dentry->d_inode)->data;
#endif
    
    BUG_ON(iwq == NULL);
    BUG_ON(iwq->pkd == NULL);

    rc = wait_event_interruptible(iwq->q, iwq->signalled != 0);

    if(rc == -ERESTARTSYS) {
      return 0;
    }

    //PDBG("unblocked from wait_event_interruptible (Q=%p)",(void*) &iwq->q);
    rc = snprintf(tmp,16,"%d\n",iwq->irq);

    iwq->signalled = 0; /* clear signal */
  }
  
  if(rc > 0) {
    int c = copy_to_user(u,(void*)tmp,rc);
    ASSERT(c == 0);
      
    return rc;
  }
  else {
    return 0;
  }
}

/** 
 * Called as a write on /proc/parasite/pkNNN/XXX to wait for interrupt.  Use to
 * clear IRQ masking for the interrupt
 * 
 * @param file 
 * @param buf 
 * @param len 
 * @param ppos 
 * 
 * @return 
 */
ssize_t msi_proc_write(struct file *fp, 
                       const char __user *buf,
                       size_t len, loff_t *ppos)
{
  {
    struct interrupt_wait_queue_t * iwq;
    /* different kernel version have different macros - not sure what exact version! */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,6,0)
    iwq = (struct interrupt_wait_queue_t *)(long)PDE_DATA(fp->f_path.dentry->d_inode);    
#else
    iwq = (struct interrupt_wait_queue_t *)(long)PDE(fp->f_path.dentry->d_inode)->data;
#endif
    
    BUG_ON(iwq == NULL);
    BUG_ON(iwq->pkd == NULL);

    /* by default clear interrupt mask */
    if(iwq->pkd->irq_mode == IRQ_MODE_MASKING) {
      enable_irq(iwq->irq);
    }
  }
  return len;
}


static const struct file_operations msix_proc_fops = {
  .read	 = msi_proc_read,
	.write = msi_proc_write,
};


int setup_msi_handlers(struct pk_device * pkdev)
{
  unsigned i, base;
  int rc;

  ASSERT(pkdev);
  ASSERT(pkdev->msi_proc_dir_root != NULL);

  if(pkdev->msi_entry_num == 0) 
    return -EINVAL;

  base = pkdev->pci_dev->irq;

  /* for each MSI interrupt */
  for(i=0;i<pkdev->msi_entry_num;i++) {
    
    unsigned this_vector = base + i;

    /* initialize wait queue for this vector */
    init_waitqueue_head(&pkdev->msi_irq_wait_queue[i].q);
    pkdev->msi_irq_wait_queue[i].irq = this_vector;
    pkdev->msi_irq_wait_queue[i].pkd = pkdev;
    
    /** 
     * request MSI IRQ from the PCI subsystem
     * 
     */
    rc = request_threaded_irq(this_vector, 
                              msi_primary_irq_handler,
                              msi_threaded_irq_handler,
                              0, //IRQF_SHARED,
                              pkdev->name,
                              (void*)(&pkdev->msi_irq_wait_queue[i]));

    if (rc) {
      PDBG("request_irq failed.");
      return -EINVAL;
    }
        
    PDBG("requested threaded IRQ handler on MSI %d",this_vector);

    /* allocate /proc item */
    {
      char entry_name[16];     
        
      sprintf(entry_name,"msi-%d",this_vector);
      proc_create_data(entry_name, 
                       0666, 
                       pkdev->msi_proc_dir_root,
                       &msix_proc_fops,
                       (void*)(&pkdev->msi_irq_wait_queue[i])); // pass thru ptr to wait Q

      pkdev->msi_proc_dir_entry_num += 1;

      PLOG("creating /proc/parasite/%s/%s entry (pkdev=%p)(waitQ=%p)",
           pkdev->name, entry_name, pkdev,(void*)(long) (&pkdev->msi_irq_wait_queue[i].q));
    }

  }

  return 0;
}


/** 
 * Set up wait queue for each allocated MSI-X vector.  Attach handler to 
 * each vector.
 * 
 * @param pkdev 
 * 
 * @return 
 */
int setup_msix_handlers(struct pk_device * pkdev)
{
  unsigned i;
  int rc;

  ASSERT(pkdev);
  ASSERT(pkdev->msi_proc_dir_root != NULL);

  if(pkdev->msix_entry_num == 0) 
    return -EINVAL;

  /* for each allocated MSI-X interrupt */
  for(i=0;i<pkdev->msix_entry_num;i++) {
   
    unsigned this_vector = pkdev->msix_entry[i].vector;

    ASSERT(i <= MAX_MSI_VECTORS_PER_DEVICE);
    ASSERT(this_vector > 0);
    
    /* initialize wait queue for this vector */
    init_waitqueue_head(&pkdev->msi_irq_wait_queue[i].q);
    pkdev->msi_irq_wait_queue[i].irq = this_vector;
    pkdev->msi_irq_wait_queue[i].pkd = pkdev;

    /** 
     * request MSI-X IRQ from the PCI subsystem
     * 
     */
    rc = request_threaded_irq(this_vector,
                              msi_primary_irq_handler,
                              msi_threaded_irq_handler,
                              0, //IRQF_SHARED,
                              pkdev->name,
                              &pkdev->msi_irq_wait_queue[i]);

    PLOG("attached threaded IRQ (%u) to queue %p",
         this_vector,
         (void*) &pkdev->msi_irq_wait_queue[i]);


    if (rc) {
      PDBG("request_irq failed.");
      return -EINVAL;
    }


    /* allocate /proc item */
    {
      char entry_name[16];     
        
      sprintf(entry_name,"msix-%d",this_vector);

      /* TODO: permissions should be locked down to owner */
      proc_create_data(entry_name, 
                       0666, 
                       pkdev->msi_proc_dir_root,
                       &msix_proc_fops,
                       (void*)(&pkdev->msi_irq_wait_queue[i])); // pass thru ptr to wait Q


      pkdev->msi_proc_dir_entry_num += 1;
      
      PLOG("creating /proc/parasite/%s/%s entry (pkdev=%p)",pkdev->name, entry_name, pkdev);
    }

  }

  return 0;
}

/** 
 * Called as WRITE on /pkXX/msix_alloc with single decimal parameter which is number of vectors.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * @param count 
 * 
 * @return 
 */
static ssize_t msi_alloc_store(struct device *dev, 
                               struct device_attribute *attr,
                               const char *buf, 
                               size_t count)
{
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);
  unsigned num_vecs = 0;
  int rc;

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;  

  if(pkdev->msi_entry_num > 0) {
    return -EIO;
  }

  /* format "%d" where '%d' is number of vectors to allocate */
  if (sscanf(buf,"%u",&num_vecs)!=1) {
    PLOG("msi_alloc_store: failed parse of parameters");
    return -EINVAL; 
  }

  /* limit vectors per device */
  if (num_vecs > MAX_MSI_VECTORS_PER_DEVICE || num_vecs == 0) {
    PLOG("msix_alloc_store: too many vectors or none (num_vecs=%d)", num_vecs);
    return -EINVAL;
  }

  /*********/
  /* MSI-X */
  /*********/
  if (pkdev->msi_support == 0x2) {

    PLOG("allocating %u MSI-X vectors", num_vecs);

    /* allocate msix entry memory */
    pkdev->msix_entry = 
      (struct msix_entry *) kzalloc(sizeof(struct msix_entry) * num_vecs, 
                                    GFP_KERNEL);

    BUG_ON(pkdev->msix_entry == NULL);

    pkdev->msix_entry_num = num_vecs;

    {
      int i;
      for(i=0;i<num_vecs;i++) 
        pkdev->msix_entry[i].entry = i;
    }

    /* request MSI-X vectors from the kernel sub-system */
    rc = pci_enable_msix(pkdev->pci_dev, pkdev->msix_entry, num_vecs);

    if (rc!=0) {
      if(rc > 0) {
        PLOG("cannot allocate more than %d vectors for this device",rc);
      }
      else {
        PLOG("pci_enable_msix failed (rc=%d)",rc);
      }
      return rc;
    }
  
    /* set up MSI-X irq handlers */
    rc = setup_msix_handlers(pkdev);
    if (rc!=0)
      return rc;
  }
  /*********/
  /* MSI   */
  /*********/
  else if (pkdev->msi_support == 0x1) {
    
    PLOG("allocating %u MSI vectors", num_vecs);
    // check is power of 2 up to 5
    // TODO
    if(count > 32) 
      return -EINVAL;

    pkdev->msi_entry_num = num_vecs;

    rc = pci_enable_msi_block(pkdev->pci_dev, num_vecs);    

    if (rc!=0) {
      if (rc > 0) {
        PLOG("pci_enable_msi_block can only %u MSI vectors for this device",rc);
      }
      else {
        PERR("pci_enable_msi_block failed. err=%d",rc);
      }
      return rc;
    }

    rc = setup_msi_handlers(pkdev);
    ASSERT(rc==0);
  }
  else {
    PWRN("device does not support MSI or MSI-X");
    return -EINVAL;
  }
  
  return count; // OK

 error:
  PERR("dev_get_drvdata returned a NULL pointer.");
  return -EIO;
}

static ssize_t msi_cap_show(struct device * dev, 
                            struct device_attribute * attr, 
                            char *buf)
{
  int support = 0;
  struct pci_dev * pcidev;
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);

  if(!pkdev) 
    goto error;

  pcidev = pkdev->pci_dev;
  if(!pcidev || !pcidev->bus) 
    goto error;  

  /* check MSI/MSI-X support */
  if (pci_find_capability(pkdev->pci_dev,PCI_CAP_ID_MSI)) 
    support = 0x1;

  if (pci_find_capability(pkdev->pci_dev,PCI_CAP_ID_MSIX))
    support |= (1 << 2);

  return sprintf(buf, "%d", support);

 error:
  return sprintf(buf, "error: %s\n",__FUNCTION__);
}

static ssize_t irq_mode_show(struct device *dev,
                               struct device_attribute *attr, 
                               char *buf)
{
  unsigned mode;
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;

  /* format "%d" where '%d' is number of vectors to allocate */
  if (sscanf(buf,"%u",&mode)!=1) {
    PLOG("irq_mode_show: failed parse of parameters");
    return -EINVAL; 
  }
  pkdev->irq_mode = mode;
  BUG_ON(mode > 2 || mode == 0);
 
  return sprintf(buf,"%s","to do..");
 error:
  return -EIO;
}

/** 
 * Set the irq mode (1=NONMASKING, 2=MASKING)
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * @param count 
 * 
 * @return 
 */
static ssize_t irq_mode_store(struct device *dev, 
                              struct device_attribute *attr,
                              const char *buf, 
                              size_t count)
{
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);
  unsigned mode, i;

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;  

  /* format "%d" where '%d' is number of vectors to allocate */
  if (sscanf(buf,"%u",&mode)!=1) {
    PLOG("irq_mode_store: failed parse of parameters");
    return -EINVAL; 
  }
  if(mode == 1) {
    pkdev->irq_mode = IRQ_MODE_NONMASKING;
    PLOG("set non-masking mode for IRQ handling");
  }
  else if(mode == 2) {
    pkdev->irq_mode = IRQ_MODE_MASKING;
    PLOG("set masking mode for IRQ handling");
  }
  else {
    PERR("attempt to set unknown IRQ mode (%u)",mode);
  }

  /* reset masking state */
  for(i=0;i<pkdev->msix_entry_num;i++) {
    BUG_ON(pkdev->msix_entry[i].vector == 0);
    if(mode == 1) 
      enable_irq(pkdev->msix_entry[i].vector);
    else
      disable_irq(pkdev->msix_entry[i].vector);
  }

  return count;
 error:
  return -EIO;
}

void free_dma_memory(struct pk_device * pkdev)
{
  struct list_head * p, * safetmp;
  struct pk_dma_area * area;
  int curr_task_pid = task_pid_nr(current);
  
  LOCK_DMA_AREA_LIST;
  
  list_for_each_safe(p, safetmp, &pkdev->dma_area_list_head) {
    
    area = list_entry(p,struct pk_dma_area, list);

    /* TODO security issue */
    /* if (area->owner_pid != curr_task_pid)  */
    /*   continue; */

    PLOG("freeing %d pages at (%llx)",
         1 << area->order,
         area->phys_addr);

    /* decrement ref count and free page */
    //TOFIX    atomic_dec(&area->p->_count);
    __free_pages(area->p,get_order(area->order));
    
    /* remove from list */
    list_del(p);
       
    /* free kernel memory for pk_dma_area */
    kfree(area);
    
  }
  UNLOCK_DMA_AREA_LIST;
}


/** 
 * Write to /grant_access_store used to grant access to allocated memory.
 * 
 * @param dev 
 * @param attr 
 * @param buf 
 * @param count 
 * 
 * @return 
 */
static ssize_t grant_access_store(struct device * dev,
                                  struct device_attribute *attr, 
                                  const char * buf,
                                  size_t count)
{
  struct pk_device * pkdev = (struct pk_device *) dev_get_drvdata(dev);
  struct pk_dma_area * area;
  addr_t phys_addr = 0;

  PDBG("grant_access_store called.");

  if(!pkdev) goto error;
  if(!pkdev->pci_dev) goto error;

  /* string is of the form "<address>" */
  if (sscanf(buf,"0x%lx",&phys_addr) != 1) {
    PWRN("PK's grant_access_store could not parse input params.");
    return -EINVAL;
  }

  /* check that the calling process owns this allocation */
  area = get_owned_dma_area(phys_addr);

  if(area==NULL) {
    PWRN("area not owned by calling process.");
    return -EINVAL;
  }

  area->flags |= DMA_AREA_FLAG_SHARED_ALL;
  PDBG("granted shared-all access to 0x%lx", phys_addr);

  return count;

 error:
  PERR("dev_get_drvdata returned a NULL pointer.");
  return -EIO;

}



/** 
 * Device attribute declaration
 * 
 */
DEVICE_ATTR(name, S_IRUGO, show_name, NULL);
DEVICE_ATTR(version, S_IRUGO, show_version, NULL);
DEVICE_ATTR(pci, S_IRUGO, show_pci, NULL);
DEVICE_ATTR(irq, S_IRUGO, wait_irq, NULL);
DEVICE_ATTR(irq_mode, S_IRUGO | S_IWUGO, irq_mode_show, irq_mode_store);
DEVICE_ATTR(dma_mask, S_IRUGO | S_IWUGO, dma_mask_show, dma_mask_store);
DEVICE_ATTR(dma_page_alloc, S_IRUGO | S_IWUGO, dma_alloc_show, dma_alloc_store);
DEVICE_ATTR(dma_page_free, S_IWUGO, NULL, dma_free_store);
DEVICE_ATTR(msi_alloc, S_IRUGO | S_IWUGO, msi_alloc_show, msi_alloc_store);
DEVICE_ATTR(msi_cap, S_IRUGO, msi_cap_show, NULL);
DEVICE_ATTR(grant_access, S_IWUGO, NULL, grant_access_store);




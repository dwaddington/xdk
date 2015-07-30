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

/*
 echo "8086 10f5" > /sys/class/parasite/new_id
*/

#include "pk.h"

static int probe(struct pci_dev *, const struct pci_device_id *);
void remove(struct pci_dev *);
int verify_pci_version(struct pci_dev *pdev);

struct pci_driver pci_driver = {
  .name = "parasite",
  .id_table = NULL,
  .probe = probe,
  .remove = remove,
};

void pci_init(void)
{
  if(pci_register_driver(&pci_driver) < 0) {
    PERR("pci_register_init failed.");
  }
}

void pci_cleanup(void)
{
  pci_unregister_driver(&pci_driver);
}

/** 
 * Basic PCI interrupt handler routine
 * 
 * @param irq IRQ number that has fired
 * @param dev_id Cast pk_device pointer
 * 
 * @return 
 */
static irqreturn_t interrupt_handler(int irq, 
                                     void *dev_id)
{
  struct pk_device * pkdev = (struct pk_device *) dev_id;
  struct pci_dev * pci_dev = pkdev->pci_dev;
  u16 reg;

  PLOG("PK interrupt!!");
  pci_bus_read_config_word(pci_dev->bus,
                           pci_dev->devfn,
                           0x4, // status reg
                           &reg);

  
  /* check this is our interrupt (in case of shared) */
  if(pkdev->magic == 0xfeeb1e00) {
    //    wake_up_interruptible(&pkdev->irq_wait);
    PLOG("PK interrupt handler entered PCI status = %x",reg);
    pkdev->irq_wait_flag = 0;

    //    pci_disable_device(pkdev->pci_dev); // this clear the PCI status bit 3.
  }
  else {
    return IRQ_NONE;
  }

  return IRQ_HANDLED;
}


/** 
 * Used to set up conventional PCI interrupts
 * 
 * @param dev 
 * @param pkd 
 */
void setup_basic_interrupt(struct pci_dev *dev, struct pk_device * pkd)
{
  int rc;

  rc = request_irq(dev->irq,
                   interrupt_handler,
                   IRQF_SHARED, /* slow interrupt */
                   pkd->name, /* name appears in /proc/interrupts */
                   (void*)(pkd));
}

/** 
 * Used to set up conventional PCI interrupts
 * 
 * @param dev 
 * @param pkd 
 */
void setup_msi_interrupt(struct pci_dev *dev, struct pk_device * pkd)
{
  int rc;

  rc = pci_enable_msi(dev);
  ASSERT(rc == 0);

  rc = request_irq(dev->irq,
                   interrupt_handler,
                   IRQF_SHARED, /* slow interrupt */
                   pkd->name, /* name appears in /proc/interrupts */
                   (void*)(pkd));
}



/** 
 * Main probe routine called by the PCI subsystem when a request is made for a
 * new device to be registered
 * 
 * @param dev 
 * @param id 
 * 
 * @return 
 */
static int probe(struct pci_dev *dev, 
                 const struct pci_device_id *id)
{
  int msi = 0;
  int err = 0;

  PLOG("pci:probe");
  PLOG("pci: bus num=%u prim=%u",dev->bus->number, dev->bus->primary);
  PLOG("pci: vendor=%x device=%x",dev->vendor, dev->device);
  PLOG("pci: slot=%u", PCI_SLOT(dev->devfn));
  if(verify_pci_version(dev)==0) {
    PLOG("pci:probe device supports PCI 2.3");
  }
  else {
    PLOG("pci:probe device does NOT support PCI 2.3");
    //    return -1;
  }

  /* enable PCI for device */
  err = pci_enable_device(dev);
  if(err) {
    dev_err(&dev->dev, "%s: pci_enable_device failed: %d\n", __func__, err);
    return err;
  }

  /* enable DMA bus mastering */
  pci_set_master(dev);
  err = pci_enable_device_mem(dev);
  ASSERT(err == 0);

  /* check MSI support */
  if (pci_find_capability(dev,PCI_CAP_ID_MSI)) {
    msi=1;
    pci_disable_msi(dev);
    PLOG("detected MSI support");
  }
  
  /* check MSI-X support */
  if (pci_find_capability(dev,PCI_CAP_ID_MSIX)) {
    msi=2;
    pci_disable_msix(dev);

    /* for MSI-X we assume 64bit DMA. */
    if(pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(64))) {
      PLOG("unable to set consistent DMA mask to 64 bits");
      return -1;
    }
    if(pci_set_dma_mask(dev,DMA_BIT_MASK(64))) {
      PLOG("unable to set DMA mask to 64 bits");
      return -1;
    }
    PLOG("DMA masks set OK (dev=%p)",dev);
    PLOG("Detected MSIX support");
  }
  
  if(msi==0) {
    PLOG("no MSI support");
  }
  
  //  unsigned               memory_mapped_io_region_count;
  //  struct memory_region * memory_mapped_io_regions[MAX_MEMORY_MAPPED_IO_REGIONS];

  /* read memory mapped IO resources and request regions. this ensures 
     no other device is try to use the same region
  */
  {
    int i;
    for (i = 0; i <= PCI_STD_RESOURCE_END; i++) {
      unsigned flags = pci_resource_flags(dev, i);
      if ((flags & IORESOURCE_MEM) || (flags & IORESOURCE_IO)) {
        PLOG("found PCI resource (%lx) @ %lx",pci_resource_flags(dev, i), (long) pci_resource_start(dev, i));
      }      
    }
  }

  if (pci_request_regions(dev,"pk-device"))
    panic("resource conflict device across memory/io regions");

  //  unsigned long pci_resource_start(struct pci_dev *dev, int bar);

  BUG_ON(dev==NULL);
  PLOG("Registering device with sys/class");
  {
    struct pk_device * pkd = sysfs_class_register_device(dev);
    pkd->msi_support = msi;

    switch(msi) {
    case 1: 
      // MSI we don't set up implicitly
      break;
    case 2:
      // MSI-X we don't set up implicitly
      break;    
    default: 
      PLOG("setting up basic interrupt for device.");
      setup_basic_interrupt(dev,pkd);
      break;
    }
  }




  return 0;
}


/* Verify that the device supports Interrupt Disable bit in command register,
 * per PCI 2.3, by flipping this bit and reading it back: this bit was readonly
 * in PCI 2.2. 
 */
int verify_pci_version(struct pci_dev *pdev)
{
	u16 orig, new;
	int err = 0;

	pci_read_config_word(pdev, PCI_COMMAND, &orig);
	pci_write_config_word(pdev, PCI_COMMAND, orig ^ PCI_COMMAND_INTX_DISABLE);
	pci_read_config_word(pdev, PCI_COMMAND, &new);

	if ((new ^ orig) & ~PCI_COMMAND_INTX_DISABLE) {
		err = -EBUSY;
		dev_err(&pdev->dev, "Command changed from 0x%x to 0x%x: HW bug?\n", orig, new);
		goto err;
	}
	if (!((new ^ orig) & PCI_COMMAND_INTX_DISABLE)) {
		dev_warn(&pdev->dev, "Device does not support interrupt disabling: unable to bind.\n");
		err = -ENODEV;
		goto err;
	}
	/* Now restore the original value. */
	pci_write_config_word(pdev, PCI_COMMAND, orig);
err:
	return err;
}

extern void release_all_devices(void);

void remove(struct pci_dev * dev)
{
  PLOG("pci:remove");
}

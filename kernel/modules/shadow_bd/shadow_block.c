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






#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/circ_buf.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

#include "debug.h"
#include "sbd.h"

#define SHADOW_FIRST_MINOR 0
#define SHADOW_MINOR_CNT 16

static u_int shadow_major = 0;


/* forward decls */
//static int fops_misc_mmap(struct file *file, struct vm_area_struct *vma);
static ssize_t fops_misc_read(struct file * fp, 
                              char __user * u, 
                              size_t size, 
                              loff_t * offset);

static ssize_t fops_misc_write(struct file *, 
                              const char __user *, 
                              size_t, 
                              loff_t *);

static int fops_misc_open(struct inode *, struct file *);
static int fops_misc_release(struct inode *, struct file *);
static loff_t fops_misc_llseek (struct file * f, loff_t off, int p);
static void init_user_kernel_memory(void);
static void cleanup_user_kernel_memory(void);
static void init_block_device(void);

const struct file_operations shadow_dev_fops = {
  .owner          = THIS_MODULE,
  .read           = fops_misc_read,
  .write          = fops_misc_write,
  .release        = fops_misc_release,
  .open           = fops_misc_open, 
  .llseek         = fops_misc_llseek,
  //  .release        = fops_release,
  //  .unlocked_ioctl = fop_ioctl,
  //  .mmap	          = fops_misc_mmap,
};

enum {
  PROTOCOL_CMD_INIT = 1,
  PROTOCOL_CMD_ACK = 2,
  PROTOCOL_CMD_READ = 3,
  PROTOCOL_CMD_WRITE = 4,
};

typedef struct {
  u32 command;
  union {
    /* CMD_INIT */
    sector_t capacity;
    /* CMD_READ/WRITE */
    struct {
      sector_t offset;
      size_t sectors;
      unsigned long phys_addr;
    } rw;
  };
} shadow_protocol_t;

shadow_protocol_t pending_command;

/** 
 * We register a misc device for user-level control
 * 
 */
static struct miscdevice shadow_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "shadow_ctl",
	.fops = &shadow_dev_fops,
};

/* 
 * The internal structure representation of our Device
 */
static struct shadow_device
{
	/* Size is the size of the device (in sectors) */
	unsigned int size;
	/* For exclusive access to our request queue */
	spinlock_t lock;
	/* Our request queue */
	struct request_queue * shadow_queue;

	/* This is kernel's representation of an individual disk device */
	struct gendisk *       shadow_disk;
  struct page *          user_kernel_queue_pages;

  /* /sys device */
  struct device	*        dev;

  /* user-kernel coordination */
	wait_queue_head_t request_queue;
  int               request_queue_flag;

	wait_queue_head_t ack_queue;
  int               ack_queue_flag;

  struct circleq *  request_circq;
} shadow_dev;



static void hexdump(void * data, unsigned len) 
{
  unsigned i;
  uint8_t * d;

  printk("HEXDUMP----------------------------------------------\n");

  d = (uint8_t *)data;
  for(i=0;i<len;i++) {      
    if(i % 24 == 0) { printk("\n0x%x:\t",i); }
    printk("%x%x ",0xf & (d[i] >> 4), 0xf & d[i]);
  }
  printk("\n");
}


/* static int fops_misc_mmap(struct file *file, struct vm_area_struct *vma) */
/* { */
/*   PDBG("shadow_bd fops mmap"); */
/*   return 0; */
/* } */

ssize_t fops_misc_read(struct file * fp, 
                      char __user * u, 
                      size_t size, 
                      loff_t * offset)
{
  PLOG("CONTROL: !!!!!!!!!read /dev/shadow_ctl");

  wait_event_interruptible(shadow_dev.request_queue,
                           shadow_dev.request_queue_flag != 0);

  shadow_dev.request_queue_flag = 0;

  /* TODO fix to use circular buffer */
  copy_to_user(u,&pending_command,sizeof(shadow_protocol_t));

  return sizeof(shadow_protocol_t);
}

static ssize_t fops_misc_write(struct file * fp, 
                               const char __user * u, 
                               size_t size, 
                               loff_t * offset)
{
  int rc = 0;
  shadow_protocol_t * msg;

  PLOG("CONTROL: write /dev/shadow_ctl");
  msg = (shadow_protocol_t *) u;
  

  switch(msg->command) {
  case PROTOCOL_CMD_INIT:
    shadow_dev.size = msg->capacity;
    init_block_device();
    break;
  case PROTOCOL_CMD_ACK:
    PLOG("fops_misc_write received ACK");
    shadow_dev.ack_queue_flag = 1;
    wake_up_interruptible_all(&shadow_dev.ack_queue);

    break;
  default:
    PLOG("fops_misc_write unrecognized command (%d)",msg->command);
  }

  return size;
}


static int fops_misc_open(struct inode * node, struct file * f)
{
  PLOG("open /dev/shadow_ctl");
  return 0;
}
static int fops_misc_release(struct inode * node, struct file * f)
{
  PLOG("close /dev/shadow_ctl");
  return 0;
}

static loff_t fops_misc_llseek (struct file * f, loff_t off, int p)
{
  return -ESPIPE;
}



static int shadow_open(struct block_device *bdev, fmode_t mode)
{
	unsigned unit = iminor(bdev->bd_inode);

	PLOG("sbd: Device is opened");
	PLOG("sbd: Inode number is %d", unit);

	if (unit > SHADOW_MINOR_CNT)
		return -ENODEV;
	return 0;
}

static int shadow_release(struct gendisk * disk, fmode_t mode)
{
	PLOG("sbd: Device is closed");
  return 0;
}

/* 
 * Actual Data perform_request
 */
static int shadow_perform_request(struct request *req)
{
  PLOG("request on shadow device");
	//struct shadow_device *dev = (struct shadow_device *)(req->rq_disk->private_data);

	int dir = rq_data_dir(req);
	sector_t start_sector = blk_rq_pos(req);
	unsigned int sector_cnt = blk_rq_sectors(req);

	struct bio_vec *bv;
	struct req_iterator iter;

	sector_t sector_offset;
	unsigned int sectors;
	u8 *buffer;

	int ret = 0;

	//printk(KERN_DEBUG "sbd: Dir:%d; Sec:%lld; Cnt:%d\n", dir, start_sector, sector_cnt);

	sector_offset = 0;
	rq_for_each_segment(bv, req, iter)
    {
      buffer = page_address(bv->bv_page) + bv->bv_offset;
      if (bv->bv_len % SBD_SECTOR_SIZE != 0)
        {
          PLOG("sbd: Should never happen: "
               "bio size (%d) is not a multiple of SBD_SECTOR_SIZE (%d).\n"
               "This may lead to data truncation.",
               bv->bv_len, SBD_SECTOR_SIZE);
          ret = -EIO;
        }
      sectors = bv->bv_len / SBD_SECTOR_SIZE;
      PLOG("sbd: Sector Offset: %ld; Buffer: %p; Length: %d sectors",
           sector_offset, buffer, sectors);
      if (dir == WRITE) /* Write to the device */
        {
          /* set up command. wake any waiters */
          pending_command.command = PROTOCOL_CMD_WRITE;
          while(list_empty(&shadow_dev.request_queue.task_list)) {
            msleep(1000);
          }
          shadow_dev.request_queue_flag = 1;
          wake_up_interruptible_all(&shadow_dev.request_queue);

          PLOG("write cmd: waiting for user-space ack");
          /* wait for acknowledgement */
          shadow_dev.ack_queue_flag = 1;
          wait_event_interruptible(shadow_dev.ack_queue,
                                   shadow_dev.ack_queue_flag != 0);
          PLOG("write cmd:got user-space ack");

        }
      else /* Read from the device */
        {
          ///          sbd_read(start_sector + sector_offset, buffer, sectors);
          pending_command.command = PROTOCOL_CMD_READ;
          pending_command.rw.offset = start_sector + sector_offset;
          pending_command.rw.sectors = sectors;
          pending_command.rw.phys_addr = virt_to_phys(buffer);

          PLOG("sent READ command to userspace");
          while(list_empty(&shadow_dev.request_queue.task_list)) {
            msleep(1);
          }

          shadow_dev.request_queue_flag = 1;
          wake_up_interruptible_all(&shadow_dev.request_queue);

          /* wait for ack */
          wait_event_interruptible(shadow_dev.ack_queue,
                                   shadow_dev.ack_queue_flag != 0);
          PLOG("got ACK reply for READ command");

          hexdump(buffer, 256);
        }
      sector_offset += sectors;
    }
	if (sector_offset != sector_cnt)
    {
      PLOG("sbd: bio info doesn't match with the request info");
      ret = -EIO;
    }

	return ret;
}
	
/*
 * Represents a block I/O request for us to execute
 */
static void shadow_request(struct request_queue *q)
{
	struct request *req;
	int ret;

	/* Gets the current request from the dispatch queue */
	while ((req = blk_fetch_request(q)) != NULL)
	{
#if 0
		/*
		 * This function tells us whether we are looking at a filesystem request
		 * - one that moves block of data
		 */
		if (!blk_fs_request(req))
		{
			PLOG("sbd: Skip non-fs request");
			/* We pass 0 to indicate that we successfully completed the request */
			__blk_end_request_all(req, 0);
			//__blk_end_request(req, 0, blk_rq_bytes(req));
			continue;
		}
#endif
		ret = shadow_perform_request(req);
		__blk_end_request_all(req, ret);
		//__blk_end_request(req, ret, blk_rq_bytes(req));
	}
}

/* 
 * These are the file operations that performed on the block device
 */
static const struct block_device_operations sbd_fops =
{
	.owner   = THIS_MODULE,
	.open    = shadow_open,
  .release = shadow_release,
};
	

static void init_user_kernel_memory(void)
{
  int gfp = GFP_KERNEL;
  shadow_dev.user_kernel_queue_pages = alloc_pages(gfp, 1);
}

static void cleanup_user_kernel_memory(void)
{
}

static void init_block_device()
{
  int ret;

  PLOG("init_block_device (capacity=%u)",shadow_dev.size);

	shadow_major = register_blkdev(shadow_major, "shadow0");
	if (shadow_major <= 0)  {
    PLOG("sbd: Unable to get Major Number");
    return ;
  }

  PLOG("created device /dev/shadow0");

	spin_lock_init(&shadow_dev.lock);
	shadow_dev.shadow_queue = blk_init_queue(shadow_request, &shadow_dev.lock);

	if (shadow_dev.shadow_queue == NULL)  {
    PLOG("sbd: blk_init_queue failure\n");
    unregister_blkdev(shadow_major, "sbd");
    //    sbd_cleanup();
    return ; //-ENOMEM;
  }
	
	/*
	 * Add the gendisk structure
	 * By using this memory allocation is involved, 
	 * the minor number we need to pass bcz the device 
	 * will support this much partitions 
	 */
	shadow_dev.shadow_disk = alloc_disk(SHADOW_MINOR_CNT);

	if (!shadow_dev.shadow_disk) {
    PLOG("sbd: alloc_disk failure");
    blk_cleanup_queue(shadow_dev.shadow_queue);
    unregister_blkdev(shadow_major, "sbd");
    //    sbd_cleanup();
    return ; //-ENOMEM;
  }

 	/* Setting the major number */
	shadow_dev.shadow_disk->major = shadow_major;
  /* Setting the first mior number */
	shadow_dev.shadow_disk->first_minor = SHADOW_FIRST_MINOR;
 	/* Initializing the device operations */
	shadow_dev.shadow_disk->fops = &sbd_fops;
 	/* Driver-specific own internal data */
	shadow_dev.shadow_disk->private_data = &shadow_dev;
	shadow_dev.shadow_disk->queue = shadow_dev.shadow_queue;
	/*
	 * You do not want partition information to show up in 
	 * cat /proc/partitions set this flags
	 */
	//shadow_dev.shadow_disk->flags = GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(shadow_dev.shadow_disk->disk_name, "sbd");

	/* Setting the capacity of the device in its gendisk structure */
	set_capacity(shadow_dev.shadow_disk, shadow_dev.size);

	/* Adding the disk to the system */
	add_disk(shadow_dev.shadow_disk);

	/* Now the disk is "live" */
	PLOG("block driver initialised (%d sectors; %d bytes)\n",
       shadow_dev.size, shadow_dev.size * SBD_SECTOR_SIZE);
}

/* 
 * This is the registration and initialization section of the ram block device
 * driver
 */
static int __init shadow_init(void)
{
  /* set up user-kernel queues */
  init_user_kernel_memory();

  /* initialize request queue */
  init_waitqueue_head(&shadow_dev.request_queue);
  init_waitqueue_head(&shadow_dev.ack_queue);
  //  CIRC_INIT(&shadow_dev.request_circq);

  /* set up our misc device */
  if (misc_register(&shadow_miscdev))
    panic("misc_register failed unexpectedly.");

  shadow_dev.size = 0;

  return 0;
}

/*
 * This is the unregistration and uninitialization section of the ram block
 * device driver
 */
static void __exit shadow_cleanup(void)
{
  if(shadow_dev.size > 0) {
    del_gendisk(shadow_dev.shadow_disk);
    put_disk(shadow_dev.shadow_disk);
    blk_cleanup_queue(shadow_dev.shadow_queue);
    unregister_blkdev(shadow_major, "sbd");
  }

  misc_deregister(&shadow_miscdev);
  cleanup_user_kernel_memory();
}

module_init(shadow_init);
module_exit(shadow_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Waddington <d.waddington@samsung.com>");
MODULE_DESCRIPTION("Shadow Block Device Driver");
MODULE_ALIAS_BLOCKDEV_MAJOR(shadow_major);

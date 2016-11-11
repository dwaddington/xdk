#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/cred.h>
#include "pk.h"

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
ssize_t pci_config_proc_read(struct file * fp, 
                             char __user * u, 
                             size_t size, 
                             loff_t * offset)
{
  BUG_ON(offset == NULL);
  BUG_ON(fp ==NULL);
  
  struct pk_device * pkdev;
  struct pci_dev * pci_dev;
  int rc;

  pkdev = PDE_DATA(file_inode(fp));
  BUG_ON(pkdev == NULL);

  pci_dev = pkdev->pci_dev;
  BUG_ON(pci_dev == NULL);

  /* check device permissions */
  if(!check_authority(pci_dev))
    return -EINVAL;

  switch(size) {
  case 4:
    rc = pci_read_config_dword(pci_dev, *offset, (u32 *) u);    
    break;
  case 2:
    rc = pci_read_config_word(pci_dev, *offset, (u16 *) u);    
    break;
  case 1:
    rc = pci_read_config_byte(pci_dev, *offset, (u8 *) u);    
    break;
  default:
    return 0;
  }

  if(rc != 0) 
    return rc;

  return size;
}

ssize_t pci_config_proc_write(struct file *fp, 
                              const char __user *buf,
                              size_t size, 
                              loff_t *offset)
{
  struct pk_device * pkdev;
  struct pci_dev * pci_dev;
  int rc;

  pkdev = PDE_DATA(file_inode(fp));
  BUG_ON(pkdev == NULL);

  pci_dev = pkdev->pci_dev;
  BUG_ON(pci_dev == NULL);

  /* check device permissions */
  if(!check_authority(pci_dev))
    return -EINVAL;

  switch(size) {
  case 4:
    rc = pci_write_config_dword(pci_dev, *offset, *((u32 *) buf));    
    break;
  case 2:
    rc = pci_write_config_word(pci_dev, *offset, *((u16 *) buf));    
    break;
  case 1:
    rc = pci_write_config_byte(pci_dev, *offset, *((u8 *) buf));    
    break;
  default:
    return 0;
  }

  if(rc != 0) 
    return rc;

  return size;
}

static const struct file_operations pci_config_fops = {
  .read	 = pci_config_proc_read,
	.write = pci_config_proc_write,
};


void setup_pci_config_procfs(struct pk_device * pkdev)
{
  BUG_ON(pkdev == NULL);
  PLOG("setting up /proc entry with %p",pkdev);
  proc_create_data("pci_config",
                   0666, // owned by root
                   pkdev->msi_proc_dir_root,
                   &pci_config_fops,
                   (void*)pkdev); /* pass through pkdev */
}

void cleanup_pci_config_procfs(struct pk_device * pkdev)
{
  BUG_ON(pkdev == NULL);
  remove_proc_entry("pci_config", pkdev->msi_proc_dir_root);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include <linux/proc_fs.h>
#include <linux/types.h>

#include "pk.h"

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
ssize_t pci_config_proc_read(struct file * fp, 
                             char __user * u, size_t size, loff_t * offset)
{
  PLOG("pci_config_proc_read!!!!");
  return 0;
}

ssize_t pci_config_proc_write(struct file *fp, 
                              const char __user *buf,
                              size_t len, loff_t *ppos)
{
  PLOG("pci_config_proc_write!!!!");
  return len;
}

static const struct file_operations pci_config_fops = {
  .read	 = pci_config_proc_read,
	.write = pci_config_proc_write,
};


void setup_pci_config_procfs(struct pk_device * pkdev)
{
  BUG_ON(pkdev == NULL);
  proc_create_data("pci_config",
                   0666, 
                   pkdev->msi_proc_dir_root,
                   &pci_config_fops,
                   (void*)NULL);
}

void cleanup_pci_config_procfs(struct pk_device * pkdev)
{
  BUG_ON(pkdev == NULL);
  remove_proc_entry("pci_config", pkdev->msi_proc_dir_root);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

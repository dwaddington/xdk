#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>

ssize_t pk_grant_device_store(struct class *class,
                              struct class_attribute *attr, 
                              const char * buf,
                              size_t count);

ssize_t pk_grant_device_show(struct class *class,
                             struct class_attribute *attr, char *buf);


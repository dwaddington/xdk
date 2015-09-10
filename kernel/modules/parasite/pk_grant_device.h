#ifndef __PK_GRANT_DEVICE_H__
#define __PK_GRANT_DEVICE_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>

/** 
 * Write handler for /sys/class/parasite/grant
 * 
 * @param class Class structure
 * @param attr Attributes
 * @param buf User content of the form <domain:hex> <bus:hex> <fn:hex> <uid:int> <op:0 or 1>
 * @param count Size of buffer
 * 
 * @return Number of bytes received
 */
ssize_t pk_grant_device_store(struct class *class,
                              struct class_attribute *attr, 
                              const char * buf,
                              size_t count);

/** 
 * Read handler for /sys/class/parasite/grant
 * 
 * @param class Class structure
 * @param attr Attributes
 * @param buf Buffer to return to user-space
 * 
 * @return Size of buffer returned in bytes
 */
ssize_t pk_grant_device_show(struct class *class,
                             struct class_attribute *attr, 
                             char *buf);


#endif // __PK_GRANT_DEVICE_H__

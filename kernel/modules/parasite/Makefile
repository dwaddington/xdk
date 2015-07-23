SRC = pk_main.c pk_fops.c pk_pci.c pk_class.c

KERNEL_VER = `uname -r`
BUILD = `date +%Y%m%d.%k%m`

ccflags-y := -g -Wall

obj-m += pk.o

pk-objs := pk_main.o pk_fops.o pk_pci.o pk_class.o pk_grant.o common.o

#obj-$(CONFIG_UIO_PDRV_GENIRQ)   += uio_pdrv_genirq.o


all: version.h pk.ko

pk.ko: 
	make -C /lib/modules/$(KERNEL_VER)/build M=$(PWD) modules

version.h:
	@echo "#ifndef __VERSION_H__" > version.h
	@echo "#define BUILD \"$(BUILD)\"" >> version.h
	@echo "#endif" >> version.h

clean:
	make -C /lib/modules/$(KERNEL_VER)/build M=$(PWD) clean
	@rm -f *.ko *.o

depend:
	makedepend -I./ -- -- $(SRC)

install:
	sudo insmod ./pk.ko
	sudo rmmod e1000
	sudo echo -n 0000:03:00.0 > /sys/bus/pci/drivers/ixgbe/unbind

probe:
	sudo echo "8086 100e" > /sys/bus/pci/drivers/parasite/new_id

probe10g:
	echo "8086 1528" > /sys/bus/pci/drivers/parasite/new_id

uninstall:
	sudo rmmod pk

unbind:
	sudo echo -n 0000:03:00.0 > /sys/bus/pci/drivers/ixgbe/unbind

full-tags:
#	etags `find /usr/src/linux-source-$(KERNEL_VER)/include -name '*.h'`
	etags `find /usr/src/linux-$(KERNEL_VER)/include -name '*.h'`
	etags `find . -name '*.h'` --append
	etags `find . -name '*.c'` --append


.PHONY: depend TAGS version.h

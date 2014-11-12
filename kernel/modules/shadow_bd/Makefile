SRC = pk_shadow_bd.c
#
ifeq ($(shell hostname),ash)
KERNEL_VER = 3.2.46
else
KERNEL_VER = `uname -r`
endif
#KERNEL_VER = 3.2.0
#KERNEL_VER = 3.2.0-51-generic
#KERNEL_VER = `uname -r`

obj-m += shadow_bd.o

shadow_bd-objs := shadow_block.o sbd.o partition.o

all: pk_sbd.ko

pk_sbd.ko: 
	make -C /lib/modules/$(KERNEL_VER)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KERNEL_VER)/build M=$(PWD) clean
	rm *.ko *.o

depend:
	makedepend -I./ -- -- $(SRC)

full-tags:
#	etags `find /usr/src/linux-source-$(KERNEL_VER)/include -name '*.h'`
	etags `find /usr/src/linux/include -name '*.h'`
	etags `find . -name '*.h'` --append
	etags `find . -name '*.c'` --append


.PHONY: depend TAGS

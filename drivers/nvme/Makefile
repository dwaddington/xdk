include ../../mk/global.mk

TARGET = nvme_drv
SOURCES = $(wildcard *.cc) 

CXXFLAGS += -static -O2 -I$(SILICA_BASE)/lib/libexo

LIBS = -L$(SILICA_BASE)/lib/libexo -lexo -lpthread -lrt -lnuma -ldl -ltinyxml

all: nvme_drv

nvme_drv: $(OBJS) $(SILICA_BASE)/lib/libexo/libexo.so
	g++ -g -Wall $(CXXFLAGS) -o nvme_drv $(OBJS) $(LIBS) -Wl,-rpath=$(SILICA_BASE)/lib/libexo


$(SILICA_BASE)/lib/libexo/libexo.so:
	make -C $(SILICA_BASE)/lib/libexo

clean:
	rm -Rf *.o nvme_drv obj/


.PHONY: clean
#$(SILICA_BASE)/lib/libexo/libexo.so
# DO NOT DELETE

obj/cq_thread.o: /usr/include/signal.h /usr/include/features.h
obj/cq_thread.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
obj/cq_thread.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
obj/cq_thread.o: /usr/include/gnu/stubs-64.h /usr/include/bits/sigset.h
obj/cq_thread.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
obj/cq_thread.o: /usr/include/bits/signum.h /usr/include/time.h
obj/cq_thread.o: /usr/include/bits/siginfo.h /usr/include/bits/sigaction.h
obj/cq_thread.o: /usr/include/bits/sigcontext.h /usr/include/bits/sigstack.h
obj/cq_thread.o: /usr/include/sys/ucontext.h /usr/include/bits/pthreadtypes.h
obj/cq_thread.o: /usr/include/bits/sigthread.h cq_thread.h nvme_types.h
obj/cq_thread.o: nvme_queue.h nvme_registers.h /usr/include/stdint.h
obj/cq_thread.o: /usr/include/bits/wchar.h nvme_command.h
obj/cq_thread.o: nvme_command_structs.h nvme_common.h nvme_device.h
obj/cq_thread.o: nvme_registers_wrappers.h notify.h config.h
obj/cq_thread.o: /usr/include/tinyxml.h /usr/include/ctype.h
obj/cq_thread.o: /usr/include/endian.h /usr/include/bits/endian.h
obj/cq_thread.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
obj/cq_thread.o: /usr/include/stdio.h /usr/include/libio.h
obj/cq_thread.o: /usr/include/_G_config.h /usr/include/wchar.h
obj/cq_thread.o: /usr/include/bits/stdio_lim.h
obj/cq_thread.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
obj/cq_thread.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
obj/cq_thread.o: /usr/include/sys/types.h /usr/include/sys/select.h
obj/cq_thread.o: /usr/include/bits/select.h /usr/include/bits/time.h
obj/cq_thread.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
obj/cq_thread.o: /usr/include/string.h /usr/include/assert.h
obj/main.o: /usr/include/stdio.h /usr/include/features.h
obj/main.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
obj/main.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
obj/main.o: /usr/include/gnu/stubs-64.h /usr/include/bits/types.h
obj/main.o: /usr/include/bits/typesizes.h /usr/include/libio.h
obj/main.o: /usr/include/_G_config.h /usr/include/wchar.h
obj/main.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
obj/main.o: /usr/include/sys/time.h /usr/include/time.h
obj/main.o: /usr/include/bits/time.h /usr/include/sys/select.h
obj/main.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
obj/main.o: /usr/include/sched.h /usr/include/bits/sched.h nvme_device.h
obj/main.o: nvme_common.h nvme_registers.h /usr/include/stdint.h
obj/main.o: /usr/include/bits/wchar.h nvme_registers_wrappers.h nvme_queue.h
obj/main.o: nvme_command.h nvme_command_structs.h nvme_types.h cq_thread.h
obj/main.o: notify.h config.h /usr/include/tinyxml.h /usr/include/ctype.h
obj/main.o: /usr/include/endian.h /usr/include/bits/endian.h
obj/main.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
obj/main.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
obj/main.o: /usr/include/bits/waitstatus.h /usr/include/sys/types.h
obj/main.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
obj/main.o: /usr/include/alloca.h /usr/include/string.h /usr/include/assert.h
obj/nvme_command_admin.o: nvme_command_admin.h nvme_command_structs.h
obj/nvme_command_admin.o: /usr/include/stdint.h /usr/include/features.h
obj/nvme_command_admin.o: /usr/include/bits/predefs.h
obj/nvme_command_admin.o: /usr/include/sys/cdefs.h
obj/nvme_command_admin.o: /usr/include/bits/wordsize.h
obj/nvme_command_admin.o: /usr/include/gnu/stubs.h
obj/nvme_command_admin.o: /usr/include/gnu/stubs-64.h
obj/nvme_command_admin.o: /usr/include/bits/wchar.h nvme_device.h
obj/nvme_command_admin.o: nvme_common.h nvme_registers.h
obj/nvme_command_admin.o: nvme_registers_wrappers.h nvme_queue.h
obj/nvme_command_admin.o: nvme_command.h nvme_types.h cq_thread.h notify.h
obj/nvme_command_admin.o: config.h /usr/include/tinyxml.h
obj/nvme_command_admin.o: /usr/include/ctype.h /usr/include/bits/types.h
obj/nvme_command_admin.o: /usr/include/bits/typesizes.h /usr/include/endian.h
obj/nvme_command_admin.o: /usr/include/bits/endian.h
obj/nvme_command_admin.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
obj/nvme_command_admin.o: /usr/include/stdio.h /usr/include/libio.h
obj/nvme_command_admin.o: /usr/include/_G_config.h /usr/include/wchar.h
obj/nvme_command_admin.o: /usr/include/bits/stdio_lim.h
obj/nvme_command_admin.o: /usr/include/bits/sys_errlist.h
obj/nvme_command_admin.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
obj/nvme_command_admin.o: /usr/include/bits/waitstatus.h
obj/nvme_command_admin.o: /usr/include/sys/types.h /usr/include/time.h
obj/nvme_command_admin.o: /usr/include/sys/select.h
obj/nvme_command_admin.o: /usr/include/bits/select.h
obj/nvme_command_admin.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
obj/nvme_command_admin.o: /usr/include/sys/sysmacros.h
obj/nvme_command_admin.o: /usr/include/bits/pthreadtypes.h
obj/nvme_command_admin.o: /usr/include/alloca.h /usr/include/string.h
obj/nvme_command_admin.o: /usr/include/assert.h
obj/nvme_command.o: nvme_command_admin.h nvme_command_structs.h
obj/nvme_command.o: /usr/include/stdint.h /usr/include/features.h
obj/nvme_command.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
obj/nvme_command.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
obj/nvme_command.o: /usr/include/gnu/stubs-64.h /usr/include/bits/wchar.h
obj/nvme_command.o: nvme_device.h nvme_common.h nvme_registers.h
obj/nvme_command.o: nvme_registers_wrappers.h nvme_queue.h nvme_command.h
obj/nvme_command.o: nvme_types.h cq_thread.h notify.h config.h
obj/nvme_command.o: /usr/include/tinyxml.h /usr/include/ctype.h
obj/nvme_command.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
obj/nvme_command.o: /usr/include/endian.h /usr/include/bits/endian.h
obj/nvme_command.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
obj/nvme_command.o: /usr/include/stdio.h /usr/include/libio.h
obj/nvme_command.o: /usr/include/_G_config.h /usr/include/wchar.h
obj/nvme_command.o: /usr/include/bits/stdio_lim.h
obj/nvme_command.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
obj/nvme_command.o: /usr/include/bits/waitflags.h
obj/nvme_command.o: /usr/include/bits/waitstatus.h /usr/include/sys/types.h
obj/nvme_command.o: /usr/include/time.h /usr/include/sys/select.h
obj/nvme_command.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
obj/nvme_command.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
obj/nvme_command.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
obj/nvme_command.o: /usr/include/string.h /usr/include/assert.h
obj/nvme_common.o: /usr/include/stdio.h /usr/include/features.h
obj/nvme_common.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
obj/nvme_common.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
obj/nvme_common.o: /usr/include/gnu/stubs-64.h /usr/include/bits/types.h
obj/nvme_common.o: /usr/include/bits/typesizes.h /usr/include/libio.h
obj/nvme_common.o: /usr/include/_G_config.h /usr/include/wchar.h
obj/nvme_common.o: /usr/include/bits/stdio_lim.h
obj/nvme_common.o: /usr/include/bits/sys_errlist.h nvme_common.h
obj/nvme_common.o: nvme_registers.h /usr/include/stdint.h
obj/nvme_common.o: /usr/include/bits/wchar.h
obj/nvme_device.o: nvme_device.h nvme_common.h nvme_registers.h
obj/nvme_device.o: /usr/include/stdint.h /usr/include/features.h
obj/nvme_device.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
obj/nvme_device.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
obj/nvme_device.o: /usr/include/gnu/stubs-64.h /usr/include/bits/wchar.h
obj/nvme_device.o: nvme_registers_wrappers.h nvme_queue.h nvme_command.h
obj/nvme_device.o: nvme_command_structs.h nvme_types.h cq_thread.h notify.h
obj/nvme_device.o: config.h /usr/include/tinyxml.h /usr/include/ctype.h
obj/nvme_device.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
obj/nvme_device.o: /usr/include/endian.h /usr/include/bits/endian.h
obj/nvme_device.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
obj/nvme_device.o: /usr/include/stdio.h /usr/include/libio.h
obj/nvme_device.o: /usr/include/_G_config.h /usr/include/wchar.h
obj/nvme_device.o: /usr/include/bits/stdio_lim.h
obj/nvme_device.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
obj/nvme_device.o: /usr/include/bits/waitflags.h
obj/nvme_device.o: /usr/include/bits/waitstatus.h /usr/include/sys/types.h
obj/nvme_device.o: /usr/include/time.h /usr/include/sys/select.h
obj/nvme_device.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
obj/nvme_device.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
obj/nvme_device.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
obj/nvme_device.o: /usr/include/string.h /usr/include/assert.h
obj/nvme_queue.o: /usr/include/sys/param.h /usr/include/limits.h
obj/nvme_queue.o: /usr/include/features.h /usr/include/bits/predefs.h
obj/nvme_queue.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
obj/nvme_queue.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h
obj/nvme_queue.o: /usr/include/bits/posix1_lim.h
obj/nvme_queue.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
obj/nvme_queue.o: /usr/include/bits/posix2_lim.h /usr/include/linux/param.h
obj/nvme_queue.o: /usr/include/asm/param.h /usr/include/asm-generic/param.h
obj/nvme_queue.o: /usr/include/sys/types.h /usr/include/bits/types.h
obj/nvme_queue.o: /usr/include/bits/typesizes.h /usr/include/time.h
obj/nvme_queue.o: /usr/include/endian.h /usr/include/bits/endian.h
obj/nvme_queue.o: /usr/include/bits/byteswap.h /usr/include/sys/select.h
obj/nvme_queue.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
obj/nvme_queue.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
obj/nvme_queue.o: /usr/include/bits/pthreadtypes.h nvme_queue.h
obj/nvme_queue.o: nvme_registers.h /usr/include/stdint.h
obj/nvme_queue.o: /usr/include/bits/wchar.h nvme_command.h
obj/nvme_queue.o: nvme_command_structs.h nvme_types.h cq_thread.h
obj/nvme_queue.o: nvme_device.h nvme_common.h nvme_registers_wrappers.h
obj/nvme_queue.o: notify.h config.h /usr/include/tinyxml.h
obj/nvme_queue.o: /usr/include/ctype.h /usr/include/xlocale.h
obj/nvme_queue.o: /usr/include/stdio.h /usr/include/libio.h
obj/nvme_queue.o: /usr/include/_G_config.h /usr/include/wchar.h
obj/nvme_queue.o: /usr/include/bits/stdio_lim.h
obj/nvme_queue.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
obj/nvme_queue.o: /usr/include/bits/waitflags.h
obj/nvme_queue.o: /usr/include/bits/waitstatus.h /usr/include/alloca.h
obj/nvme_queue.o: /usr/include/string.h /usr/include/assert.h
obj/nvme_queue.o: nvme_command_admin.h nvme_command_io.h

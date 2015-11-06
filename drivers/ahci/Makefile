include ../../mk/global.mk

TARGET = ahci_drv
SOURCES = main.cc ahci_device.cc ahci_port.cc debug.cc
CXXFLAGS += $(XDK_INCLUDES)
LIBS = $(XDK_LIBS) -lpthread -lrt $(XDK_NUMA_LIB)

all: ahci_drv

ahci_drv: $(OBJS) $(XDK_BASE)/lib/libexo/libexo.so
	g++ -g -Wall $(CXXFLAGS) -o ahci_drv $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o ahci_drv obj/


.PHONY: clean $(XDK_BASE)/lib/libexo/libexo.so


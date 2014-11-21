include ../../mk/global.mk

TARGET = ahci_drv
SOURCES = main.cc ahci_device.cc ahci_port.cc debug.cc
CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcommon
LIBS = -L$(XDK_BASE)/lib/libexo -L$(XDK_BASE)/lib/libcommon -L$(XDK_BASE)/lib/libcomponent -lcommon -lcomponent -lexo -lpthread -lrt -lnuma

all: exotest

exotest: $(OBJS) $(XDK_BASE)/lib/libexo/libexo.so
	g++ -g -Wall $(CXXFLAGS) -o ahci_drv $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o ahci_drv obj/


.PHONY: clean $(XDK_BASE)/lib/libexo/libexo.so

include ../../../../mk/global.mk

TARGET = test_drv
SOURCES = main.cc 
CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcommon
LIBS = -L$(XDK_BASE)/lib/libexo -L$(XDK_BASE)/lib/libcommon -L$(XDK_BASE)/lib/libcomponent -lcommon -lcomponent -lexo -lpthread -lrt -lnuma

all: test_drv

test_drv: $(OBJS) $(XDK_BASE)/lib/libexo/libexo.so
	g++ -g -Wall $(CXXFLAGS) -o test_drv $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


clean:
	rm -Rf *.o ahci_drv obj/

.PHONY: clean

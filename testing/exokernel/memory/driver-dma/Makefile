include ../../../../mk/global.mk

TARGET = test_drv
SOURCES = main.cc 
CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcommon -DCONFIG_BUILD_DEBUG
LIBS = -L$(XDK_BASE)/lib/libexo -L$(XDK_BASE)/lib/libcommon -L$(XDK_BASE)/lib/libcomponent -lcommon -lcomponent -lexo -lpthread -lrt -lnuma

all: test_drv

test_drv: $(OBJS) 
	g++ -g -Wall $(CXXFLAGS) -o test_drv $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


clean:
	rm -Rf *.o test_drv obj/

.PHONY: clean

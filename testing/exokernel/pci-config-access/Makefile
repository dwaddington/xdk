include ../../../mk/global.mk

TARGET = test_drv
SOURCES = main.cc 
CXXFLAGS += -g $(XDK_INCLUDES) -DCONFIG_BUILD_DEBUG
LIBS =  $(XDK_LIBS) -lpthread -lrt $(XDK_NUMA_LIB) 

all: test_drv

test_drv: $(OBJS) 
	g++ -g -Wall $(CXXFLAGS) -o test_drv $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


clean:
	rm -Rf *.o test_drv obj/

.PHONY: test_drv

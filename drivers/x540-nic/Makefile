include ../../mk/global.mk
BOOST_LIBS = -lboost_thread
CXXFLAGS += -g -O3 -std=c++11 -Wall -DCONFIG_BUILD_DEBUG -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libexo/exo/ -I$(XDK_BASE)/drivers/x540-nic/ -I$(XDK_BASE) -I$(XDK_BASE)/include -I$(XDK_BASE)/lib/libcommon -I$(XDK_BASE)/lib/libcomponent/component -I$(XDK_BASE)/lib/libcomponent/
LIBS = $(XDK_LIBS) $(BOOST_LIBS) -L$(XDK_BASE)/lib/libexo -L$(XDK_BASE)/lib/libcomponent -lexo -lcomponent -lpthread $(XDK_NUMA_LIB) -ldl -lrt -lm -ltinyxml  

NIC_COMPONENT_SRC := nic_component.cc \
                     ./x540/x540_device.cc \
                     xml_config_parser.cc \

MEM_COMPONENT_SRC := mem_component.cc \
                     xml_config_parser.cc \

all: mem_component nic_component app_main

app_main:
	g++ $(CXXFLAGS) -o $@ main.cc xml_config_parser.cc $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo 

nic_component: 
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_nic.so.1 -o libcomp_nic.so.1 $(NIC_COMPONENT_SRC) $(LIBS)

mem_component: 
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_mem.so.1 -o libcomp_mem.so.1 $(MEM_COMPONENT_SRC) $(LIBS)

clean:
	rm -f app_main libcomp*.so.1 *.o x540/*.o


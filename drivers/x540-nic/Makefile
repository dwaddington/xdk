include ../../mk/global.mk
SILICA_BASE = /home/jkuang/xdk
BOOST_LIBS = -lboost_thread
CXXFLAGS += -g -O3 -I$(SILICA_BASE)/lib/libexo -I$(SILICA_BASE)/lib/libexo/exo/ -I$(SILICA_BASE)/drivers/x540-nic/ -I$(SILICA_BASE)/net -I$(SILICA_BASE)/net/micro-udp -I$(SILICA_BASE) -I$(SILICA_BASE)/include -I$(SILICA_BASE)/lib/libcommon -I$(SILICA_BASE)/lib/libcomponent/component -I$(SILICA_BASE)/lib/libcomponent/
LIBS = $(XDK_LIBS) $(BOOST_LIBS) -L$(SILICA_BASE)/lib/libexo -L$(SILICA_BASE)/lib/libcomponent -lexo -lcomponent -lpthread -lnuma -ldl -lrt -lm -ltinyxml  

NIC_COMPONENT_SRC := nic_component.cc \
                     ./x540/x540_device.cc \
                     ./x540/xml_config_parser.cc \
                     $(SILICA_BASE)/net/micro-udp/stack/exo_stack.cc

MEM_COMPONENT_SRC := mem_component.cc \
                     ./x540/x540_device.cc \
                     ./x540/xml_config_parser.cc \
                    $(SILICA_BASE)/net/micro-udp/stack/exo_stack.cc

STACK_COMPONENT_SRC := $(SILICA_BASE)/net/micro-udp/stack_component.cc \
                       ./x540/x540_device.cc \
                       ./x540/xml_config_parser.cc \
                       $(SILICA_BASE)/net/micro-udp/stack/exo_stack.cc

all: nic_component stack_component mem_component app_main

app_main:
	g++ $(CXXFLAGS) -o $@ main.cc ./x540/xml_config_parser.cc $(LIBS) -Wl,-rpath=$(SILICA_BASE)/lib/libexo 

nic_component: 
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_nic.so.1 -o libcomp_nic.so.1 $(NIC_COMPONENT_SRC) $(LIBS)

mem_component: 
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_mem.so.1 -o libcomp_mem.so.1 $(MEM_COMPONENT_SRC) $(LIBS)

stack_component: 
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_stack.so.1 -o libcomp_stack.so.1 $(STACK_COMPONENT_SRC) $(LIBS)

clean:
	rm -f app_main libcomp*.so.1 *.o x540/*.o


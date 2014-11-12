include ../../mk/global.mk

CXXFLAGS += -g -O3 -I$(SILICA_BASE)/lib/libexo -I$(SILICA_BASE)/app/mcd-binprot -I$(SILICA_BASE)/lib/libexo/exo/component -I$(SILICA_BASE)/drivers/x540-nic/ -I$(SILICA_BASE)/network_stack -I$(SILICA_BASE) -I$(SILICA_BASE)/app/mcd-binprot -I$(SILICA_BASE)/include
LIBS = -L$(SILICA_BASE)/lib/libexo -lexo -lpthread -lnuma -ldl -lrt -lm -ltinyxml -L$(SILICA_BASE)/app/mcd-binprot/libmcdbinprot 

NIC_COMPONENT_SRC := nic_component.cc \
                     ./x540/x540_device.cc \
                     ./x540/evict_support.cc \
                     ./x540/xml_config_parser.cc \
                     $(SILICA_BASE)/network_stack/micro-udp/stack/exo_stack.cc

MEM_COMPONENT_SRC := mem_component.cc \
                     ./x540/x540_device.cc \
                     ./x540/xml_config_parser.cc \
                    $(SILICA_BASE)/network_stack/micro-udp/stack/exo_stack.cc

STACK_COMPONENT_SRC := $(SILICA_BASE)/network_stack/micro-udp/stack_component.cc \
                       ./x540/x540_device.cc \
                       ./x540/xml_config_parser.cc \
                       ./x540/evict_support.cc \
                       $(SILICA_BASE)/network_stack/micro-udp/stack/exo_stack.cc

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


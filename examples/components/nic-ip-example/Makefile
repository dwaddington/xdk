include ../../../../tools/mk/global.mk

CXXFLAGS += -g -I$(SILICA_BASE)/lib/libexo
LIBS = -L$(SILICA_BASE)/lib/libexo -lexo -ldl

all: nic_component tcpip_component app_main

app_main: main.o
	g++ $(CXXFLAGS) -g -o $@ main.cc $(LIBS)

nic_component: nic_component.o
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_nic.so.1 -o libcomp_nic.so.1 nic_component.cc

tcpip_component: tcpip_component.o
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_tcpip.so.1 -o libcomp_tcpip.so.1 tcpip_component.cc

clean:
	rm -f app_main libcomp*.so.1 *.o


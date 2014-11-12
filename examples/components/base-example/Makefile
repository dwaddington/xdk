include ../../../../tools/mk/global.mk

CXXFLAGS += -g -I$(SILICA_BASE)/lib/libexo
LIBS = -L$(SILICA_BASE)/lib/libexo -lexo -ldl

all: hello_component example_client

example_client: 
	g++ $(CXXFLAGS) -g -o $@ main.cc $(LIBS)

hello_component:
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libhello.so.1 -o libhello.so.1 hello_component.cc

clean:
	rm -f example *.o

.PHONY: example_client
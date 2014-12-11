include ../../../mk/global.mk

CXXFLAGS += -g $(XDK_INCLUDES)
BOOST_LIBS = -lboost_thread
LIBS = $(XDK_LIBS) -ldl $(BOOST_LIBS)

all: block_device_component example_client

example_client: 
	g++ $(CXXFLAGS) -g -o $@ main.cc $(LIBS)

block_device_component:
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libcomp_dummybd.so.1 -o libcomp_dummybd.so.1 dummy_block_device.cc $(LIBS)

clean:
	rm -f *.o libcomp_dummybd.so.1 example_client

.PHONY: example_client block_device_component 
include ../../../mk/global.mk

CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcomponent
BOOST_LIBS = -lboost_thread
LIBS = -L$(XDK_BASE)/lib/libexo -lexo -L$(XDK_BASE)/lib/libcomponent -lcomponent -ldl $(BOOST_LIBS)

all: hello_component example_client

example_client: 
	g++ $(CXXFLAGS) -g -o $@ main.cc $(LIBS)

hello_component:
	g++ $(CXXFLAGS) -g -shared -fPIC -Wl,-soname,libhello.so.1 -o libhello.so.1 hello_component.cc $(LIBS)

clean:
	rm -f example *.o

.PHONY: example_client
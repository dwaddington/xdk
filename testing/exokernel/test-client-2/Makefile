include ../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g $(XDK_INCLUDES)
LIBS = $(XDK_LIBS) $(XDK_NUMA_LIB)

all: test-client-2

test-client-2: $(OBJS) 
	g++ -static -g -Wall $(CXXFLAGS) -o test-client-2 $(OBJS) $(LIBS) 

clean:
	rm -Rf *.o test-client-2 obj/


.PHONY: test-client-2

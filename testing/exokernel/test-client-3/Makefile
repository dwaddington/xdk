include ../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g $(XDK_INCLUDES)
LIBS = $(XDK_LIBS) $(XDK_NUMA_LIB)

all: test-client-3

test-client-3: $(OBJS) 
	g++ -g -static -Wall $(CXXFLAGS) -o test-client-3 $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o test-client-3 obj/


.PHONY: test-client-3

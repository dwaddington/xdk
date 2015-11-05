include ../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g $(XDK_INCLUDES) 
LIBS = $(XDK_LIBS) $(XDK_NUMA_LIB)

all: test-ds

test-ds: $(OBJS) 
	g++ -g -Wall $(CXXFLAGS) -o test-ds $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo -lpthread


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o test-ds obj/


.PHONY: test-ds

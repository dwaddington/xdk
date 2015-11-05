include ../../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g $(XDK_INCLUDES)
LIBS = $(XDK_LIBS) $(XDK_NUMA_LIB) 

all: pagemap

pagemap: $(OBJS) 
	g++ -static -g -Wall $(CXXFLAGS) -o pagemap $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o pagemap obj/


.PHONY: pagemap

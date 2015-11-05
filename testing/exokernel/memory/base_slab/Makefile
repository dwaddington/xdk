include ../../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g $(XDK_INCLUDES)
LIBS = $(XDK_LIBS) $(XDK_NUMA_LIB)

all: baseslab

baseslab: $(OBJS) 
	g++ -Wall $(CXXFLAGS) -o baseslab $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o baseslab obj/


.PHONY: baseslab

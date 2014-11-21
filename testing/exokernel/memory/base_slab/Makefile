include ../../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcommon
LIBS = -L$(XDK_BASE)/lib/libexo  -L$(XDK_BASE)/lib/libcommon -lcommon -lexo -lnuma


all: baseslab

baseslab: $(OBJS) 
	g++ -Wall $(CXXFLAGS) -o baseslab $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o baseslab obj/


.PHONY: clean

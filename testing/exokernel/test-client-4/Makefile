include ../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcommon

LIBS = -L$(XDK_BASE)/lib/libexo -lexo -lnuma

all: test-client-4

test-client-4: $(OBJS) 
	g++ -g -static -Wall $(CXXFLAGS) -o test-client-4 $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o test-client-4 obj/


.PHONY: clean

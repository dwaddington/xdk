include ../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo
LIBS = -L$(XDK_BASE)/lib/libexo -lexo -lnuma

all: test-client-2

test-client-2: $(OBJS) 
	g++ -static -g -Wall $(CXXFLAGS) -o test-client-2 $(OBJS) $(LIBS) 

clean:
	rm -Rf *.o test-client-2 obj/


.PHONY: clean

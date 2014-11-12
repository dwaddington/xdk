include ../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g -I$(XDK_BASE)/lib/libexo
LIBS = -L$(XDK_BASE)/lib/libexo -lexo -lnuma

all: test-client-5

test-client-5: $(OBJS) 
	g++ -g -static -Wall $(CXXFLAGS) -o test-client-5 $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o test-client-5 obj/


.PHONY: clean

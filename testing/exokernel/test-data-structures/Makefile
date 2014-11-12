include ../../../../tools/mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g -I$(SILICA_BASE)/lib/libexo
LIBS = -L$(SILICA_BASE)/lib/libexo -lexo -lnuma

all: test-ds

test-ds: $(OBJS) 
	g++ -g -Wall $(CXXFLAGS) -o test-ds $(OBJS) $(LIBS) -Wl,-rpath=$(SILICA_BASE)/lib/libexo -lpthread


$(SILICA_BASE)/lib/libexo/libexo.so:
	make -C $(SILICA_BASE)/lib/libexo

clean:
	rm -Rf *.o test-ds obj/


.PHONY: clean

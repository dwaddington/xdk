include ../../../../../tools/mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g -I$(SILICA_BASE)/lib/libexo
LIBS = -L$(SILICA_BASE)/lib/libexo -lexo -lnuma

all: baseslab

baseslab: $(OBJS) 
	g++ -Wall $(CXXFLAGS) -o baseslab $(OBJS) $(LIBS) -Wl,-rpath=$(SILICA_BASE)/lib/libexo


$(SILICA_BASE)/lib/libexo/libexo.so:
	make -C $(SILICA_BASE)/lib/libexo

clean:
	rm -Rf *.o baseslab obj/


.PHONY: clean

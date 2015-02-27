include ../../../../mk/global.mk

SOURCES = main.cc
CXXFLAGS += -g -I$(SILICA_BASE)/lib/libexo
LIBS = -L$(SILICA_BASE)/lib/libexo -lexo -lnuma

all: hugepage

hugepage: $(OBJS) 
	g++ -static -g -Wall $(CXXFLAGS) -o hugepage $(OBJS) $(LIBS) -Wl,-rpath=$(SILICA_BASE)/lib/libexo


$(SILICA_BASE)/lib/libexo/libexo.so:
	make -C $(SILICA_BASE)/lib/libexo

clean:
	rm -Rf *.o hugepage obj/


.PHONY: clean

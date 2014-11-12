include ../../mk/global.mk

TARGET = nvme_drv
SOURCES = $(wildcard *.cc) 

CXXFLAGS += -static -O2 -I$(XDK_BASE)/lib/libexo

LIBS = -L$(XDK_BASE)/lib/libexo -lexo -lpthread -lrt -lnuma -ldl -ltinyxml

all: nvme_drv

nvme_drv: $(OBJS) $(XDK_BASE)/lib/libexo/libexo.so
	g++ -g -Wall $(CXXFLAGS) -o nvme_drv $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o nvme_drv obj/


.PHONY: clean

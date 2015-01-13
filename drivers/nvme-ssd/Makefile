include ../../mk/global.mk

TARGET = nvme_drv
SOURCES = $(wildcard *.cc) 

CXXFLAGS += -g -O2 -DCONFIG_BUILD_DEBUG -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcommon

LIBS = -L$(XDK_BASE)/lib/libexo -L$(XDK_BASE)/lib/libcommon -L$(XDK_BASE)/lib/libcomponent -lcommon -lcomponent -lexo -lpthread -lrt -lnuma -ltinyxml

all: nvme_drv

nvme_drv: $(OBJS) $(XDK_BASE)/lib/libexo/libexo.so
	g++ -static -g -Wall $(CXXFLAGS) -o nvme_drv $(OBJS) $(LIBS) -Wl,-rpath=$(XDK_BASE)/lib/libexo


$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o nvme_drv obj/


.PHONY: clean

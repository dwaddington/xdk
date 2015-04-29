include ../../mk/global.mk

TARGET = nvme_drv
SOURCES = cq_thread.cc nvme_command_admin.cc nvme_common.cc nvme_queue.cc \
nvme_command.cc nvme_device.cc tests.cc nvme_drv_component.cc \
mt_tests.cc verify_tests.cc 

INCLUDES = -I$(XDK_BASE)/lib/libexo -I$(XDK_BASE)/lib/libcommon -I$(XDK_BASE)/lib/libcomponent

#CXXFLAGS += -DCONFIG_IRQ_COAL
#CXXFLAGS += -DCONFIG_BUILD_DEBUG
#CXXFLAGS += -pg
CXXFLAGS += -g -O2 -fPIC $(INCLUDES)

LIBS = -L$(XDK_BASE)/lib/libexo -L$(XDK_BASE)/lib/libcommon -L$(XDK_BASE)/lib/libcomponent -lcommon -lcomponent -lexo -lpthread -lrt -lnuma -ltinyxml -lboost_random

RPATHS=-Wl,-rpath=$(XDK_BASE)/lib/libexo -Wl,-rpath=/usr/lib/x86_64-linux-gnu/ -Wl,-rpath=.

all: nvme_drv test_client

nvme_drv: $(OBJS) $(XDK_BASE)/lib/libexo/libexo.so
	g++ -g -shared -fPIC -Wall $(CXXFLAGS) $(RPATHS) -Wl,-soname,nvme_drv.so.1 -o nvme_drv.so.1 $(OBJS) $(LIBS)

test_client: test_client.o blast_test.o
	g++ $(CXXFLAGS) -o test_client test_client.o blast_test.o $(LIBS) -ldl

$(XDK_BASE)/lib/libexo/libexo.so:
	make -C $(XDK_BASE)/lib/libexo

clean:
	rm -Rf *.o nvme_drv.so.* obj/


.PHONY: clean

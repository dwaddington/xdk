include ../../mk/global.mk

DEPS = ../../deps
OBJS = actor.o connection.o channel.o 
OBJS += network_channel.o remote_session.o container.o

CXXFLAGS += -g -O2 -std=c++11 -fPIC  -I$(DEPS)/cppzmq -I$(DEPS)/libzmq -I../libcommon -I../libcomponent -I./
# CXXFLAGS += -DCONFIG_BUILD_DEBUG

all: libactor.so.1

libactor.so.1: protos $(OBJS)
	g++ -shared -Wl,-soname,libactor.so.1 -o libactor.so.1 $(OBJS) -L/usr/local/lib  -L../libcommon -lcommon -Wl,-R../libcommon -L../libcomponent -lcomponent -Wl,-R../lib/libcomponent -ldl -lrt -lpthread -lprotobuf $(XDK_NUMA_LIB) -lboost_system -lboost_thread -lboost_program_options -lboost_regex -lzmq
	rm -f libactor.so
	ln -s libactor.so.1 libactor.so

clean:
	rm -f *.o libactor.so.1 libactor.so libactor.a


.PHONY: protos clean


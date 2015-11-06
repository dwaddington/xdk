include ../../mk/global.mk
CXX_FLAGS+=-I./

ifeq ($(XDK_NUMA_LIB), -lnuma)
	OBJS=dump_utils.o cycles.o utils.o
else
	OBJS=dump_utils.o cycles.o utils.o xdk_numa_wrapper.o
endif

#OBJS=dump_utils.o cycles.o utils.o xdk_numa_wrapper.o

all: libcommon.a

libcommon.a: $(OBJS)
	ar rcs libcommon.a $(OBJS)

clean:
	rm -f *.o libcommon.a

%.o:%.cc
	g++ -fPIC -g $(CXX_FLAGS) -c $< -o $@

CXX_FLAGS+=-I./
OBJS=dump_utils.o cycles.o utils.o

all: libcommon.a

libcommon.a: $(OBJS)
	ar rcs libcommon.a $(OBJS)

clean:
	rm -f *.o libcommon.a

%.o:%.cc
	g++ -fPIC -g $(CXX_FLAGS) -c $< -o $@

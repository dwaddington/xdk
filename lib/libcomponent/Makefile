CXX_FLAGS+=-I./ -I../libcommon

all: libcomponent.a

libcomponent.a: component.o
	ar rcs libcomponent.a $<

clean:
	rm -f *.o libcomponent.a

%.o:%.cc
	g++ -std=c++11 -fPIC -g $(CXX_FLAGS) -c $< -o $@

include ../../mk/global.mk

SOURCES := $(wildcard *.cc)
CXXFLAGS += -g -fPIC -Wall

all: libexo.so libexo.a

libexo.a: $(OBJS)
	ar rcs libexo.a $(OBJS)

libexo.so: $(OBJS)
	gcc -g -shared -Wl,-soname,libexo.so -o libexo.so $(OBJS) -ldl -lnuma

clean:
	rm -Rf obj

TAGS:
	etags *.cc *.h

doc:
	doxygen

.PHONY: depend clean TAGS doc

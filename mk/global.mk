#XDK_BASE = $(HOME)/xdk

COBJS   = $(addprefix obj/,$(subst .c,.o,$(filter %.c,$(SOURCES)))) 
CXXOBJS = $(addprefix obj/,$(subst .cc,.o,$(filter %.cc,$(SOURCES))))
OBJS = $(CXXOBJS) $(COBJS)

all:

# emacs tags (for you emacs lovers out there!)
#
etags:
	etags $(SOURCES) *.h $(HOME)/xdk/lib/libexo/*.cc $(HOME)/xdk/lib/libexo/exo/*.h

# valgrind memcheck
#
memcheck:
ifeq ($(P),"")
	@echo "Please do 'make memcheck P' where P is the binary to check"
else
	valgrind --tool=memcheck --leak-check=full $(P)
endif

# makedepend defines rules for header dependencies so
# when you edit a header file you don't need to do
# a make clean; must have 'makedepend' tool installed
#
depend:
	makedepend -pobj/ -o.o -I./ -- -- $(SOURCES)

# general build rule for C++
#
obj/%.o:%.cc
	@mkdir -p obj	
	g++ -c -o $@ $(notdir $<) $(CXXFLAGS)

# general build rule for C
#
obj/%.o:%.c
	@mkdir -p obj	
	gcc -c -o $@ $(notdir $<) $(CFLAGS)

.PHONY: depend memcheck etags run_coverity



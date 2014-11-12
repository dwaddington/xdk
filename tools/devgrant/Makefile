SRC=devgrant.cc
LIBS=-L/usr/local/lib -lboost_program_options -lrt

all: devgrant

devgrant:
	g++ -static -o devgrant $(SRC) $(LIBS)

clean:
	rm -f *.o devgrant

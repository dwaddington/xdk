all: modheader cal-avg

modheader: modheader.o
	g++ -o $@ $< -lstdc++

cal-avg:
	gcc -o cal-avg cal-avg.c
clean:
	rm -f *.o modheader



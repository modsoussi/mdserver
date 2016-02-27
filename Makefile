all: mdserver.c mdserver.h
	gcc -g -Wall -o mdserver mdserver.c

clean:
	rm mdserver 

love:
	@echo Not War

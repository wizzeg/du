FLAGS=-Wall -g -std=gnu11 -Werror -Wall -Wextra -Wpedantic -Wmissing-declarations -Wmissing-prototypes -Wold-style-definition

all: mdu

mdu: mdu.o jobber.o target.o
	gcc -o mdu mdu.o jobber.o target.o -lm -pthread $(FLAGS)

mdu.o: mdu.c jobber.o target.o mdu.h
	gcc -c mdu.c $(FLAGS)

jobber.o: jobber.c target.o jobber.h
	gcc -c jobber.c $(FLAGS)

target.o: target.c target.h
	gcc -c target.c $(FLAGS)
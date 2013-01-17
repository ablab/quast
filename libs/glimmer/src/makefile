# Copyright (c) 2003 by Mihaela Pertea



CC=g++
CFLAGS=-g -I /usr/include/malloc

all: glimmerhmm

glimmerhmm: glimmerhmm.o graph.o sites.o tree_util_prob.o util.o
	$(CC) $(CFLAGS) -o ../glimmerhmm glimmerhmm.o graph.o sites.o tree_util_prob.o util.o  -lm 

glimmerhmm.o: glimmerhmm.c
	$(CC) $(CFLAGS) -c glimmerhmm.c

graph.o: graph.c 
	$(CC) $(CFLAGS) -c graph.c

sites.o: sites.c
	$(CC) $(CFLAGS) -c sites.c

tree_util_prob.o: tree_util_prob.c
	$(CC) $(CFLAGS) -c tree_util_prob.c

util.o: util.c
	$(CC) $(CFLAGS) -c util.c

clean:
	rm -f ../glimmerhmm glimmerhmm.o graph.o sites.o tree_util_prob.o util.o

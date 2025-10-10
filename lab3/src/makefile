CC=gcc
CFLAGS=-I.
TARGETS=sequential_min_max parallel_min_max launch_sequential

all: $(TARGETS)

sequential_min_max: utils.o find_min_max.o sequential_min_max.c
	$(CC) -o $@ find_min_max.o utils.o sequential_min_max.c $(CFLAGS)

parallel_min_max: utils.o find_min_max.o parallel_min_max.c
	$(CC) -o $@ utils.o find_min_max.o parallel_min_max.c $(CFLAGS)

launch_sequential: launch_sequential.c
	$(CC) -o $@ launch_sequential.c $(CFLAGS)

utils.o: utils.c utils.h
	$(CC) -o $@ -c utils.c $(CFLAGS)

find_min_max.o: find_min_max.c find_min_max.h utils.h
	$(CC) -o $@ -c find_min_max.c $(CFLAGS)

clean:
	rm -f utils.o find_min_max.o $(TARGETS)

test-launch: all
	./launch_sequential 42 1000000
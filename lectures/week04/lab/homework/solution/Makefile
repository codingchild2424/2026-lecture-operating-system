CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -O2
TARGET = hashtable_bench

SRCS = main.c hashtable_solution.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c hashtable.h
	$(CC) $(CFLAGS) -c -o $@ $<

hashtable_solution.o: hashtable_solution.c hashtable.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

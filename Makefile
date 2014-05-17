CC = gcc

CFLAGS1 = -O2
CFLAGS = $(CFLAGS1) -D_FILE_OFFSET_BITS=64
LFLAGS = -lhoard -lm -lpthread

SRC = ./mobility.c ./fifo.c
TARGET = mobility

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LFLAGS)

debug:
	$(CC) -g -o $(TARGET) $(SRC) $(LFLAGS)

clean:
	rm $(TARGET)

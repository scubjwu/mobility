CC = gcc

CFLAGS1 = -O2
CFLAGS2 = -D_FILE_OFFSET_BITS=64
CFLAGS = $(CFLAGS1) $(CFLAGS2)
LFLAGS1 = -lm -lpthread
LFLAGS = -lhoard $(LFLAGS1)

SRC = ./mobility.c ./fifo.c
TARGET = mobility

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LFLAGS)

debug:
	$(CC) -g $(CFLAGS2) -o $(TARGET) $(SRC) $(LFLAGS1)

clean:
	rm $(TARGET)

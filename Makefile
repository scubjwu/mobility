CC = gcc

CFLAGS = -O2
LFLAGS = -lhoard -lm -lpthread

SRC = ./mobility.c ./fifo.c
TARGET = mobility

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LFLAGS)

clean:
	rm $(TARGET)

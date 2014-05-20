CC = gcc

CFLAGS1 = -O2
CFLAGS2 = -D_FILE_OFFSET_BITS=64
CFLAGS = $(CFLAGS1) $(CFLAGS2)
LFLAGS1 = -lm -lpthread
LFLAGS = -lhoard $(LFLAGS1)

TSRC = ./mobility.c ./fifo.c
TARGET = mobility
FSRC = ./file_handle.c
FILE_HANDLER = file_handle

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(TSRC) $(LFLAGS)
	$(CC) $(CFLAGS) -o $(FILE_HANDLER) $(FSRC) $(LFALGS)

debug:
	$(CC) -g $(CFLAGS2) -o $(TARGET) $(TSRC) $(LFLAGS1)

clean:
	rm $(TARGET), $(FILE_HANDLER)

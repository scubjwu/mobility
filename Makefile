CC = gcc

CFLAGS1 = -O2
#CFLAGS2 = -D_FILE_OFFSET_BITS=64
CFLAGS2 = 
CFLAGS = $(CFLAGS1) $(CFLAGS2)
LFLAGS1 = -lm -lpthread
LFLAGS2 = -lgsl -lgslcblas
LFLAGS = -lhoard $(LFLAGS1) $(LFLAGS2)

TSRC = ./mobility.c ./fifo.c ./common.c ./flood_trans.c
TARGET = mobility
FSRC = ./file_handle.c ./common.c
FILE_HANDLER = file_handle

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(TSRC) $(LFLAGS)
	$(CC) $(CFLAGS) -o $(FILE_HANDLER) $(FSRC) -lhoard

debug:
	$(CC) -g $(CFLAGS2) -o $(TARGET) $(TSRC) $(LFLAGS1) $(LFLAGS2)

clean:
	rm $(TARGET) $(FILE_HANDLER)

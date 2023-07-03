ifeq ($(DEBUG), y)
	CFLAGS = -Wall -O -g -DDEBUG
else
	CFLAGS = -Wall -O
endif

ifndef CC
	CC = gcc
endif

all: tftp tftpd

tftp: tftp.o
	$(CC) -o tftp tftp.o

tftpd: tftpd.o
	$(CC) -o tftpd tftpd.o

tftp.o: tftp.c
	$(CC) $(CFLAGS) -c tftp.c 

tftpd.o: tftpd.c
	$(CC) $(CFLAGS) -c tftpd.c 

clean:
	rm -f *.o tftp tftpd


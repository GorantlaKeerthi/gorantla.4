CC=gcc
CFLAGS=-Wall -g

sim: oss user

oss: oss.c  proc.o clock.o mlfq.o ioq.o msq.o shm.o
	$(CC) $(CFLAGS) oss.c proc.o clock.o mlfq.o msq.o ioq.o shm.o -o oss -lrt

user: user.c  proc.o clock.o msq.o shm.o
	$(CC) $(CFLAGS) user.c proc.o clock.o msq.o ioq.o shm.o -o user

mlfq.o: mlfq.c mlfq.h
	$(CC) $(CFLAGS) -c mlfq.c

msq.o: msq.c msq.h
	$(CC) $(CFLAGS) -c msq.c

ioq.o: ioq.c ioq.h
	$(CC) $(CFLAGS) -c ioq.c

proc.o: proc.c proc.h
	$(CC) $(CFLAGS) -c proc.c

clock.o: clock.h clock.c
	$(CC) $(CFLAGS) -c clock.c

shm.o: shm.c shm.h
	$(CC) $(CFLAGS) -c shm.c

clean:
	rm -f ./oss ./user *.log *.o output.txt

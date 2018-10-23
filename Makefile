default: oss

CC=gcc
CFLAGS= -g
OBJS=oss.o queue.o

oss: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

oss.o: oss.c oss_struct.h
	$(CC) $(CFLAGS) -c oss.c

queue.o: queue.c oss_struct.h
	$(CC) $(CFLAGS) -c queue.c

clean:
	-rm -f *.o
	-rm -f oss
	-rm -f user
	-rm -f *.out
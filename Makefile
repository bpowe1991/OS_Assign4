default: oss user

CC=gcc
CFLAGS= -g
OBJS=oss.o queue.o
OBJS2=user.o queue.o

oss: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -pthread

oss.o: oss.c oss_struct.h
	$(CC) $(CFLAGS) -c oss.c -pthread

queue.o: queue.c oss_struct.h
	$(CC) $(CFLAGS) -c queue.c -pthread

user.o: user.c oss_struct.h
	$(CC) $(CFLAGS) -c user.c -pthread

user: $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) -pthread

clean:
	-rm -f *.o
	-rm -f oss
	-rm -f user
	-rm -f *.out
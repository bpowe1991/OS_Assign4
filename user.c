/*
Programmer: Briton A. Powe          Program Homework Assignment #4
Date: 10/25/18                      Class: Operating Systems
File: user.c
------------------------------------------------------------------------
Program Description:
This is the file for child processes for oss. User process run in a loop,
checking if it is scheduled from the clock in shared memory. Once it is
scheduled, it will generate a random number to determine how it operate.
Once it recieves a value for terminating, it will exit the loop and end.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <signal.h>
#include <semaphore.h>
#include "oss_struct.h"
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

struct Oss_clock *clockptr;

void sigQuitHandler(int);

int main(int argc, char *argv[]){ 
    signal(SIGQUIT, sigQuitHandler);
    int shmid, deadlineSec, deadlineNanoSec, randInt;
    key_t key = 3670407;
    sem_t *mutex;

    srand(time(0));

    //Finding shared memory segment.
    if ((shmid = shmget(key, sizeof(struct Oss_clock), 0666|IPC_CREAT)) < 0) {
        perror(strcat(argv[0],": Error: Failed shmget find"));
        exit(-1);
    }

    //Attaching to memory segment.
    if ((clockptr = shmat(shmid, NULL, 0)) == (void *) -1) {
        perror(strcat(argv[0],": Error: Failed shmat attach"));
        exit(-1);
    }

    //Accessing semaphore.
    if ((mutex = sem_open ("ossSemTesting2", 1)) == NULL){
        perror(strcat(argv[0],": Error: Failed semaphore creation"));
        exit(-1);    
    } 

    //Entering child critical section and assigning the option.
    do {
        while(clockptr->currentlyRunning != getpid() && clockptr->readyFlag == 0);
        if (clockptr->readyFlag == 1){
            clockptr->childOption = (rand() % (100)) + 1;
            sem_post(mutex);
            clockptr->readyFlag = 0;
        }
    } while(clockptr->childOption > 45 && clockptr->childOption <= 90);

    //Unlinking from semaphore.
    sem_close(mutex);

    //Detaching from memory segment.
    if (shmdt(clockptr) == -1) {
      perror(strcat(argv[0],": Error: Failed shmdt detach"));
      clockptr = NULL;
      exit(-1);
   }

    return 0;
}

//Handler for quit signal.
void sigQuitHandler(int sig) {
   abort();
}
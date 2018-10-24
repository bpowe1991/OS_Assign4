/*
Programmer: Briton A. Powe          Program Homework Assignment #2
Date: 9/23/18                       Class: Operating Systems
File: oss.c
------------------------------------------------------------------------
Program Description:
Takes in integer command line parameters for option -n and -s to fork
and execvp n number of processes, allowing only s to be run simultaneously.
Program terminates children and self after catching a signal from a 
2 sec alarm or user enters Ctrl-C. Before ending, program outputs integer
values in shared memory and deallocates the shared memory segment. 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <signal.h>
#include <semaphore.h>
#include "oss_struct.h"

int flag = 0;
pid_t parent;

int is_pos_int(char test_string[]);
void alarmHandler();
void interruptHandler();

int main(int argc, char *argv[]){

    signal(SIGINT,interruptHandler);
    signal(SIGALRM, alarmHandler);
    signal(SIGQUIT, SIG_IGN);

    struct Oss_clock *clockptr;
    char filename[20] = "log.txt";
    int opt, s = 10, t = 2, shmid, status = 0, iteration = 0;
    double percentage = 0.0;
    key_t key = 3670407;
	pid_t childpid = 0, wpid;
    FILE *logPtr;
    sem_t *mutex;
    parent = getpid();
    srand(time(0));
    
    //Setting queues for oss.
    queue* lowPriority = createQueue(18);
    queue* highPriority = createQueue(18);
    queue* blocked = createQueue(18);

    //Parsing options.
    while((opt = getopt(argc, argv, "s:t:l:hp")) != -1){
		switch(opt){

            //Option to enter s.
            case 's':
				if(is_pos_int(optarg) == 1){
					fprintf(stderr, "%s: Error: Entered illegal input for option -s\n",
							argv[0]);
					exit(-1);
				}
				else{
                    s = atoi(optarg);
                    if (s <= 0 || s > 18) {
                        fprintf(stderr, "%s: Error: Entered illegal input for option -s :"\
                                " s needs to be 1-18\n", argv[0]);
                        exit(-1);
                    }
				}
				break;
            
            //Option to enter t.
            case 't':
				if(is_pos_int(optarg) == 1){
					fprintf(stderr, "%s: Error: Entered illegal input for option -t\n",
							argv[0]);
					exit(-1);
				}
				else {
                    t = atoi(optarg);
                    if (t <= 0) {
                        fprintf(stderr, "%s: Error: Entered illegal input for option -t", argv[0]);
                        exit(-1);
                    }
				}
				break;
            
            //Option to enter l.
            case 'l':
				sprintf(filename, "%s", optarg);
                if (strcmp(filename, "") == 0){
                    fprintf(stderr, "%s: Error: Entered illegal input for option -l:"\
                                        " invalid filename\n", argv[0]);
                    exit(-1);
                }
				break;

            //Help option.
            case 'h':
                fprintf(stderr, "\nThis program creates s number of child processes with the\n"\
                                "-s option. The parent increments a timer in shared memory and\n"\
                                " the children terminate based on the timer.\n"\
                                "OPTIONS:\n\n"\
                                "-s Set the number of process to be entered. "\
                                "(i.e. \"executible name\" -s 4 creates 4 children processes).\n"\
                                "-t Set the number of seconds the program will run."\
                                "(i.e. -t 4 allows the program to run for 4 sec).\n"\
                                "-l set the name of the log file (default: log.txt).\n"\
                                "(i.e. -l logFile.txt sets the log file name to logFile.txt).\n"\
                                "-h Bring up this help message.\n"\
                                "-p Bring up a test error message.\n\n");
                exit(0);
                break;
            
            //Option to print error message using perror.
            case 'p':
                perror(strcat(argv[0], ": Error: This is a test Error message"));
                exit(-1);
                break;
            case '?':
                fprintf(stderr, "%s: Error: Unrecognized option \'-%c\'\n", argv[0], optopt);
                exit(-1);
                break;
			default:
				fprintf(stderr, "%s: Error: Unrecognized option\n",
					    argv[0]);
				exit(-1);
		}
	}

    //Checking if m, s, and t have valid integer values.
    if (s <= 0 || t <= 0){
        perror(strcat(argv[0], ": Error: Illegal parameter for -n, -s, or -t"));
        exit(-1);
    }
   
   //Creating or opening log file.
   if((logPtr = fopen(filename,"w")) == NULL)
   {
      fprintf(stderr, "%s: Error: Failed to open/create log file\n",
					    argv[0]);
      exit(-1);             
   }

    //alarm(t);
    
    //Creating shared memory segment.
    if ((shmid = shmget(key, sizeof(struct Oss_clock), 0666|IPC_CREAT)) < 0) {
        perror(strcat(argv[0],": Error: Failed shmget allocation"));
        exit(-1);
    }

    //Attaching to memory segment.
    if ((clockptr = shmat(shmid, NULL, 0)) == (void *) -1) {
        perror(strcat(argv[0],": Error: Failed shmat attach"));
        exit(-1);
    }

    //Creating or opening semaphore
    if ((mutex = sem_open ("ossSemTesting2", O_CREAT, 0644, 0)) == NULL){
        perror(strcat(argv[0],": Error: Failed semaphore creation"));

        exit(-1);    
    } 

    int clockIncrement = 0, numberRunning = 0;
    //Parent main loop.
    do {
        clockIncrement = 0;
        
        if (clockptr->nanoSec > ((int)1e9)) {
                clockptr->sec += (clockptr->nanoSec/((int)1e9));
                clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
            }

        if (((rand() % (10)) + 1) <= 75 && numberRunning < 18){
            iteration += 1;
            clockptr->numChildren += 1;
            numberRunning += 1;
            processBlock newBlock = {.childPid = (pid_t)(getpid()+iteration), .usageTime = 0, .waitTimeNSec = 0};
            if (((rand() % (100)) + 1) >= 85){
                newBlock.priority = 0;
            }
            else {
                newBlock.priority = 1;
            }
            
            //Generating overhead time and adjusting clock.
            clockIncrement = (rand() % 1001);
            clockptr->nanoSec += clockIncrement;
            clockptr->totalUsage += clockIncrement;
            if (clockptr->nanoSec > ((int)1e9)) {
                clockptr->sec += (clockptr->nanoSec/((int)1e9));
                clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
            }

            if (newBlock.priority == 0){
                enqueue(highPriority, newBlock);
                fprintf(logPtr, "OSS : %ld : Adding new process to high priority queue at %d.%d\n", 
                        (long)newBlock.childPid, clockptr->sec, clockptr->nanoSec);
            }
            else {
                enqueue(lowPriority, newBlock);
                fprintf(logPtr, "OSS : %ld : Adding new process to low priority queue at %d.%d\n", 
                        (long)newBlock.childPid, clockptr->sec, clockptr->nanoSec);
            }

            fprintf(logPtr, "OSS : Time taken to generate process : %d ns\n", clockIncrement);
        }

        if (isEmpty(blocked) == 0){
            for (int x = 0; x < blocked->size; x++){
                if ((blocked->array[blocked->front+x].waitTimeSec < clockptr->sec) ||
                    ((blocked->array[blocked->front+x].waitTimeSec == clockptr->sec) &&
                    (blocked->array[blocked->front+x].waitTimeNSec <= clockptr->nanoSec))){
                    blocked->array[blocked->front+x].waitTimeSec = 0;
                    blocked->array[blocked->front+x].waitTimeNSec = 0;
                    
                    //Generating overhead time and adjusting clock.
                    clockIncrement = (rand() % 1001);
                    clockptr->nanoSec += clockIncrement;
                    clockptr->totalUsage += clockIncrement;
                    if (clockptr->nanoSec > ((int)1e9)) {
                        clockptr->sec += (clockptr->nanoSec/((int)1e9));
                        clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                    }

                    fprintf(logPtr, "OSS : Time taken to move from blocked : %d ns\n", clockIncrement);
                    if (blocked->array[blocked->front+x].priority == 0){
                        fprintf(logPtr, "OSS : %ld : Moving from blocked to high priority queue at %d.%d\n", 
                        (long)blocked->array[blocked->front+x].childPid, clockptr->sec, clockptr->nanoSec);
                        enqueue(highPriority, *(dequeue(blocked)));
                    }
                    else {
                        fprintf(logPtr, "OSS : %ld : Moving from blocked to low priority queue at %d.%d\n", 
                        (long)blocked->array[blocked->front+x].childPid, clockptr->sec, clockptr->nanoSec);
                        enqueue(lowPriority, *(dequeue(blocked)));
                    }
                    x -= 1;
                }
            }
        }

        //Scheduling process.
        if (isEmpty(highPriority) == 0 || isEmpty(lowPriority) == 0){
            if (isEmpty(highPriority) == 0){
                clockptr->currentlyRunning = front(highPriority)->childPid;
                clockptr->readyFlag = 1;
                clockptr->runningPriority = front(highPriority)->priority;
            }
            else if (isEmpty(highPriority) == 1 && isEmpty(lowPriority) == 0){
                clockptr->currentlyRunning = front(lowPriority)->childPid;
                clockptr->readyFlag = 1;
                clockptr->runningPriority = front(lowPriority)->priority;
            }

            //Generating Overhead for scheduling.
            clockIncrement = (rand() % 1001);
            clockptr->nanoSec += clockIncrement;
            clockptr->totalUsage += clockIncrement;
            if (clockptr->nanoSec > ((int)1e9)) {
                clockptr->sec += (clockptr->nanoSec/((int)1e9));
                clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
            }

            fprintf(logPtr, "OSS : %ld : Dispatching at time %d.%d\n", (long)clockptr->currentlyRunning,
                    clockptr->sec, clockptr->nanoSec);
        }
        else {
            clockptr->currentlyRunning = 0;
            clockptr->readyFlag = 0;
            clockptr->totalIdle += clockIncrement;
        }

        if (clockptr->currentlyRunning != 0){
            clockptr->childOption = (rand() % (100)) + 1;
            if (clockptr->childOption <= 45){
                if (clockptr->runningPriority == 0){
                    clockIncrement = 500000;
                    clockptr->nanoSec += clockIncrement;
                    clockptr->totalUsage += clockIncrement;
                    if (clockptr->nanoSec > ((int)1e9)) {
                        clockptr->sec += (clockptr->nanoSec/((int)1e9));
                        clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                    }
                    fprintf(logPtr, "OSS : %ld : Ran for 500000 ns\n", (long)clockptr->currentlyRunning);
                }
                else {
                    clockIncrement = 1000000;
                    clockptr->nanoSec += clockIncrement;
                    clockptr->totalUsage += clockIncrement;
                    if (clockptr->nanoSec > ((int)1e9)) {
                        clockptr->sec += (clockptr->nanoSec/((int)1e9));
                        clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                    }
                    fprintf(logPtr, "OSS : %ld : Ran for 1000000 ns\n", (long)clockptr->currentlyRunning);
                }
            }
            else if (clockptr->childOption >= 46 && clockptr->childOption <= 90){
                clockIncrement = 300;
                clockptr->nanoSec += clockIncrement;
                clockptr->totalUsage += clockIncrement;
                if (clockptr->nanoSec > ((int)1e9)) {
                    clockptr->sec += (clockptr->nanoSec/((int)1e9));
                    clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                }
                fprintf(logPtr, "OSS : %ld : Terminating and removing from queue at time %d.%d\n", 
                        (long)clockptr->currentlyRunning, clockptr->sec, clockptr->nanoSec);
                if (clockptr->runningPriority == 0){
                    dequeue(highPriority);
                }
                else {
                    dequeue(lowPriority);
                }
            }
            else if (clockptr->childOption >= 91 && clockptr->childOption <= 100){
                percentage = (rand() % (99)) + 1;
                fprintf(logPtr, "OSS : %ld : %d%% of quantum used\n", 
                        (long)clockptr->currentlyRunning, (int)percentage);
                if (clockptr->runningPriority == 0){
                    clockIncrement = (int)((percentage*500000.00)/100.0);
                    enqueue(blocked, *(dequeue(highPriority)));
                }
                else {
                    clockIncrement = (int)((percentage*1000000.00)/100.0);
                    enqueue(blocked, *(dequeue(lowPriority)));
                }
                
                clockptr->nanoSec += clockIncrement;
                clockptr->totalUsage += clockIncrement;
                if (clockptr->nanoSec > ((int)1e9)) {
                    clockptr->sec += (clockptr->nanoSec/((int)1e9));
                    clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                }

                blocked->array[blocked->rear].waitTimeNSec = clockptr->nanoSec+(rand() % 1001);
                    blocked->array[blocked->rear].waitTimeSec = clockptr->sec;
                    fprintf(stderr, "value: %d\n", blocked->array[blocked->rear].waitTimeNSec);
                    if (blocked->array[blocked->rear].waitTimeNSec > ((int)1e9)) {
                         blocked->array[blocked->rear].waitTimeSec = 
                        (blocked->array[blocked->rear].waitTimeNSec/((int)1e9))+clockptr->sec;
                         blocked->array[blocked->rear].waitTimeNSec = 
                        (blocked->array[blocked->rear].waitTimeNSec)%((int)1e9);
                    }
                fprintf(logPtr, "OSS : %ld : Ran for %d ns: moved to blocked queue\n", 
                        (long)clockptr->currentlyRunning, clockIncrement);
                fprintf(stderr, "%ld : Wait time %d.%d\n", (long)blocked->array[blocked->rear].childPid, blocked->array[blocked->rear].waitTimeSec, 
                        blocked->array[blocked->rear].waitTimeNSec);
            }

        }

        clockptr->currentlyRunning = 0;
        clockptr->readyFlag = 0;


    } while (flag == 0 && iteration < 10);

    fprintf(stderr, "Total Usage time: %d ns\n", clockptr->totalUsage);
    fprintf(stderr, "Total Idle time: %d ns\n", clockptr->totalIdle);
    fprintf(stderr, "Average Usage time: %d ns\n", clockptr->totalUsage/clockptr->numChildren);
    fprintf(stderr, "Average Idle time: %d ns\n", clockptr->totalIdle/clockptr->numChildren);

    //Sending signal to all children
    if (flag == 1) {
        if (kill(-parent, SIGQUIT) == -1) {
            perror(strcat(argv[0],": Error: Failed kill"));
            exit(-1);
        }
    }

    //Detaching from memory segment.
    if (shmdt(clockptr) == -1) {
        perror(strcat(argv[0],": Error: Failed shmdt detach"));
        clockptr = NULL;
        exit(-1);
    }

    //Removing memory segment.
    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        perror(strcat(argv[0],": Error: Failed shmctl delete"));
        exit(-1);
    }

    //Disconnecting and removing semaphore.
    sem_close(mutex);
    sem_unlink ("ossSem");   
    fclose(logPtr);
        
    return 0;
}

    // //Child
    // else {
    //     char *args[]={"./user", NULL};
    //     if ((execvp(args[0], args)) == -1) {
    //         perror(strcat(argv[0],": Error: Failed to execvp child program\n"));
    //         exit(-1);
    //     }    
    // }


//Function to check whether string is a positive integer
int is_pos_int(char test_string[]){
	int is_num = 0;
	for(int i = 0; i < strlen(test_string); i++)
	{
		if(!isdigit(test_string[i]))
			is_num = 1;
	}

	return is_num;
}        

//Signal handler for 2 sec alarm
void alarmHandler() {
    flag = 1;
}

//Signal handler for Ctrl-C
void interruptHandler() {
    flag = 1;
}
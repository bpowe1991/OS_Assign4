/*
Programmer: Briton A. Powe          Program Homework Assignment #4
Date: 10/25/18                      Class: Operating Systems
File: oss.c
------------------------------------------------------------------------
Program Description:
Simulates an oss scheduler. Randomly forks off children and organizes them
with priority queues. Creates a clock structure in shared memory to track
time and inform which process is to be dispatched and run. All events performed
are recorded in a log file called log.txt by default or in a file designated
by the -l option. To set the timer to termante the program, it uses a -t option.
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
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

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
    int opt, t = 30, shmid, status = 0, iteration = 0, 
        scheduleTimeSec = 0, scheduleTimeNSec = 0;
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
    while((opt = getopt(argc, argv, "t:l:hp")) != -1){
		switch(opt){
            
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
                fprintf(stderr, "\nThis program simulates scheduling through queues and shared\n"\
                                "memory. The parent increments a clock in shared memory and\n"\
                                "schedules children to run based around the clock.\n"\
                                "OPTIONS:\n\n"\
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
    if (t <= 0){
        perror(strcat(argv[0], ": Error: Illegal parameter for -t option"));
        exit(-1);
    }
   
   //Creating or opening log file.
   if((logPtr = fopen(filename,"w+")) == NULL)
   {
      fprintf(stderr, "%s: Error: Failed to open/create log file\n",
					    argv[0]);
      exit(-1);             
   }

    alarm(t);
    
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
    if ((mutex = sem_open ("ossScheduleSem", O_CREAT, 0644, 0)) == NULL){
        perror(strcat(argv[0],": Error: Failed semaphore creation"));

        exit(-1);    
    } 

    int clockIncrement = 0, numberRunning = 0, numberWrites = 0;
    
    //Parent main loop.
    do {
        //Setting time for new process.
        scheduleTimeSec = clockptr->sec; //+(rand() % (2));
        scheduleTimeNSec = clockptr->nanoSec+(rand() % (1001));
        if (scheduleTimeNSec >= ((int)1e9)){
            scheduleTimeSec += scheduleTimeNSec/((int)1e9);
            scheduleTimeNSec = scheduleTimeNSec%((int)1e9);  
        } 
        
        //Determine if a new process can be created.
        if (numberRunning < 18){
            if ((scheduleTimeSec < clockptr->sec) ||
                (scheduleTimeSec == clockptr->sec && scheduleTimeNSec <= clockptr->nanoSec)){
                //Forking child after wait.
                if ((childpid = fork()) < 0) {
                    perror(strcat(argv[0],": Error: Failed to create child"));
                }
                else if (childpid == 0) {
                    char *args[]={"./user", NULL};
                    if ((execvp(args[0], args)) == -1) {
                        perror(strcat(argv[0],": Error: Failed to execvp child program\n"));
                        exit(-1);
                    }    
                }

                clockptr->numChildren += 1;
                numberRunning += 1;
                processBlock newBlock = {.childPid = childpid, .usageTime = 0,
                                         .waitTimeNSec = 0, .timeInSysSec = clockptr->sec,
                                         .timeInSysNSec = clockptr->nanoSec};
                
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

                //Generating new process priority.
                if (newBlock.priority == 0){
                    enqueue(highPriority, newBlock);
                    fprintf(logPtr, "OSS : %ld : Adding new process to high priority queue at %d.%d\n", 
                            (long)newBlock.childPid, clockptr->sec, clockptr->nanoSec);
                    numberWrites += 1;
                }
                else {
                    enqueue(lowPriority, newBlock);
                    fprintf(logPtr, "OSS : %ld : Adding new process to low priority queue at %d.%d\n", 
                            (long)newBlock.childPid, clockptr->sec, clockptr->nanoSec);
                    numberWrites += 1;
                }

                fprintf(logPtr, "OSS : Time taken to generate process : %d ns\n", clockIncrement);
                numberWrites += 1;
            }
        }

        //Checking blocked queue for ready processes.
        if (isEmpty(blocked) == 0){
            
            //Looping through blocked queue for ready processes.
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

                    //Moving ready process back to respective priority queue.
                    fprintf(logPtr, "OSS : Time taken to move from blocked : %d ns\n", clockIncrement);
                    numberWrites += 1;
                    if (blocked->array[blocked->front+x].priority == 0){
                        fprintf(logPtr, "OSS : %ld : Moving from blocked to high priority queue at %d.%d\n", 
                        (long)blocked->array[blocked->front+x].childPid, clockptr->sec, clockptr->nanoSec);
                        enqueue(highPriority, *(dequeue(blocked)));
                        numberWrites += 1;
                    }
                    else {
                        fprintf(logPtr, "OSS : %ld : Moving from blocked to low priority queue at %d.%d\n", 
                        (long)blocked->array[blocked->front+x].childPid, clockptr->sec, clockptr->nanoSec);
                        enqueue(lowPriority, *(dequeue(blocked)));
                        numberWrites += 1;
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
            numberWrites += 1;
        }

        //If no process is ready then adding to system idle time.
        else if (isEmpty(highPriority) == 1 && isEmpty(lowPriority) == 1){
            clockIncrement = (rand() % 1001);
            clockptr->currentlyRunning = 0;
            clockptr->readyFlag = 0;
            clockptr->totalIdle += clockIncrement;
        }

        //Letting scheduled process run based on option listed in shared memory.
        if (clockptr->currentlyRunning != 0){
            //clockptr->childOption = (rand() % (100)) + 1;
            sem_wait(mutex);

            //If option was to let process run til quantum.
            if (clockptr->childOption <= 45){
                if (clockptr->runningPriority == 0){
                    clockIncrement = 5000;
                    clockptr->nanoSec += clockIncrement;
                    clockptr->totalUsage += clockIncrement;
                    if (clockptr->nanoSec > ((int)1e9)) {
                        clockptr->sec += (clockptr->nanoSec/((int)1e9));
                        clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                    }
                    fprintf(logPtr, "OSS : %ld : Ran for 5000 ns\n", (long)clockptr->currentlyRunning);
                    numberWrites += 1;
                }
                else {
                    clockIncrement = 10000;
                    clockptr->nanoSec += clockIncrement;
                    clockptr->totalUsage += clockIncrement;
                    if (clockptr->nanoSec > ((int)1e9)) {
                        clockptr->sec += (clockptr->nanoSec/((int)1e9));
                        clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                    }
                    fprintf(logPtr, "OSS : %ld : Ran for 10000 ns\n", (long)clockptr->currentlyRunning);
                    numberWrites += 1;
                }
            }

            //If option was to terminate.
            else if (clockptr->childOption > 45 && clockptr->childOption <= 90){
                clockIncrement = (rand() % 1001);
                clockptr->nanoSec += clockIncrement;
                clockptr->totalUsage += clockIncrement;
                if (clockptr->nanoSec > ((int)1e9)) {
                    clockptr->sec += (clockptr->nanoSec/((int)1e9));
                    clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                }
                fprintf(logPtr, "OSS : %ld : Terminating and removing from queue at time %d.%d\n", 
                        (long)clockptr->currentlyRunning, clockptr->sec, clockptr->nanoSec);
                numberWrites += 1;
                if (clockptr->runningPriority == 0){
                    clockptr->totalWait+= 
                            (((clockptr->sec-highPriority->array[highPriority->front].timeInSysSec)*((int)1e9))+
                               abs(clockptr->nanoSec-highPriority->array[highPriority->front].timeInSysNSec))-
                               highPriority->array[highPriority->front].usageTime;
                    dequeue(highPriority);
                }
                else {
                    clockptr->totalWait+= 
                            (((clockptr->sec-lowPriority->array[lowPriority->front].timeInSysSec)*((int)1e9))+
                               abs(clockptr->nanoSec-lowPriority->array[lowPriority->front].timeInSysNSec))-
                               lowPriority->array[lowPriority->front].usageTime;
                    dequeue(lowPriority);
                }
            }

            //If option was to be interrupted and use a percentage of quantum.
            else if (clockptr->childOption > 90 && clockptr->childOption <= 100){
                
                //Generating quantum percentage and moving process to blocked queue.
                percentage = (rand() % (99)) + 1;
                fprintf(logPtr, "OSS : %ld : %d%% of quantum used\n", 
                        (long)clockptr->currentlyRunning, (int)percentage);
                numberWrites += 1;
                
                if (clockptr->runningPriority == 0){
                    clockIncrement = (int)((percentage*5000.00)/100.0);
                    enqueue(blocked, *(dequeue(highPriority)));
                }
                else {
                    clockIncrement = (int)((percentage*10000.00)/100.0);
                    enqueue(blocked, *(dequeue(lowPriority)));
                }
                
                //Adding overhead to system clock.
                clockptr->nanoSec += clockIncrement;
                clockptr->totalUsage += clockIncrement;
                if (clockptr->nanoSec > ((int)1e9)) {
                    clockptr->sec += (clockptr->nanoSec/((int)1e9));
                    clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
                }

                //Generating wait time for blocked process.
                blocked->array[blocked->rear].waitTimeNSec = clockptr->nanoSec+(rand() % 1001);
                blocked->array[blocked->rear].waitTimeSec = clockptr->sec;//+(rand() % 6);
                if (blocked->array[blocked->rear].waitTimeNSec > ((int)1e9)) {
                        blocked->array[blocked->rear].waitTimeSec += 
                    (blocked->array[blocked->rear].waitTimeNSec/((int)1e9))+clockptr->sec;
                        blocked->array[blocked->rear].waitTimeNSec = 
                    (blocked->array[blocked->rear].waitTimeNSec)%((int)1e9);
                }
                
                fprintf(logPtr, "OSS : %ld : Ran for %d ns: moved to blocked queue\n", 
                        (long)clockptr->currentlyRunning, clockIncrement);
                numberWrites += 1;
            }

        }

        clockptr->currentlyRunning = 0;
        clockptr->readyFlag = 0;

    } while (flag == 0 && numberWrites < 10000);

    fprintf(logPtr, "\nOSS :: System Ending ::\n\nOSS : System Statistics\n");
    fprintf(logPtr, "Total Usage time: %ld ns\n", clockptr->totalUsage);
    fprintf(logPtr, "Total Idle time: %ld ns\n", clockptr->totalIdle);
    fprintf(logPtr, "Total Wait time: %ld ns\n", clockptr->totalWait);
    fprintf(logPtr, "Average Usage time: %ld ns\n", clockptr->totalUsage/clockptr->numChildren);
    fprintf(logPtr, "Average Idle time: %ld ns\n", clockptr->totalIdle/clockptr->numChildren);
    fprintf(logPtr, "Average Wait time: %ld ns\n", clockptr->totalWait/clockptr->numChildren);

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
    sem_unlink ("ossScheduleSem");   
    fclose(logPtr);
        
    return 0;
}

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
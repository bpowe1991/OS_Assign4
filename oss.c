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

    struct clock *clockptr;
    char filename[20] = "log.txt";
    int opt, s = 10, t = 2, shmid, status = 0;
    key_t key = 3670407;
	pid_t childpid = 0, wpid;
    FILE *logPtr;
    sem_t *mutex;
    parent = getpid();

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

queue* lowPriority = createQueue(18);

for(int x = 0; x < 3; x++){
    processBlock newProcess = {.childPid = (pid_t)(getpid()+x)};
    enqueue(lowPriority, newProcess);
}

fprintf(stderr, "%d\n", isFull(lowPriority));
fprintf(stderr, "%d\n", isEmpty(lowPriority));

for(int x = 0; x < 5; x++){
    fprintf(stderr, "%ld\n", (long)front(lowPriority).childPid);
    dequeue(lowPriority);
}

//     //Checking if m, s, and t have valid integer values.
//     if (s <= 0 || t <= 0){
//         perror(strcat(argv[0], ": Error: Illegal parameter for -n, -s, or -t"));
//         exit(-1);
//     }
   
//    //Creating or opening log file.
//    if((logPtr = fopen(filename,"a")) == NULL)
//    {
//       fprintf(stderr, "%s: Error: Failed to open/create log file\n",
// 					    argv[0]);
//       exit(-1);             
//    }

//     alarm(t);
    
//     //Creating shared memory segment.
//     if ((shmid = shmget(key, sizeof(struct clock), 0666|IPC_CREAT)) < 0) {
//         perror(strcat(argv[0],": Error: Failed shmget allocation"));
//         exit(-1);
//     }

//     //Attaching to memory segment.
//     if ((clockptr = shmat(shmid, NULL, 0)) == (void *) -1) {
//         perror(strcat(argv[0],": Error: Failed shmat attach"));
//         exit(-1);
//     }

//     //Creating or opening semaphore
//     if ((mutex = sem_open ("ossSemTesting1", O_CREAT, 0644, 0)) == NULL){
//         perror(strcat(argv[0],": Error: Failed semaphore creation"));

//         exit(-1);    
//     } 

    //Parent main loop.
    // do {
            

    // } while (flag == 0);

    //Sending signal to all children
    if (flag == 1) {
        if (kill(-parent, SIGQUIT) == -1) {
            perror(strcat(argv[0],": Error: Failed kill"));
            exit(-1);
        }
    }

    // //Detaching from memory segment.
    // if (shmdt(clockptr) == -1) {
    //     perror(strcat(argv[0],": Error: Failed shmdt detach"));
    //     clockptr = NULL;
    //     exit(-1);
    // }

    // //Removing memory segment.
    // if (shmctl(shmid, IPC_RMID, 0) == -1) {
    //     perror(strcat(argv[0],": Error: Failed shmctl delete"));
    //     exit(-1);
    // }

    // //Disconnecting and removing semaphore.
    // sem_close(mutex);
    // sem_unlink ("ossSem");   
    // fclose(logPtr);
        
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
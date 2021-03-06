/*
Programmer: Briton A. Powe          Program Homework Assignment #4
Date: 10/25/18                      Class: Operating Systems
File: oss_struct.c
------------------------------------------------------------------------
Program Description:
Header file to declare process block, oss shared memory clock, and queue
structures. The function declarations are also stated here.
*/

#ifndef OSS_CLOCK
#define OSS_CLOCK
#include <sys/types.h>
#include <stdio.h>

//Stucture of process control block.
typedef struct processBlock {
    pid_t childPid;
    int usageTime;
    int waitTimeSec;
    int waitTimeNSec;
    int priority;
    int timeInSysSec;
    int timeInSysNSec;
} processBlock;

//Structure to be used in shared memory.
typedef struct Oss_clock {
    pid_t currentlyRunning;
    int readyFlag;
    int runningPriority;
    int sec;
    int nanoSec;
    int childOption;
    long totalUsage;
    long totalWait;
    int numChildren;
    long totalIdle;
} oss_clock;

//Queue structure.
typedef struct queue 
{ 
    int front, rear, size; 
    unsigned capacity; 
    processBlock* array; 
} queue; 

//Queue function declarations.
queue* createQueue(unsigned capacity);
int isFull(struct queue* queue);
int isEmpty(struct queue* queue);
void enqueue(struct queue* queue, struct processBlock item);
processBlock* dequeue(struct queue* queue);
processBlock* front(struct queue* queue);

#endif
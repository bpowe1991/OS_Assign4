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
} processBlock;

//Structure to be used in shared memory.
typedef struct Oss_clock {
    pid_t currentlyRunning;
    int readyFlag;
    int runningPriority;
    int sec;
    int nanoSec;
    int childOption;
    int totalUsage;
    int totalWait;
    int numChildren;
    int totalIdle;
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
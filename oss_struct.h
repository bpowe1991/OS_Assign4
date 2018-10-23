#ifndef OSS_CLOCK
#define OSS_CLOCK
#include <sys/types.h>
#include <stdio.h>

//Stucture of process control block.
typedef struct processBlock {
    pid_t childPid;
    int readyFlag;
    int usageTime;
    int waitTime;
    int priority;
} processBlock;

//Structure to be used in shared memory.
typedef struct clock {
    pid_t currentlyRunning;
    int readyFlag;
    int sec;
    int nanoSec;
    int childOption;
    int totalUsage;
    int totalIdle;
    int numChildren;
} clock;

//Queue structure.
typedef struct queue 
{ 
    int front, rear, size; 
    unsigned capacity; 
    struct processBlock* array; 
} queue; 

//Queue function declarations.
queue* createQueue(unsigned capacity);
int isFull(struct queue* queue);
int isEmpty(struct queue* queue);
void enqueue(struct queue* queue, struct processBlock item);
struct processBlock dequeue(struct queue* queue);
struct processBlock front(struct queue* queue);

#endif
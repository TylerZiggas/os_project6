#pragma once
#ifndef OSS_H
#define OSS_H

#include <stdlib.h> // Necessary includes
#include <stdio.h>    
#include <stdbool.h>    
#include <stdint.h>   
#include <string.h>    
#include <unistd.h>   
#include <stdarg.h>   
#include <errno.h>      
#include <signal.h> 
#include <ctype.h> 
#include <sys/ipc.h> 
#include <sys/msg.h>   
#include <sys/shm.h>  
#include <sys/sem.h>   
#include <sys/time.h>   
#include <sys/types.h>  
#include <sys/wait.h>   
#include <time.h>      
#include <math.h>	

#define BUFFER_LENGTH 4096 // Constant declarations
#define MAX_FILE_LINE 100000
#define TERMINATION_TIME 10
#define MAX_PROCESS 18
#define PROCESS_SIZE 32000
#define PAGE_SIZE 1000
#define MAX_PAGE (PROCESS_SIZE / PAGE_SIZE)
#define MEMORY_SIZE 256000
#define FRAME_SIZE PAGE_SIZE
#define MAX_FRAME (MEMORY_SIZE / FRAME_SIZE)

typedef unsigned int uint;

typedef struct { // Shared memory clock for simulation
	unsigned int second;
	unsigned int nanosecond;
} SharedClock;

typedef struct { // Message queue
	long mtype;
	int index;
	pid_t childPid;
	int flag;
	unsigned int address;
	unsigned int requestPage;
	char message[BUFFER_LENGTH];
} Message;

typedef struct { // Struct for a page
	uint frameNo;
	uint address: 8;
	uint protection: 1; 
	uint dirty: 1;     
	uint valid: 1;      
} PageTableEntry; 

typedef struct { // Process control blocks
	int pidIndex;
	pid_t actualPid;
	PageTableEntry page_table[MAX_PAGE];
} ProcessControlBlock;

typedef struct NodeQ { // Nodes in queue
	int index;
	struct NodeQ *next;
} QNode; 


typedef struct { // Queue
	QNode *front;
	QNode *rear;
	int count;
} Queue;

typedef struct NodeL { // Nodes in list
	int index;
	int page;
	int frame;
	struct NodeL *next;
} LNode; 


typedef struct { // Lists
	LNode *front;
} List;

void createFile(char *); // File declarations
void logOutput(char*, char*, ...);
List *createList();
LNode *newLNode(int, int, int);
void addListElement(List *, int, int, int);
void deleteListFirst(List *);
int deleteListElement(List *, int, int, int);
bool isInList(List *, int);
Queue *createQueue(); // Queue block
QNode *newQNode(int);
void enQueue(Queue*, int);
QNode *deQueue(Queue *);
bool isQueueEmpty(Queue *);

#endif

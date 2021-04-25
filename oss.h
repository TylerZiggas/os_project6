#pragma once
#ifndef OSS_H
#define OSS_H

#include <stdlib.h>   
#include <stdio.h>    
#include <stdbool.h>    
#include <stdint.h>   
#include <string.h>    
#include <unistd.h>   
#include <stdarg.h>   
#include <errno.h>      
#include <signal.h>  
#include <sys/ipc.h> 
#include <sys/msg.h>   
#include <sys/shm.h>  
#include <sys/sem.h>   
#include <sys/time.h>   
#include <sys/types.h>  
#include <sys/wait.h>   
#include <time.h>      
#include <math.h>	


#define BUFFER_LENGTH 4096
#define MAX_FILE_LINE 100000



#define TERMINATION_TIME 20
#define MAX_PROCESS 18
#define TOTAL_PROCESS 100


#define PROCESS_SIZE 32000
#define PAGE_SIZE 1000
#define MAX_PAGE (PROCESS_SIZE / PAGE_SIZE)

#define MEMORY_SIZE 256000
#define FRAME_SIZE PAGE_SIZE
#define MAX_FRAME (MEMORY_SIZE / FRAME_SIZE)


typedef unsigned int uint;

// Shared memory
typedef struct 
{
	unsigned int second;
	unsigned int nanosecond;
}SharedClock;


typedef struct
{
	long mtype;
	int index;
	pid_t childPid;
	int flag;
	unsigned int address;
	unsigned int requestPage;
	char message[BUFFER_LENGTH];
}Message;



typedef struct
{
	uint frameNo;
	uint address: 8;
	uint protection: 1; 
	uint dirty: 1;     
	uint valid: 1;      
}PageTableEntry; 

typedef struct
{
	int pidIndex;
	pid_t actualPid;
	PageTableEntry page_table[MAX_PAGE];
}ProcessControlBlock;

//Queue.h
typedef struct NodeQ
{ 
	int index;
	struct NodeQ *next;
}QNode; 


typedef struct
{ 
	QNode *front;
	QNode *rear;
	int count;
}Queue; 


Queue *createQueue();
QNode *newQNode(int index);
void enQueue(Queue* q, int index);
QNode *deQueue(Queue *q);
bool isQueueEmpty(Queue *q);
int getQueueCount(Queue *q);
char *getQueue(const Queue *q);

//helper.h
char *strduplicate(const char *src);

//link list.h
typedef struct NodeL
{ 
	int index;
	int page;
	int frame;
	struct NodeL *next;
}LNode; 


typedef struct
{ 
	LNode *front;
}List;


List *createList();
LNode *newLNode(int index, int page, int frame);

void addListElement(List *l, int index, int page, int frame);
void deleteListFirst(List *l);
int deleteListElement(List *l, int index, int page, int frame);
bool isInList(List *l, int key);
char *getList(const List *l);



#endif

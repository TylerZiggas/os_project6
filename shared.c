#include "oss.h"

// helper.h
char *strduplicate(const char *src) 
{
	size_t len = strlen(src) + 1;       // String plus '\0'
	char *dst = malloc(len);            // Allocate space
	if (dst == NULL) return NULL;       // No memory
	memcpy (dst, src, len);             // Copy the block
	return dst;                         // Return the new string
}

// linklist.h
List *createList()
{
	List *l = (List *)malloc(sizeof(List));
	l->front = NULL;
	return l;
}


LNode *newLNode(int index, int page, int frame)
{ 
    LNode *temp = (LNode *)malloc(sizeof(LNode));
    temp->index = index;
	temp->page = page;
	temp->frame = frame;
    temp->next = NULL;
    return temp;
}


void addListElement(List *l, int index, int page, int frame)
{
	LNode *temp = newLNode(index, page, frame);

	if(l->front == NULL)
	{
		l->front = temp;
		return;
	}
	
	LNode *next = l->front;
	while(next->next != NULL)
	{
		next = next->next;
	}
	next->next = temp;
}



void deleteListFirst(List *l) 
{
    if(l->front == NULL)
    {
        return;
    }
    
    LNode *temp = l->front;
    l->front = l->front->next;
    free(temp);
}


int deleteListElement(List *l, int index, int page, int frame)
{
	LNode *current = l->front;
    LNode *previous = NULL;
    
    if(current == NULL)
    {
        return -1;
    }
    
    while(current->index != index || current->page != page || current->frame != frame)
    {
        if(current->next == NULL)
        {
            return -1;
        }
        else
        {
            previous = current;
            current = current->next;
        }
    }
    
    if(current == l->front)
    {
		int x = current->frame;
		free(current);
        l->front = l->front->next;
		return x;
    }
    else
    {
		int x = previous->next->frame;
		free(previous->next);
        previous->next = current->next;
		return x;
    }
}


bool isInList(List *l, int key) 
{
    LNode next;
    next.next = l->front;

    if(next.next == NULL) 
    {
        return false;
    }

    while(next.next->frame != key) 
    {
        if(next.next->next == NULL) 
        {
            return false;
        }
        else 
        {
            next.next = next.next->next;
        }
    }      
	
    return true;
}

char *getList(const List *l) 
{
	char buf[4096];
    LNode next;
    next.next = l->front;
    
    if(next.next == NULL) 
    {
        return strduplicate(buf);
    }
    
	sprintf(buf, "Linked List: ");
    while(next.next != NULL) 
    {
        sprintf(buf, "%s(%d | %d| %d)", buf, next.next->index, next.next->page, next.next->frame);
        
        next.next = (next.next->next != NULL) ? next.next->next : NULL;
		if(next.next != NULL)
		{
			sprintf(buf, "%s, ", buf);
		}
    }
	sprintf(buf, "%s\n", buf);

	return strduplicate(buf);
}

//queue.c

Queue *createQueue()
{
	Queue *q = (Queue *)malloc(sizeof(Queue));
	q->front = NULL;
	q->rear = NULL;
	q->count = 0;
	return q;
}


QNode *newQNode(int index)
{ 
    QNode *temp = (QNode *)malloc(sizeof(QNode));
    temp->index = index;
    temp->next = NULL;
    return temp;
} 


void enQueue(Queue *q, int index) 
{ 

	QNode *temp = newQNode(index);


	q->count = q->count + 1;


	if(q->rear == NULL)
	{
		q->front = q->rear = temp;
		return;
	}


	q->rear->next = temp;
	q->rear = temp;
}


QNode *deQueue(Queue *q) 
{

	if(q->front == NULL) 
	{
		return NULL;
	}


	QNode *temp = q->front;
	free(temp);
	q->front = q->front->next;


	if(q->front == NULL)
	{
		q->rear = NULL;
	}


	q->count = q->count - 1;
	return temp;
} 

bool isQueueEmpty(Queue *q)
{
	if(q->rear == NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}


int getQueueCount(Queue *q)
{
	return (q->count);	
}


char *getQueue(const Queue *q)
{
	char buf[4096];
	QNode next;
	next.next = q->front;

	sprintf(buf, "Queue: ");
	while(next.next != NULL)
	{
		sprintf(buf, "%s%d", buf, next.next->index);
		
		next.next = (next.next->next != NULL) ? next.next->next : NULL;
		if(next.next != NULL)
		{
			sprintf(buf, "%s, ", buf);
		}
	}
	sprintf(buf, "%s\n", buf);

	return strduplicate(buf);
}

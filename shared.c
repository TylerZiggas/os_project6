#include "oss.h"

void createFile(char* path) { // Creation of file
	FILE* newFile = fopen(path, "w"); // Take whatever char* was passed
		if (newFile == NULL) { // If there is a problem creating the file
			perror("Failed to touch file");
			exit(EXIT_FAILURE);
		}
	fclose(newFile); // Close file at end regardless
}

void logOutput(char* path, char* fmt, ...) {
	FILE* fp = fopen(path, "a+"); // Open a file for writing

	if (fp == NULL) { // In case of failed logging to file
		perror("Failed to open file for logging output");
		exit(EXIT_FAILURE);
	}

	int n = 4096; 
	char buf[n];
	va_list args; // Intialize to grab all arguments for logging
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);	
	fprintf(fp, buf); // Writing to the file 
	fclose(fp);
}

List *createList() { // Creation of our list
	List *l = (List *)malloc(sizeof(List));
	l->front = NULL;
	return l;
}

LNode *newLNode(int index, int page, int frame) { // Adding a new node
    LNode *temp = (LNode *)malloc(sizeof(LNode)); // allocate for it
    temp->index = index;
	temp->page = page;
	temp->frame = frame;
    temp->next = NULL; // next does not exist yet
    return temp;
}

void addListElement(List *l, int index, int page, int frame) { // adding an element to it
	LNode *temp = newLNode(index, page, frame);

	if(l->front == NULL) { // Check if the front is null
		l->front = temp;
		return;
	}
	
	LNode *next = l->front;
	while(next->next != NULL) { // Check if the next one is null
		next = next->next;
	}
	next->next = temp;
}

void deleteListFirst(List *l) { // Check front of the list
    	if(l->front == NULL) {
        	return;
    	}
    
    	LNode *temp = l->front;
    	l->front = l->front->next;
    	free(temp); // Free it up once done
}

int deleteListElement(List *l, int index, int page, int frame) { // Delete an element
	LNode *current = l->front;
    	LNode *previous = NULL;
    
    	if(current == NULL) { // If there is nothing in the list like that, return -1
        	return -1;
    	}
    
	while(current->index != index || current->page != page || current->frame != frame) {
		if(current->next == NULL) { // If nothing next, return -1
          		return -1;
        	} else { // Move list if we are good
            		previous = current;
            		current = current->next;
        	}
    	}
    
	if(current == l->front) { // If we are looking at the front of the list
		int x = current->frame;
		free(current);
        	l->front = l->front->next;
		return x;
    	} else { // If we are not looking at the front
		int x = previous->next->frame;
		free(previous->next);
        	previous->next = current->next;
		return x;
    }
}

bool isInList(List *l, int key) { // Make sure it is in the list or not
    	LNode next;
    	next.next = l->front;

    	if(next.next == NULL) { // If there is nothing  
        	return false; 
    	}

    	while(next.next->frame != key) { // Checking whether or not they match up
        	if(next.next->next == NULL) {
            		return false;
        	} else {
            		next.next = next.next->next;
        	}
    	}	      
    return true;
}

// Queue Block // 
Queue *createQueue() { // Initial creation of our queue
	Queue *q = (Queue *)malloc(sizeof(Queue)); // Allocate for it
	q->front = NULL;
	q->rear = NULL;
	q->count = 0; // initial count is 0 as we just intialized it
	return q;
}

QNode *newQNode(int index) { // Adding another node
    	QNode *temp = (QNode *)malloc(sizeof(QNode)); // Allocation of the node
    	temp->index = index;
    	temp->next = NULL;
    	return temp;
} 

void enQueue(Queue *q, int index) { // Queue another 
	QNode *temp = newQNode(index);
	q->count = q->count + 1;
	if(q->rear == NULL) { // Check to see if the queue is empty
		q->front = q->rear = temp;
		return;
	}

	q->rear->next = temp; // Otherwise add to next
	q->rear = temp;
}

QNode *deQueue(Queue *q) { // Removing from the queue
	if(q->front == NULL) { // Check if there is nothing on the front 
		return NULL;
	}

	QNode *temp = q->front;
	free(temp);
	q->front = q->front->next;

	if(q->front == NULL) { 
		q->rear = NULL;
	}

	q->count = q->count - 1; // Remove 1 from queue
	return temp;
} 

bool isQueueEmpty(Queue *q) { // Checker to see if the queue is empty
	if(q->rear == NULL) { // Simple check to see if it null
		return true;
	} else {
		return false;
	}
}

// Queue Block //

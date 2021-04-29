// Author: Tyler Ziggas
// Date: May 2021
// This is the file for the user processes as we figure out if the memory can be referenced/requested

#include "oss.h"

char *exe_name; // Some declarations for queues and exe
key_t key;
int exeIndex, mqueueid = -1;
Message user_message;

void processInterrupt(); // Declarations for handlers/interrupts and the message queue
void processHandler(int);
void resumeHandler(int);
void getMessageQueue();

int main(int argc, char *argv[]) {
	processInterrupt(); // Set up interrupts
	exe_name = argv[0];
	exeIndex = atoi(argv[1]);
	srand(getpid()); // Random based on our process id
	getMessageQueue(); // Grab our message queue

	bool toTerminate = false;
	int memoryRef = 0;
	unsigned int address = 0;
	unsigned int pageReq = 0;
	while(1) { // Infinite loop
		msgrcv(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), getpid(), 0);

		if(memoryRef <= 1000) { // Keep looping if we are not done
			address = rand() % 32768 + 0;
			pageReq = address >> 10;
			memoryRef++;
		} else { // If it is time to terminate the process
			toTerminate = true;
		}
		
		user_message.mtype = 1; // Set our message queue parts
		user_message.flag = (toTerminate) ? 0 : 1;
		user_message.address = address;
		user_message.requestPage = pageReq;
		msgsnd(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), 0);
	
		if(toTerminate) { // If we are to terminate, end
			break;
		}
	}
	exit(exeIndex); // Exit 
}


void processInterrupt() { // Creation of our process interrupt
	struct sigaction sa1; // Set up our sigaction
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &processHandler;
	sa1.sa_flags = SA_RESTART;
	if(sigaction(SIGUSR1, &sa1, NULL) == -1) { // Make sure it is made correctly
		perror("ERROR");
	}

	struct sigaction sa2; // Create another for control c
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &processHandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1) { // Make sure this is made correctly
		perror("ERROR");
	}
}

void processHandler(int signum) { // Process simply ends them
	exit(2);
}

void getMessageQueue() { // ALlocate our message queue
	key = ftok("./oss.c", 1);
	mqueueid = msgget(key, 0600);
	if(mqueueid < 0) { // Make sure it was made correctly
		fprintf(stderr, "%s ERROR: could not get [message queue] shared memory! Exiting...\n", exe_name);
		exit(EXIT_FAILURE);
	}
}

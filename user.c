// Author: Tyler Ziggas
// Date: May 2021

#include "oss.h"

char *exe_name;
key_t key;

int exeIndex, mqueueid = -1;
Message user_message;

void processInterrupt();
void processHandler(int);
void resumeHandler(int);
void getMessageQueue();

int main(int argc, char *argv[]) {
	processInterrupt();
	exe_name = argv[0];
	exeIndex = atoi(argv[1]);
	srand(getpid());
	getMessageQueue();

	bool toTerminate = false;
	int memoryRef = 0;
	unsigned int address = 0;
	unsigned int pageReq = 0;
	while(1) {
		msgrcv(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), getpid(), 0);

		if(memoryRef <= 1000) {
			address = rand() % 32768 + 0;
			pageReq = address >> 10;
			memoryRef++;
		} else {
			toTerminate = true;
		}
		
		user_message.mtype = 1;
		user_message.flag = (toTerminate) ? 0 : 1;
		user_message.address = address;
		user_message.requestPage = pageReq;
		msgsnd(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), 0);
	
		if(toTerminate) {
			break;
		}
	}

	exit(exeIndex);
}


void processInterrupt() {
	struct sigaction sa1;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &processHandler;
	sa1.sa_flags = SA_RESTART;
	if(sigaction(SIGUSR1, &sa1, NULL) == -1) {
		perror("ERROR");
	}

	struct sigaction sa2;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &processHandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1) {
		perror("ERROR");
	}
}

void processHandler(int signum) {
	exit(2);
}

void getMessageQueue() {
	key = ftok("./oss.c", 1);
	mqueueid = msgget(key, 0600);
	if(mqueueid < 0) {
		fprintf(stderr, "%s ERROR: could not get [message queue] shared memory! Exiting...\n", exe_name);
		exit(EXIT_FAILURE);
	}
}

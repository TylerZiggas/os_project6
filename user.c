// Author: Tyler Ziggas
// Date: May 2021

#include "oss.h"

char *exe_name;
key_t key;

int exe_index, mqueueid = -1, shmclock_shmid = -1, semid = -1, pcbt_shmid = -1;
Message user_message;
SharedClock *shmclock_shmptr = NULL;
ProcessControlBlock *pcbt_shmptr = NULL;

void processInterrupt();
void processHandler(int);
void resumeHandler(int);
void discardShm(void *, char *, char *, char *);
void cleanUp();
void getSharedMemory();

int main(int argc, char *argv[]) {
	processInterrupt();
	exe_name = argv[0];
	exe_index = atoi(argv[1]);
	srand(getpid());
	getSharedMemory();

	bool is_terminate = false;
	int memory_reference = 0;
	unsigned int address = 0;
	unsigned int request_page = 0;
	while(1) {
		msgrcv(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), getpid(), 0);

		if(memory_reference <= 1000) {

			address = rand() % 32768 + 0;
			request_page = address >> 10;
			memory_reference++;
		} else {
			is_terminate = true;
		}
		
		user_message.mtype = 1;
		user_message.flag = (is_terminate) ? 0 : 1;
		user_message.address = address;
		user_message.requestPage = request_page;
		msgsnd(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), 0);
	
		if(is_terminate) {
			break;
		}
	}

	cleanUp();
	exit(exe_index);
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
	cleanUp();
	exit(2);
}

void discardShm(void *shmaddr, char *shm_name , char *exe_name, char *process_type) {
	if(shmaddr != NULL) {
		if((shmdt(shmaddr)) << 0) {
			fprintf(stderr, "%s (%s) ERROR: could not detach [%s] shared memory!\n", exe_name, process_type, shm_name);
		}
	}
}

void getSharedMemory() {
	key = ftok("./oss.c", 1);
	mqueueid = msgget(key, 0600);
	if(mqueueid < 0) {
		fprintf(stderr, "%s ERROR: could not get [message queue] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}

	key = ftok("./oss.c", 2);
	shmclock_shmid = shmget(key, sizeof(SharedClock), 0600);
	if(shmclock_shmid < 0) {
		fprintf(stderr, "%s ERROR: could not get [shmclock] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}


	shmclock_shmptr = shmat(shmclock_shmid, NULL, 0);
	if(shmclock_shmptr == (void *)( -1 )) {
		fprintf(stderr, "%s ERROR: fail to attach [shmclock] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);	
	}

	key = ftok("./oss.c", 3);
	semid = semget(key, 1, 0600);
	if(semid == -1) {
		fprintf(stderr, "%s ERROR: fail to attach a private semaphore! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}

	key = ftok("./oss.c", 4);
	size_t process_table_size = sizeof(ProcessControlBlock) * MAX_PROCESS;
	pcbt_shmid = shmget(key, process_table_size, 0600);
	if(pcbt_shmid < 0) {
		fprintf(stderr, "%s ERROR: could not get [pcbt] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}

	pcbt_shmptr = shmat(pcbt_shmid, NULL, 0);
	if(pcbt_shmptr == (void *)( -1 )) {
		fprintf(stderr, "%s ERROR: fail to attach [pcbt] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);	
	}
}

void cleanUp() {
        discardShm(shmclock_shmptr, "shmclock", exe_name, "Child");
        discardShm(pcbt_shmptr, "pcbt", exe_name, "Child");
}


// Author: Tyler Ziggas
// Date: May 2021
// The purpose of this project is to simulate memory management using the FIFO algorithm
// There are 3 parmeters you can use, -h for help, -l to change the logfile name, and -p to change the number of processes

#include "oss.h"

char* logfile = "logfile"; // Default logfile name
key_t key;
Queue *queue;
SharedClock forkclock;
int mqueueid = -1, shmclock_shmid = -1, semid = -1, pcbt_shmid = -1; // ID declarations
int forkNumber = 0, memAccess = 0, pagefaultNumber = 0, lastFrame = -1, numOfProcesses = 20; // Global varaibles
Message message; // Shared declarations
SharedClock *shmclock_shmptr = NULL;
struct sembuf sema_operation;
ProcessControlBlock *pcbt_shmptr = NULL;
pid_t pid = -1;
unsigned int totalAccess = 0;
unsigned char bitmap[MAX_PROCESS];
unsigned char mainMemory[MAX_FRAME];
List *refString;

void masterInterrupt(int); // Declarations for our handlers and timers
void masterHandler(int);
void segHandler(int);
void exitHandler(int);
void timer(int);
void finalize(); // Declarations for cleaning up
void cleanUp();
void discardShm(int, void *, char *, char *);
void semaLock(int); // Declarations for semaphores
void semaRelease(int);
int incShmclock(int);
void initPCBT(ProcessControlBlock *); // Declarations for process control block
void initPCB(ProcessControlBlock *, int, pid_t);

int main(int argc, char *argv[]) {
	srand(getpid()); // Set up how we do random using our pid
	bool allDigit = true; 
	int character, optargCount, forkCounter = 0;
	while((character = getopt(argc, argv, "hl:p:")) != -1) { // Switch for our options for the project
		switch(character) {
			case 'h': // In case the user needs help
				printf("./oss - simulate the memory management module of FIFO page replacement algorithm with dirty-bit optimization.\n");
				printf("USE BY: ./oss [-h] [-l logfile] [-p processes].\n");
				printf("-h           : print the help page and exit.\n");
				printf("-l filename  : the log file used (default is logfile).\n");
				printf("-p processes : number of processes (between 20 and 40)\n");
				exit(EXIT_SUCCESS);
			case 'l': // In case of changing the logfile name
				logfile = optarg;
				printf("Your new log file is: %s\n", logfile); // Tell them what it is now
				break;
			case 'p': // In case we change the number of user processes at maximum
				allDigit = true;
				for (optargCount = 0; optargCount < strlen(optarg); optargCount++) { // Check if entire optarg is digit
					if (!isdigit(optarg[optargCount])) { // If it is not a digit, this isnt valid
						allDigit = false;
						break;
					}
				}		
				if (allDigit) { // After that, check if it meets requirements to change max
					int testDigit = atoi(optarg);
					if (20 <= testDigit) { // Check to make sure it is in our range
						if (testDigit <= 40) { 
							numOfProcesses = atoi(optarg);
						} else { // Print if it is not in the amount needed
							printf("Must be between the default of 20 and the maximum of 40\n");
						}
					} else {
						printf("Must be between the default of 20 and the maximum of 40\n");
					}
				} else { // If it doesn't work at all, exit
					errno = 22;
					perror("-s requires a valid argument");
					printf("Please see the help menu for more information\n");
					return EXIT_FAILURE;
				}
				continue;
			default: // In case user inputs something we do not allow
				printf("Please use \"-h\" option for more info.\n");
				exit(EXIT_FAILURE);
		}
	}

	createFile(logfile); // Create our file
	memset(bitmap, '\0', sizeof(bitmap));
	key = ftok("./oss.c", 1); // Allocation of our memory queue
	mqueueid = msgget(key, IPC_CREAT | 0600);
	if(mqueueid < 0) {
		fprintf(stderr, "ERROR: could not allocate\n");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	key = ftok("./oss.c", 2); // Allocation of our shared clock
	shmclock_shmid = shmget(key, sizeof(SharedClock), IPC_CREAT | 0600);
	if(shmclock_shmid < 0) {
		fprintf(stderr, "ERROR: could not allocate\n");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	shmclock_shmptr = shmat(shmclock_shmid, NULL, 0);
	if(shmclock_shmptr == (void *)( -1 )) { // Attaching our shared clock
		fprintf(stderr, "ERROR: failed to attach\n");
		cleanUp();
		exit(EXIT_FAILURE);	
	}

	shmclock_shmptr->second = 0; // Set our times to a default of 0 and add from there
	shmclock_shmptr->nanosecond = 0;
	forkclock.second = 0;
	forkclock.nanosecond = 0;

	key = ftok("./oss.c", 3); // Creation of our semaphore
	semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);
	if(semid == -1) {
		fprintf(stderr, "ERROR: failed to create semaphores\n");
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	semctl(semid, 0, SETVAL, 1);
	key = ftok("./oss.c", 4); // Creation of our process table
	size_t processTableSize = sizeof(ProcessControlBlock) * MAX_PROCESS;
	pcbt_shmid = shmget(key, processTableSize, IPC_CREAT | 0600);
	if(pcbt_shmid < 0) { // Allocate our process control block
		fprintf(stderr, "ERROR: could not allocate\n");
		cleanUp();
		exit(EXIT_FAILURE);
	}

	pcbt_shmptr = shmat(pcbt_shmid, NULL, 0);
	if(pcbt_shmptr == (void *)( -1 )) { // Attaching our process control block
		fprintf(stderr, "ERROR: failed to attach\n");
		cleanUp();
		exit(EXIT_FAILURE);	
	}

	initPCBT(pcbt_shmptr); // Set it up
	queue = createQueue(); // Create our queue
	refString = createList(); // Create our list
	masterInterrupt(TERMINATION_TIME); // Set up our interrupts

	printf("\nSimulating FIFO Page Replacement Algorithm...\n"); // Start our simulation

	int lastIndex = -1;
	while(1) { // Infinite loop for our process and algorithm
		int spawn_nano = rand() % 500000000 + 1000000;
		if(forkclock.nanosecond >= spawn_nano) { 
			forkclock.nanosecond = 0;

			bool isBitmapOpen = false;
			int countProcess = 0;
			while(1) { // Infinite loop for bitmap
				lastIndex = (lastIndex + 1) % MAX_PROCESS;
				uint32_t bit = bitmap[lastIndex / 8] & (1 << (lastIndex % 8));
				if(bit == 0) { // Open the bitmap
					isBitmapOpen = true;
					break;
				}

				if(countProcess >= MAX_PROCESS - 1) { // Make sure number of processes isn't too high
					break;
				}
				countProcess++;
			}

			if(isBitmapOpen == true && forkCounter < numOfProcesses) { // Make sure bitmap is open and we havent hit the max processes
				forkCounter++; // Add to our counter
				pid = fork();
				if(pid == -1) { //Display an error if the process wasn't spawned correctly, then get out
					fprintf(stderr, "(Master) ERROR: %s\n", strerror(errno));
					finalize();
					cleanUp();
					exit(0);
				}
		
				if(pid == 0) { // Child processes
					signal(SIGUSR1, exitHandler);
					char execIndex[BUFFER_LENGTH];
					sprintf(execIndex, "%d", lastIndex);
					int exectStatus = execl("./get_page", "./get_page", execIndex, NULL); // Exec the process to our page getter
					if(exectStatus == -1) { // Make sure we didnt fail to exec
						fprintf(stderr, "(Child) ERROR: execl fail to execute at index [%d]! Exiting...\n", lastIndex);
					}
				
					finalize(); // Clean up and exit
					cleanUp();
					exit(EXIT_FAILURE);
				} else { // Master process
					forkNumber++; // Update the number forked
					bitmap[lastIndex / 8] |= (1 << (lastIndex % 8)); 
					initPCB(&pcbt_shmptr[lastIndex], lastIndex, pid); // Intialize the process control block
					enQueue(queue, lastIndex); // Queue it up

					// Display that the process is being generated
					logOutput(logfile, "Generating process with PID (%d) [%d] and putting it in queue at time %d.%d\n", 
						pcbt_shmptr[lastIndex].pidIndex, pcbt_shmptr[lastIndex].actualPid, shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				}
			}
		}

		incShmclock(0); // Update our shared clock
		QNode qnext;
		Queue *trackingQueue = createQueue(); // Create our tracking queue
		qnext.next = queue->front;
		while(qnext.next != NULL) { // Check if another is left in the queue
			incShmclock(0); // Update our clock

			int cindex = qnext.next->index;
			message.mtype = pcbt_shmptr[cindex].actualPid; // Put message queue information in
			message.index = cindex;
			message.childPid = pcbt_shmptr[cindex].actualPid;
			msgsnd(mqueueid, &message, (sizeof(Message) - sizeof(long)), 0); // Send our message queue
			msgrcv(mqueueid, &message, (sizeof(Message) - sizeof(long)), 1, 0);
			incShmclock(0); // Update our clock

			if(message.flag == 0) { // Display if process is finished running
				logOutput(logfile, "Process with PID (%d) [%d] has finish running at my time %d.%d\n",
					message.index, message.childPid, shmclock_shmptr->second, shmclock_shmptr->nanosecond);

				int i;
				for(i = 0; i < MAX_PAGE; i++) { // Fix our list element 
					if(pcbt_shmptr[cindex].page_table[i].frameNo != -1) {
						int frame = pcbt_shmptr[cindex].page_table[i].frameNo;
						deleteListElement(refString, cindex, i, frame);
						mainMemory[frame / 8] &= ~(1 << (frame % 8));
					}
				}
			} else { // If we are not finished processing
				totalAccess += incShmclock(0); // Add to our total time
				enQueue(trackingQueue, cindex); // Queue this up
	
				unsigned int address = message.address;
				unsigned int requestPage = message.requestPage;
				if(pcbt_shmptr[cindex].page_table[requestPage].protection == 0) { // Log our process read request
 					logOutput(logfile, "Process (%d) [%d] requesting read of address (%d) [%d] at time %d:%d\n", 
						message.index, message.childPid, 
						address, requestPage,
						shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				} else { // Log our process write request
					logOutput(logfile, "Process (%d) [%d] requesting write of address (%d) [%d] at time %d:%d\n", 
						message.index, message.childPid, 
						address, requestPage,
						shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				}
				memAccess++; // Increment our memory access

				if(pcbt_shmptr[cindex].page_table[requestPage].valid == 0) { // In case of a page fault
 					logOutput(logfile, "Address (%d) [%d] is not in a frame, pagefault\n",
						address, requestPage);
					
					pagefaultNumber++; // Add to our overall page faults
					totalAccess += incShmclock(14000000); // Add to our simulated time
					bool isMemoryOpen = false;
					int countFrame = 0;
					while(1) { // Loop to see if the memory is open
						lastFrame = (lastFrame + 1) % MAX_FRAME;
						uint32_t frame = mainMemory[lastFrame / 8] & (1 << (lastFrame % 8));
						if(frame == 0) { // Check if memory is open
							isMemoryOpen = true;
							break;
						}

						if(countFrame >= MAX_FRAME - 1) {
							break;
						}
						countFrame++;
					}

					if(isMemoryOpen == true) { // We are good to go
						pcbt_shmptr[cindex].page_table[requestPage].frameNo = lastFrame;
						pcbt_shmptr[cindex].page_table[requestPage].valid = 1;
						mainMemory[lastFrame / 8] |= (1 << (lastFrame % 8));
						addListElement(refString, cindex, requestPage, lastFrame);
						logOutput(logfile, "Allocated frame [%d] to PID (%d) [%d]\n",
							lastFrame, message.index, message.childPid); // Allocating our frame

						if(pcbt_shmptr[cindex].page_table[requestPage].protection == 0) { // Giving the data
							logOutput(logfile, "Address (%d) [%d] in frame (%d), giving data to process (%d) [%d] at time %d:%d\n",
								address, requestPage, 
								pcbt_shmptr[cindex].page_table[requestPage].frameNo,
								message.index, message.childPid,
								shmclock_shmptr->second, shmclock_shmptr->nanosecond);

							pcbt_shmptr[cindex].page_table[requestPage].dirty = 0;
						} else { // If we are instead writing to frame
							logOutput(logfile, "Address (%d) [%d] in frame (%d), writing data to frame at time %d:%d\n",
								address, requestPage, 
								pcbt_shmptr[cindex].page_table[requestPage].frameNo,
								shmclock_shmptr->second, shmclock_shmptr->nanosecond);

							pcbt_shmptr[cindex].page_table[requestPage].dirty = 1;
						}
					} else { // In case the memory is currently full
 						logOutput(logfile, "Address (%d) [%d] is not in a frame, memory is full. Invoking page replacement...\n", address, requestPage);

						unsigned int fifoIndex = refString->front->index;
						unsigned int fifoPage = refString->front->page;
						unsigned int fifoAddress = fifoPage << 10;
						unsigned int fifoFrame = refString->front->frame;
						if(pcbt_shmptr[fifoIndex].page_table[fifoPage].dirty == 1) { // In case we have to change the address
							logOutput(logfile, "Address (%d) [%d] was modified. Modified information is written back to the disk\n", 
										fifoAddress, fifoPage);
						}

						pcbt_shmptr[fifoIndex].page_table[fifoPage].frameNo = -1; // Update our indexs and pcbt information
						pcbt_shmptr[fifoIndex].page_table[fifoPage].dirty = 0;
						pcbt_shmptr[fifoIndex].page_table[fifoPage].valid = 0;
						pcbt_shmptr[cindex].page_table[requestPage].frameNo = fifoFrame;
						pcbt_shmptr[cindex].page_table[requestPage].dirty = 0;
						pcbt_shmptr[cindex].page_table[requestPage].valid = 1;
							
						deleteListFirst(refString); // Get rid first on list and add a new element
						addListElement(refString, cindex, requestPage, fifoFrame);

						if(pcbt_shmptr[cindex].page_table[requestPage].protection == 1) { // Check for the dirty bit
							logOutput(logfile, "Dirty bit of frame (%d) set, adding additional time to the clock\n", lastFrame);
							logOutput(logfile, "Indicating to process (%d) [%d] that write has happend to address (%d) [%d]\n", 
								message.index, message.childPid, address, requestPage);
							pcbt_shmptr[cindex].page_table[requestPage].dirty = 1;
						}
					}
				} else { 
					if(pcbt_shmptr[cindex].page_table[requestPage].protection == 0) { // If the frame is already in use for the address
						logOutput(logfile, "Address (%d) [%d] is already in frame (%d), giving data to process (%d) [%d] at time %d:%d\n",
							address, requestPage, 
							pcbt_shmptr[cindex].page_table[requestPage].frameNo,
							message.index, message.childPid,
							shmclock_shmptr->second, shmclock_shmptr->nanosecond);
					} else { // In case the address is in a frame
						logOutput(logfile, "Address (%d) [%d] is already in frame (%d), writing data to frame at time %d:%d\n",
							address, requestPage, 
							pcbt_shmptr[cindex].page_table[requestPage].frameNo,
							shmclock_shmptr->second, shmclock_shmptr->nanosecond);
					}
				}
			}

			qnext.next = (qnext.next->next != NULL) ? qnext.next->next : NULL; // Setting up our next message queue
			message.mtype = -1;
			message.index = -1;
			message.childPid = -1;
			message.flag = -1;
			message.requestPage = -1;
		}

		while(!isQueueEmpty(queue)) { // Empty the queue
			deQueue(queue);
		}

		while(!isQueueEmpty(trackingQueue)) { // Empty the tracking queue
			int i = trackingQueue->front->index;
			enQueue(queue, i);
			deQueue(trackingQueue);
		}

		free(trackingQueue); // Free our queue
		incShmclock(0);
		int child_status = 0;
		pid_t child_pid = waitpid(-1, &child_status, WNOHANG);

		if(child_pid > 0) { // Check child pid
			int return_index = WEXITSTATUS(child_status);
			bitmap[return_index / 8] &= ~(1 << (return_index % 8));
		}

		if(forkCounter >= numOfProcesses) { // If we reach max, we just want to end
			timer(0);
			masterHandler(0);
		}
	}
	return EXIT_SUCCESS; 
}

void masterInterrupt(int seconds) { // Set up our timers
	timer(seconds); // Set up our timer
	struct sigaction sa1;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &masterHandler;
	sa1.sa_flags = SA_RESTART;
	if(sigaction(SIGALRM, &sa1, NULL) == -1) { //  Make sure it is set up correctly
		perror("ERROR");
	}

	struct sigaction sa2; // Set up our control c interrupt
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &masterHandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1) { // Make sure this is set up correctly
		perror("ERROR");
	}

	signal(SIGUSR1, SIG_IGN); // Setting up of the signals
	signal(SIGSEGV, segHandler);
}

void masterHandler(int signum) {
	finalize(); // Finalize our process
	double memPerSecond = (double)memAccess / (double)shmclock_shmptr->second; // Calculate the results we need
	double pagefaultPerAccess = (double)pagefaultNumber / (double)memAccess;
	double avgMem = (double)totalAccess / (double)memAccess;
	avgMem /= 1000000.0;

	printf("OSS PID: %d\n", getpid()); // Printing our results for the user
      	printf("Number of forking during this execution: %d\n", forkNumber);
        printf("Final simulation time of this execution: %d.%d\n", shmclock_shmptr->second, shmclock_shmptr->nanosecond);
        printf("Number of memory accesses per nanosecond: %f memory/second\n", memPerSecond);
	printf("Number of page faults per memory access: %f pagefault/access\n", pagefaultPerAccess);
        printf("Average memory access speed: %f ms/n\n", avgMem);
        printf("Total memory access time: %f ms\n", (double)totalAccess / 1000000.0);	

	logOutput(logfile, "OSS PID: %d\n", getpid()); // Logging our results for the user after the process is done
	logOutput(logfile, "Number of forking during this execution: %d\n", forkNumber);
	logOutput(logfile, "Final simulation time of this execution: %d.%d\n", shmclock_shmptr->second, shmclock_shmptr->nanosecond);
	logOutput(logfile, "Number of memory accesses per nanosecond: %f memory/second\n", memPerSecond);
	logOutput(logfile, "Number of page faults per memory access: %f pagefault/access\n", pagefaultPerAccess);
	logOutput(logfile, "Average memory access speed: %f ms/n\n", avgMem);
	logOutput(logfile, "Total memory access time: %f ms\n", (double)totalAccess / 1000000.0);
	
	printf("More information can be found in our log file named: %s\n\n", logfile);
	cleanUp(); // Clean up our resources
	exit(EXIT_SUCCESS); 
}

void segHandler(int signum) { // Handler in case of a segmentation fault
	fprintf(stderr, "Segmentation Fault\n");
	masterHandler(0);
}

void exitHandler(int signum) { // Handler for our process
	fprintf(stderr, "%d: Terminated!\n", getpid());
	exit(EXIT_SUCCESS);
}


void timer(int seconds) { // Setting up of our timer
	struct itimerval value;
	value.it_value.tv_sec = seconds;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	
	if(setitimer(ITIMER_REAL, &value, NULL) == -1) { // Make sure it is set up correctly
		perror("ERROR");
	}
}

void finalize() { // Finalize and fully kill our processes
	fprintf(stderr, "\nTerminating...\n");
	kill(0, SIGUSR1); // Kill the process
	pid_t p = 0;
	while(p >= 0) {
		p = waitpid(-1, NULL, WNOHANG);
	}
}

void discardShm(int shmid, void *shmaddr, char *shm_name, char *process_type) { // Discarding our shared memory
	if(shmaddr != NULL) {
		if((shmdt(shmaddr)) << 0) { // Deatach the shared memory
			fprintf(stderr, "(%s) ERROR: could not detach [%s] shared memory!\n", process_type, shm_name);
		}
	}
	
	if(shmid > 0) {
		if((shmctl(shmid, IPC_RMID, NULL)) < 0) { // Delete the shared memory
			fprintf(stderr, "(%s) ERROR: could not delete [%s] shared memory! Exiting...\n", process_type, shm_name);
		}
	}
}

void cleanUp() { // Cleaning up our resources
	if(mqueueid > 0) { // Clean up our message queue
		msgctl(mqueueid, IPC_RMID, NULL);
	}

	discardShm(shmclock_shmid, shmclock_shmptr, "shmclock", "Master");

	if(semid > 0) { // Clean up our semaphore 
		semctl(semid, 0, IPC_RMID);
	}

	discardShm(pcbt_shmid, pcbt_shmptr, "pcbt", "Master"); // Clean up our shared memory
}

void semaLock(int sem_index) { // Function for locking our semaphore
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = -1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}

void semaRelease(int sem_index) { // Function for releasing our semaphore
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = 1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}

int incShmclock(int increment) { // Incrementing our memory clock
	semaLock(0); // Lock our semaphore
	int nano = (increment > 0) ? increment : rand() % 1000 + 1; // Randomize our time added

	forkclock.nanosecond += nano; // Add our nano to both
	shmclock_shmptr->nanosecond += nano;

	while(shmclock_shmptr->nanosecond >= 1000000000) { // Correct our new time and add seconds if we need it
		shmclock_shmptr->second++;
		shmclock_shmptr->nanosecond = abs(1000000000 - shmclock_shmptr->nanosecond);
	}

	semaRelease(0);
	return nano;
}

void initPCBT(ProcessControlBlock *pcbt) { // Initialize our process control block table
	int i, j;
	for(i = 0; i < MAX_PROCESS; i++) {
		pcbt[i].pidIndex = -1;
		pcbt[i].actualPid = -1;
		for(j = 0; j < MAX_PAGE; j++) { // loop for hte table to create it
			pcbt[i].page_table[j].frameNo = -1;
			pcbt[i].page_table[j].protection = rand() % 2;
			pcbt[i].page_table[j].dirty = 0;
			pcbt[i].page_table[j].valid = 0;
		}
	}		
}

void initPCB(ProcessControlBlock *pcb, int index, pid_t pid) { // Intialize our process control block
	int i;
	pcb->pidIndex = index;
	pcb->actualPid = pid;

	for(i = 0; i < MAX_PAGE; i++) { // Loop for our block to create it
		pcb->page_table[i].frameNo = -1;
		pcb->page_table[i].protection = rand() % 2;
		pcb->page_table[i].dirty = 0;
		pcb->page_table[i].valid = 0;
	}
}

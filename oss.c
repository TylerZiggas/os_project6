#include "oss.h"

static FILE *fpw = NULL;
static char *exe_name;
static int percentage = 0;
static char log_file[256] = "fifolog.dat";
static char isDebugMode = false;
static char isDisplayTerminal = false;
static int algorithm_choice = 0;
static key_t key;
static Queue *queue;
static SharedClock forkclock;

static int mqueueid = -1;
static Message master_message;
static int shmclock_shmid = -1;
static SharedClock *shmclock_shmptr = NULL;
static int semid = -1;
static struct sembuf sema_operation;
static int pcbt_shmid = -1;
static ProcessControlBlock *pcbt_shmptr = NULL;

static int fork_number = 0;
static pid_t pid = -1;
static unsigned char bitmap[MAX_PROCESS];

static int memoryaccess_number = 0;
static int pagefault_number = 0;
static unsigned int total_access_time = 0;
static unsigned char main_memory[MAX_FRAME];
static int last_frame = -1;
static List *reference_string;
static List *lru_stack;

void printWrite(FILE *fpw, char *fmt, ...);
void masterInterrupt(int seconds);
void masterHandler(int signum);
void segHandler(int signum);
void exitHandler(int signum);
void timer(int seconds);
void finalize();
void discardShm(int shmid, void *shmaddr, char *shm_name , char *exe_name, char *process_type);
void cleanUp();
void semaLock(int sem_index);
void semaRelease(int sem_index);
int incShmclock(int increment);

void initPCBT(ProcessControlBlock *pcbt);
char *getPCBT(ProcessControlBlock *pcbt);
void initPCB(ProcessControlBlock *pcb, int index, pid_t pid);

int main(int argc, char *argv[]) 
{

	exe_name = argv[0];
	srand(getpid());

	int character;
	while((character = getopt(argc, argv, "hl:dt")) != -1) {
		switch(character) {
			case 'h':
				printf("NAME:\n");
				printf("	%s - simulate the memory management module and compare LRU and FIFO page replacement algorithms, both with dirty-bit optimization.\n", exe_name);
				printf("\nUSAGE:\n");
				printf("	%s [-h] [-l logfile] [-a choice].\n", exe_name);
				printf("\nDESCRIPTION:\n");
				printf("	-h           : print the help page and exit.\n");
				printf("	-l filename  : the log file used (default is fifolog.dat).\n");
				printf("	-d           : turn on debug mode (default is off). Beware, result will be different and inconsistent.\n");
				printf("	-t           : display result in the terminal as well (default is off).\n");
				exit(EXIT_SUCCESS);

			case 'l':
				strncpy(log_file, optarg, 255);
				fprintf(stderr, "Your new log file is: %s\n", log_file);
				break;

			case 'd':
				isDebugMode = true; 
				break;

			case 't':
				isDisplayTerminal = true;
				break;
			default:
				fprintf(stderr, "%s: please use \"-h\" option for more info.\n", exe_name);
				exit(EXIT_FAILURE);
		}
	}

	fpw = fopen(log_file, "w");
	if(fpw == NULL) {
		fprintf(stderr, "%s ERROR: unable to write the output file.\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	memset(bitmap, '\0', sizeof(bitmap));

	key = ftok("./oss.c", 1);
	mqueueid = msgget(key, IPC_CREAT | 0600);
	if(mqueueid < 0) {
		fprintf(stderr, "%s ERROR: could not allocate [message queue] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}

	key = ftok("./oss.c", 2);
	shmclock_shmid = shmget(key, sizeof(SharedClock), IPC_CREAT | 0600);
	if(shmclock_shmid < 0) {
		fprintf(stderr, "%s ERROR: could not allocate [shmclock] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}

	shmclock_shmptr = shmat(shmclock_shmid, NULL, 0);
	if(shmclock_shmptr == (void *)( -1 )) {
		fprintf(stderr, "%s ERROR: fail to attach [shmclock] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);	
	}

	shmclock_shmptr->second = 0;
	shmclock_shmptr->nanosecond = 0;
	forkclock.second = 0;
	forkclock.nanosecond = 0;

	key = ftok("./oss.c", 3);
	semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);
	if(semid == -1) {
		fprintf(stderr, "%s ERROR: failed to create a new private semaphore! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	semctl(semid, 0, SETVAL, 1);	
	
	key = ftok("./oss.c", 4);
	size_t process_table_size = sizeof(ProcessControlBlock) * MAX_PROCESS;
	pcbt_shmid = shmget(key, process_table_size, IPC_CREAT | 0600);
	if(pcbt_shmid < 0) {
		fprintf(stderr, "%s ERROR: could not allocate [pcbt] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);
	}

	pcbt_shmptr = shmat(pcbt_shmid, NULL, 0);
	if(pcbt_shmptr == (void *)( -1 )) {
		fprintf(stderr, "%s ERROR: fail to attach [pcbt] shared memory! Exiting...\n", exe_name);
		cleanUp();
		exit(EXIT_FAILURE);	
	}

	initPCBT(pcbt_shmptr);

	queue = createQueue();
	reference_string = createList();
	if(algorithm_choice == 1) {
		lru_stack = createList();
	}

	masterInterrupt(TERMINATION_TIME);

	if(!isDisplayTerminal) {
		fprintf(stderr, "Simulating...\n");
	}
	if(isDebugMode) {
		fprintf(stderr, "DEBUG mode is ON\n");
	}
	fprintf(stderr, "Using First In First Out (FIFO) algorithm.\n");

	int last_index = -1;
	while(1) { 
		int spawn_nano = (isDebugMode) ? 100 : rand() % 500000000 + 1000000;
		if(forkclock.nanosecond >= spawn_nano) {
			forkclock.nanosecond = 0;

			bool is_bitmap_open = false;
			int count_process = 0;
			while(1) {
				last_index = (last_index + 1) % MAX_PROCESS;
				uint32_t bit = bitmap[last_index / 8] & (1 << (last_index % 8));
				if(bit == 0) {
					is_bitmap_open = true;
					break;
				}

				if(count_process >= MAX_PROCESS - 1) {
					break;
				}
				count_process++;
			}

			if(is_bitmap_open == true) {
				pid = fork();

				if(pid == -1) {
					fprintf(stderr, "%s (Master) ERROR: %s\n", exe_name, strerror(errno));
					finalize();
					cleanUp();
					exit(0);
				}
		
				if(pid == 0) {
					signal(SIGUSR1, exitHandler);
					char exec_index[BUFFER_LENGTH];
					sprintf(exec_index, "%d", last_index);
					int exect_status = execl("./get_page", "./get_page", exec_index, NULL);
					if(exect_status == -1) {	
						fprintf(stderr, "%s (Child) ERROR: execl fail to execute at index [%d]! Exiting...\n", exe_name, last_index);
					}
				
					finalize();
					cleanUp();
					exit(EXIT_FAILURE);
				} else {	
				
					if(!isDisplayTerminal && (fork_number == percentage)) {
						if(fork_number < TOTAL_PROCESS - 1 || TOTAL_PROCESS % 2 != 1) {
							fprintf(stderr, "%c%c%c%c%c", 219, 219, 219, 219, 219);
						}
						percentage += (int)(ceil(TOTAL_PROCESS * 0.1));
					}
					fork_number++;
					bitmap[last_index / 8] |= (1 << (last_index % 8));
					initPCB(&pcbt_shmptr[last_index], last_index, pid);
					enQueue(queue, last_index);

					printWrite(fpw, "%s: generating process with PID (%d) [%d] and putting it in queue at time %d.%d\n", exe_name, 
						pcbt_shmptr[last_index].pidIndex, pcbt_shmptr[last_index].actualPid, shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				}
			}
		}

		incShmclock(0);
		QNode qnext;
		Queue *trackingQueue = createQueue();
		qnext.next = queue->front;
		while(qnext.next != NULL) {
			incShmclock(0);

			int c_index = qnext.next->index;
			master_message.mtype = pcbt_shmptr[c_index].actualPid;
			master_message.index = c_index;
			master_message.childPid = pcbt_shmptr[c_index].actualPid;
			msgsnd(mqueueid, &master_message, (sizeof(Message) - sizeof(long)), 0);
			msgrcv(mqueueid, &master_message, (sizeof(Message) - sizeof(long)), 1, 0);

			incShmclock(0);

			if(master_message.flag == 0) {
				printWrite(fpw, "%s: process with PID (%d) [%d] has finish running at my time %d.%d\n",
					exe_name, master_message.index, master_message.childPid, shmclock_shmptr->second, shmclock_shmptr->nanosecond);


				int i;
				for(i = 0; i < MAX_PAGE; i++) {
					if(pcbt_shmptr[c_index].page_table[i].frameNo != -1) {
						int frame = pcbt_shmptr[c_index].page_table[i].frameNo;
						deleteListElement(reference_string, c_index, i, frame);
						main_memory[frame / 8] &= ~(1 << (frame % 8));
					}
				}
			} else {
				total_access_time += incShmclock(0);
				enQueue(trackingQueue, c_index);
	
				unsigned int address = master_message.address;
				unsigned int request_page = master_message.requestPage;
				if(pcbt_shmptr[c_index].page_table[request_page].protection == 0) {
 					printWrite(fpw, "%s: process (%d) [%d] requesting read of address (%d) [%d] at time %d:%d\n", 
						exe_name, master_message.index, master_message.childPid, 
						address, request_page,
						shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				} else {
					printWrite(fpw, "%s: process (%d) [%d] requesting write of address (%d) [%d] at time %d:%d\n", 
						exe_name, master_message.index, master_message.childPid, 
						address, request_page,
						shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				}
				memoryaccess_number++;

				if(pcbt_shmptr[c_index].page_table[request_page].valid == 0) {
 					printWrite(fpw, "%s: address (%d) [%d] is not in a frame, PAGEFAULT\n",
						exe_name, address, request_page);
					
					pagefault_number++;
					total_access_time += incShmclock(14000000);
					bool is_memory_open = false;
					int count_frame = 0;
					while(1) {
						last_frame = (last_frame + 1) % MAX_FRAME;
						uint32_t frame = main_memory[last_frame / 8] & (1 << (last_frame % 8));
						if(frame == 0) {
							is_memory_open = true;
							break;
						}

						if(count_frame >= MAX_FRAME - 1) {
							break;
						}
						count_frame++;
					}

					if(is_memory_open == true) {
						pcbt_shmptr[c_index].page_table[request_page].frameNo = last_frame;
						pcbt_shmptr[c_index].page_table[request_page].valid = 1;
						main_memory[last_frame / 8] |= (1 << (last_frame % 8));
						addListElement(reference_string, c_index, request_page, last_frame);
						printWrite(fpw, "%s: allocated frame [%d] to PID (%d) [%d]\n",
							exe_name, last_frame, master_message.index, master_message.childPid);
				
						if(algorithm_choice == 1) {
							deleteListElement(lru_stack, c_index, request_page, last_frame);
							addListElement(lru_stack, c_index, request_page, last_frame);
						}

						if(pcbt_shmptr[c_index].page_table[request_page].protection == 0) {
							printWrite(fpw, "%s: address (%d) [%d] in frame (%d), giving data to process (%d) [%d] at time %d:%d\n",
								exe_name, address, request_page, 
								pcbt_shmptr[c_index].page_table[request_page].frameNo,
								master_message.index, master_message.childPid,
								shmclock_shmptr->second, shmclock_shmptr->nanosecond);

							pcbt_shmptr[c_index].page_table[request_page].dirty = 0;
						} else {
							printWrite(fpw, "%s: address (%d) [%d] in frame (%d), writing data to frame at time %d:%d\n",
								exe_name, address, request_page, 
								pcbt_shmptr[c_index].page_table[request_page].frameNo,
								shmclock_shmptr->second, shmclock_shmptr->nanosecond);

							pcbt_shmptr[c_index].page_table[request_page].dirty = 1;
						}
					} else {
 						printWrite(fpw, "%s: address (%d) [%d] is not in a frame, memory is full. Invoking page replacement...\n", exe_name, address, request_page);
						if(isDebugMode) {
							printWrite(fpw, getList(reference_string));
						}

						unsigned int fifo_index = reference_string->front->index;
						unsigned int fifo_page = reference_string->front->page;
						unsigned int fifo_address = fifo_page << 10;
						unsigned int fifo_frame = reference_string->front->frame;
						if(pcbt_shmptr[fifo_index].page_table[fifo_page].dirty == 1) {
							printWrite(fpw, "%s: address (%d) [%d] was modified. Modified information is written back to the disk\n", 
										exe_name, fifo_address, fifo_page);
						}

						pcbt_shmptr[fifo_index].page_table[fifo_page].frameNo = -1;
						pcbt_shmptr[fifo_index].page_table[fifo_page].dirty = 0;
						pcbt_shmptr[fifo_index].page_table[fifo_page].valid = 0;
						pcbt_shmptr[c_index].page_table[request_page].frameNo = fifo_frame;
						pcbt_shmptr[c_index].page_table[request_page].dirty = 0;
						pcbt_shmptr[c_index].page_table[request_page].valid = 1;
							
						deleteListFirst(reference_string);
						addListElement(reference_string, c_index, request_page, fifo_frame);

						if(isDebugMode) {
							printWrite(fpw, "After invoking FIFO algorithm...\n");
							printWrite(fpw, getList(reference_string));
						}

						if(pcbt_shmptr[c_index].page_table[request_page].protection == 1) {
							printWrite(fpw, "%s: dirty bit of frame (%d) set, adding additional time to the clock\n", exe_name, last_frame);
							printWrite(fpw, "%s: indicating to process (%d) [%d] that write has happend to address (%d) [%d]\n", 
								exe_name, master_message.index, master_message.childPid, address, request_page);
							pcbt_shmptr[c_index].page_table[request_page].dirty = 1;
						}
					}
				} else {
					if(algorithm_choice == 1) {
						int c_frame = pcbt_shmptr[c_index].page_table[request_page].frameNo;
						deleteListElement(lru_stack, c_index, request_page, c_frame);
						addListElement(lru_stack, c_index, request_page, c_frame);
					}

					if(pcbt_shmptr[c_index].page_table[request_page].protection == 0) {
						printWrite(fpw, "%s: address (%d) [%d] is already in frame (%d), giving data to process (%d) [%d] at time %d:%d\n",
							exe_name, address, request_page, 
							pcbt_shmptr[c_index].page_table[request_page].frameNo,
							master_message.index, master_message.childPid,
							shmclock_shmptr->second, shmclock_shmptr->nanosecond);
					} else {
						printWrite(fpw, "%s: address (%d) [%d] is already in frame (%d), writing data to frame at time %d:%d\n",
							exe_name, address, request_page, 
							pcbt_shmptr[c_index].page_table[request_page].frameNo,
							shmclock_shmptr->second, shmclock_shmptr->nanosecond);
					}
				}
			}

			qnext.next = (qnext.next->next != NULL) ? qnext.next->next : NULL;
			master_message.mtype = -1;
			master_message.index = -1;
			master_message.childPid = -1;
			master_message.flag = -1;
			master_message.requestPage = -1;
		}


		while(!isQueueEmpty(queue)) {
			deQueue(queue);
		}
		while(!isQueueEmpty(trackingQueue)) {
			int i = trackingQueue->front->index;
			enQueue(queue, i);
			deQueue(trackingQueue);
		}

		free(trackingQueue);
		incShmclock(0);
		int child_status = 0;
		pid_t child_pid = waitpid(-1, &child_status, WNOHANG);

		if(child_pid > 0) {
			int return_index = WEXITSTATUS(child_status);
			bitmap[return_index / 8] &= ~(1 << (return_index % 8));
		}

		if(fork_number >= TOTAL_PROCESS) {
			timer(0);
			masterHandler(0);
		}
	}
	return EXIT_SUCCESS; 
}

void printWrite(FILE *fpw, char *fmt, ...) {
	char buf[BUFFER_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	if(isDisplayTerminal) {
		fprintf(stderr, buf);
	}

	if(fpw != NULL) {
		fprintf(fpw, buf);
		fflush(fpw);
	}
}



void masterInterrupt(int seconds) {
	
	timer(seconds);
	struct sigaction sa1;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &masterHandler;
	sa1.sa_flags = SA_RESTART;
	if(sigaction(SIGALRM, &sa1, NULL) == -1) {
		perror("ERROR");
	}

	struct sigaction sa2;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &masterHandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1) {
		perror("ERROR");
	}

	signal(SIGUSR1, SIG_IGN);
	signal(SIGSEGV, segHandler);
}
void masterHandler(int signum) {
	if(!isDisplayTerminal) {
		fprintf(stderr, "%8s(%d/%d)\n\n", "", fork_number, TOTAL_PROCESS);
	}
	finalize();

	double mem_p_sec = (double)memoryaccess_number / (double)shmclock_shmptr->second;
	double pg_f_p_mem = (double)pagefault_number / (double)memoryaccess_number;
	double avg_m = (double)total_access_time / (double)memoryaccess_number;
	avg_m /= 1000000.0;

	printWrite(fpw, "- Master PID: %d\n", getpid());
	printWrite(fpw, "- Number of forking during this execution: %d\n", fork_number);
	printWrite(fpw, "- Final simulation time of this execution: %d.%d\n", shmclock_shmptr->second, shmclock_shmptr->nanosecond);
	printWrite(fpw, "- Number of memory accesses: %d\n", memoryaccess_number);
	printWrite(fpw, "- Number of memory accesses per nanosecond: %f memory/second\n", mem_p_sec);
	printWrite(fpw, "- Number of page faults: %d\n", pagefault_number);
	printWrite(fpw, "- Number of page faults per memory access: %f pagefault/access\n", pg_f_p_mem);
	printWrite(fpw, "- Average memory access speed: %f ms/n\n", avg_m);
	printWrite(fpw, "- Total memory access time: %f ms\n", (double)total_access_time / 1000000.0);
	fprintf(stderr, "SIMULATION RESULT is recorded into the log file: %s\n", log_file);

	cleanUp();

	if(fpw != NULL) {
		fclose(fpw);
		fpw = NULL;
	}

	exit(EXIT_SUCCESS); 
}

void segHandler(int signum) {
	fprintf(stderr, "Segmentation Fault\n");
	masterHandler(0);
}

void exitHandler(int signum) {
	fprintf(stderr, "%d: Terminated!\n", getpid());
	exit(EXIT_SUCCESS);
}


void timer(int seconds) {
	struct itimerval value;
	value.it_value.tv_sec = seconds;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	
	if(setitimer(ITIMER_REAL, &value, NULL) == -1) {
		perror("ERROR");
	}
}



void finalize() {
	fprintf(stderr, "\nLimitation has reached! Invoking termination...\n");
	kill(0, SIGUSR1);
	pid_t p = 0;
	while(p >= 0) {
		p = waitpid(-1, NULL, WNOHANG);
	}
}


void discardShm(int shmid, void *shmaddr, char *shm_name , char *exe_name, char *process_type) {
	if(shmaddr != NULL) {
		if((shmdt(shmaddr)) << 0) {
			fprintf(stderr, "%s (%s) ERROR: could not detach [%s] shared memory!\n", exe_name, process_type, shm_name);
		}
	}
	
	if(shmid > 0) {
		if((shmctl(shmid, IPC_RMID, NULL)) < 0) {
			fprintf(stderr, "%s (%s) ERROR: could not delete [%s] shared memory! Exiting...\n", exe_name, process_type, shm_name);
		}
	}
}


void cleanUp() {
	if(mqueueid > 0) {
		msgctl(mqueueid, IPC_RMID, NULL);
	}

	discardShm(shmclock_shmid, shmclock_shmptr, "shmclock", exe_name, "Master");

	if(semid > 0) {
		semctl(semid, 0, IPC_RMID);
	}

	discardShm(pcbt_shmid, pcbt_shmptr, "pcbt", exe_name, "Master");
}


void semaLock(int sem_index) {
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = -1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}


void semaRelease(int sem_index) {	
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = 1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}

int incShmclock(int increment) {
	semaLock(0);
	int nano = (increment > 0) ? increment : rand() % 1000 + 1;

	forkclock.nanosecond += nano; 
	shmclock_shmptr->nanosecond += nano;

	while(shmclock_shmptr->nanosecond >= 1000000000) {
		shmclock_shmptr->second++;
		shmclock_shmptr->nanosecond = abs(1000000000 - shmclock_shmptr->nanosecond);
	}

	semaRelease(0);
	return nano;
}


void initPCBT(ProcessControlBlock *pcbt) {
	int i, j;
	for(i = 0; i < MAX_PROCESS; i++) {
		pcbt[i].pidIndex = -1;
		pcbt[i].actualPid = -1;
		for(j = 0; j < MAX_PAGE; j++) {
			pcbt[i].page_table[j].frameNo = -1;
			pcbt[i].page_table[j].protection = rand() % 2;
			pcbt[i].page_table[j].dirty = 0;
			pcbt[i].page_table[j].valid = 0;
		}
	}		
}


char *getPCBT(ProcessControlBlock *pcbt) {
	int i;
	char buf[4096];

	sprintf(buf, "PCBT: ");
	for(i = 0; i < MAX_PROCESS; i++) {
		sprintf(buf, "%s(%d| %d)", buf, pcbt[i].pidIndex, pcbt[i].actualPid);
		
		if(i < MAX_PROCESS - 1) {
			sprintf(buf, "%s, ", buf);
		}
	}
	sprintf(buf, "%s\n", buf);
	return strduplicate(buf);
}


void initPCB(ProcessControlBlock *pcb, int index, pid_t pid) {
	int i;
	pcb->pidIndex = index;
	pcb->actualPid = pid;

	for(i = 0; i < MAX_PAGE; i++) {
		pcb->page_table[i].frameNo = -1;
		pcb->page_table[i].protection = rand() % 2;
		pcb->page_table[i].dirty = 0;
		pcb->page_table[i].valid = 0;
	}
}

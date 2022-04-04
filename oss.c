#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include "sharedMem.h"
#include "queue.h"

// Define functions
void start_scheduler();
void oss_terminator(int sig);
void cleanup_process();

// Define integer variables
int shmid;
int msqid;

// Define PCB
struct PCB *controlTable;

//File pointer for output log
FILE* fp;
struct sharedRes *shared;

// Define integer variables
int toChild;
int toOSS;

// Define constants
const int maxTimeBetweenNewProcSec = 1; 
const int maxTimeBetweenNewProcNS = 500;
const int realTime = 15;

//Store max number of children
int t;
int takenPids[18];

// Define struct times
struct times exec = {0,0};
struct times totalCpuTime;
struct times totalBlocked;
struct times waitTime;
struct times idleTime;

// Define struct
struct {
	long mtype;
	char message[100];
} msg;

// Define our main function
int main(int argc, char *argv[]) {
	signal(SIGALRM, oss_terminator);
	alarm(3);
	signal(SIGINT, oss_terminator);
	int c;

	while ((c=getopt(argc, argv, "n:s:h"))!= EOF) {
		switch(c) {
			case 'h':
				printf("\nUse the command in this order: ./oss -t\n");
				printf("-t: Total processes (Maximum of 20) \n\n");
				exit(0);
				break;
			case 't':
				t = atoi(optarg);
				if (t > 20) {
					t = 20;
				}
				break;
		}
	}

	// Print messages
	printf("Our scheduling process has begun...\n");
	printf("Generating output_log.txt file...\n");
	
	//If n is 0 we'll default to 18
	if(t == 0) {
		t = 18;
	}

	//output_log.txt will hold the logs for all the processes
	fp = fopen("output_log.txt", "w");
	
	if (fp == NULL) {
		perror("./oss: Error opening log file\n");
		exit(1);
	}

	//Attach PCB and clock to shared mem
	key_t key;
	key = ftok(".",'a');
	if ((shmid = shmget(key,sizeof(struct sharedRes), IPC_CREAT | 0666)) == -1) {
		perror("shmget");
		exit(1);	
	}
	
	shared = (struct sharedRes*)shmat(shmid,(void*)0,0);
	if(shared == (void*)-1) {
		perror("Error on attaching memory");
	}

	//Attach queues for communication between processes
	key_t msgkey;
	if((msgkey = ftok("queue_file1",925)) == -1) {
		perror("ftok");
	}

	if((toChild = msgget(msgkey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");	
	}	
	
	if((msgkey = ftok("queue_file2",825)) == -1) {
		perror("ftok");
	}

	if((toOSS = msgget(msgkey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");	
	}
	
	int i;
	for(i = 0; i < 18; i++) {
		takenPids[i] = 0;
	}	
	start_scheduler();
	cleanup_process();
	return 0;
}

// Define oss_terminator function
void oss_terminator(int sig) {
	switch(sig) {
		case SIGINT:
			printf("\nCTRL + C has been detected, the program will now terminate.\n");
			break;
	}
	printf(" ");
	printf("\nThe build process was successful.\n");
	cleanup_process();
	kill(0,SIGKILL);
}

void cleanup_process() {
	fclose(fp);
	msgctl(toOSS,IPC_RMID,NULL);
	msgctl(toChild,IPC_RMID,NULL);
	shmdt((void*)shared);	
	shmctl(shmid, IPC_RMID, NULL);
}

void addClock(struct times* time, int sec, int ns) {
	time->seconds += sec;
	time->nanoseconds += ns;
	while(time->nanoseconds >= 1000000000) {
		time->nanoseconds -=1000000000;
		time->seconds++;
	}
}

int firstEmptyBlock() {
	int i;
	int allTaken = 1;
	for(i = 0; i < 18; i++) {
		if(takenPids[i] == 0) {
			takenPids[i] = 1;
			return i;
		}
	}
	return -1;
}

void start_scheduler() {
	srand(time(0));
	pid_t child;
	int running = 0;

	// Create queues for storing local pids
	struct Queue* queue0 = createQueue(t);
	struct Queue* queue1 = createQueue(t);
	struct Queue* queue2 = createQueue(t);
	struct Queue* queue3 = createQueue(t);
	struct Queue* blockedQueue = createQueue(t);

	// Set vars for time quantum on each queue
	int baseTimeQuantum = 10;
	int cost0 = baseTimeQuantum  * 1000000;
	int cost1 = baseTimeQuantum * 2 * 1000000;
	int cost2 = baseTimeQuantum * 3 * 1000000;
	int cost3 = baseTimeQuantum * 4 * 1000000; 
	int active = 0, count, status, currentActive = 0, maxExecs = 50;

	// Keep track of # of lines in output log
	int lines = 0;
	
	// This count variable is how many processes have terminated
	while(count < 50) {
		addClock(&(shared->time), 0, 100000);
		if (running == 0) {
			addClock(&(idleTime),0,100000);
		}
		if( maxExecs > 0 &&(active < t) && ((shared->time.seconds > exec.seconds) ||(shared->time.seconds == exec.seconds && shared->time.nanoseconds >= exec.nanoseconds) )) {
			exec.seconds = shared->time.seconds;
			exec.nanoseconds = shared->time.nanoseconds;
			int secs = (rand() % (maxTimeBetweenNewProcSec + 1));
			int ns = (rand() % (maxTimeBetweenNewProcNS + 1));
			addClock(&exec,secs,ns);
			
			int newProc = firstEmptyBlock();
			if (newProc > -1) {
				if ((child = fork()) == 0) {
					char str[10];
					sprintf(str, "%d", newProc);
					execl("./user",str,NULL);
					exit(0);
				}
				active++;
				maxExecs--;
				shared->controlTable[newProc].pid = child;
				shared->controlTable[newProc].processClass = ((rand()% 100) < realTime) ? 1 : 0;
				shared->controlTable[newProc].totalTimeInSystem.seconds = shared->time.seconds;
				shared->controlTable[newProc].totalTimeInSystem.nanoseconds = shared->time.nanoseconds;	
				shared->controlTable[newProc].lpid = newProc;
				shared->controlTable[newProc].totalCpuTime.nanoseconds = 0;
				shared->controlTable[newProc].totalCpuTime.seconds = 0;
				shared->controlTable[newProc].blockedTime.nanoseconds = 0;
				shared->controlTable[newProc].blockedTime.seconds = 0;
				if(shared->controlTable[newProc].processClass == 0) {
					shared->controlTable[newProc].priority = 1;
					enqueue(queue1, shared->controlTable[newProc].lpid);
					if(lines < 10000) {
						fprintf(fp,"OSS: Generating process with PID %d and putting it in queue %d at time %d:%u\n",shared->controlTable[newProc].lpid, 1, shared->time.seconds,shared->time.nanoseconds);
						lines++;
					}
				}
				if(shared->controlTable[newProc].processClass == 1) {
					shared->controlTable[newProc].priority = 0;
					enqueue(queue0, shared->controlTable[newProc].lpid);
					if(lines < 10000) {
						fprintf(fp,"OSS: Generating process with PID %d and putting it in queue %d at time %d:%u\n",shared->controlTable[newProc].lpid,0, shared->time.seconds,shared->time.nanoseconds);
						lines++;
					}
				}
			}
		}
		

		// Start running if we have procceses and one isnt currently running
		if((isEmpty(queue0) == 0 || isEmpty(queue1) == 0 || isEmpty(queue2) == 0 || isEmpty(queue3) == 0) && running == 0) {	
			
			// Check queues from highest priority to lowest
			int queue;
			strcpy(msg.message,"");

			// Remove from queue0
			if (isEmpty(queue0) == 0) {
				queue = 0;
				currentActive = dequeue(queue0);
				msg.mtype = shared->controlTable[currentActive].pid;
				msgsnd(toChild, &msg, sizeof(msg), 0);
			}
			else if(isEmpty(queue1) == 0) {
				currentActive = dequeue(queue1);
				queue = 1;
				msg.mtype = shared->controlTable[currentActive].pid;
				msgsnd(toChild, &msg, sizeof(msg), 0);
			}
			else if(isEmpty(queue2) == 0) {
				currentActive = dequeue(queue2);
				queue = 2;
				msg.mtype = shared->controlTable[currentActive].pid;
				msgsnd(toChild, &msg, sizeof(msg),0);
			}
			else if(isEmpty(queue3) == 0) {
				currentActive = dequeue(queue3);
				queue = 3;
				msg.mtype = shared->controlTable[currentActive].pid;
				msgsnd(toChild, &msg, sizeof(msg), 0);
			}
			if(lines < 10000) {
				fprintf(fp,"OSS: Dispatching process with PID %d from queue %d at time %d:%d \n", currentActive, queue, shared->time.seconds, shared->time.nanoseconds);		
				lines++;
			}

			//Account for scheduling time in system clock
			int timeCost = ((rand()% 100) + 50);
			addClock(&(shared->time), 0, timeCost);	
			running = 1;	
		}
		
		if (isEmpty(blockedQueue) == 0) {
			int k;
			for(k = 0; k < blockedQueue->size; k++) {
				int lpid = dequeue(blockedQueue);
				if((msgrcv(toOSS, &msg, sizeof(msg),shared->controlTable[lpid].pid,IPC_NOWAIT) > -1) && strcmp(msg.message,"FINISHED") == 0) {
					if(shared->controlTable[lpid].processClass == 0) {
						enqueue(queue1, shared->controlTable[lpid].lpid);
						shared->controlTable[lpid].priority = 1;
					}
					else if(shared->controlTable[lpid].processClass == 1) {
						enqueue(queue0, shared->controlTable[lpid].lpid);
						shared->controlTable[lpid].priority = 0;
					}
					if(lines < 10000) {
						fprintf(fp,"OSS: Unblocking process with PID %d at time %d:%d\n",lpid,shared->time.seconds,shared->time.nanoseconds);
						lines++;
					}
				}
				else{
					enqueue(blockedQueue, lpid);
				}
				
			}
		}

		//Sends message to children if running
		if (running == 1) {
			running = 0;

			//Block while waiting for response from child
			if ((msgrcv(toOSS, &msg, sizeof(msg),shared->controlTable[currentActive].pid, 0)) > -1 ) {
				
				// If child dies
				if (strcmp(msg.message,"DIED") == 0) {
					while(waitpid(shared->controlTable[currentActive].pid, NULL, 0) > 0);
					msgrcv(toOSS,&msg, sizeof(msg), shared->controlTable[currentActive].pid,0);
					
					// Set totaltime in system
					shared->controlTable[currentActive].totalTimeInSystem = shared->time;
					shared->controlTable[currentActive].totalCpuTime.nanoseconds = shared->time.nanoseconds - shared->controlTable[currentActive].totalCpuTime.nanoseconds; 
					
					// Get percentage of time used as integer
					int p = atoi(msg.message);
					int time;	
					if(shared->controlTable[currentActive].priority == 0) {
						time = cost0 * (p / 100);
					}
					else if(shared->controlTable[currentActive].priority == 1) {
						time = cost1 * (p / 100);
					}
					else if(shared->controlTable[currentActive].priority == 2) {
						time = cost2 * (p / 100);
					}	
					else if(shared->controlTable[currentActive].priority == 3) {
						time = cost3 * (p / 100);
					}
					active--;
					count++;
					addClock(&(shared->time),0,time);
					addClock(&(shared->controlTable[currentActive].totalCpuTime),0,time);	
	
					takenPids[currentActive] = 0;				
					if(lines < 10000) {
						fprintf(fp,"OSS: Terminating process with PID %d after %u nanoseconds\n",shared->controlTable[currentActive].lpid, time);
						lines++;
					}
					addClock(&totalCpuTime,shared->controlTable[currentActive].totalCpuTime.seconds,shared->controlTable[currentActive].totalCpuTime.nanoseconds);
					addClock(&totalBlocked, shared->controlTable[currentActive].blockedTime.seconds, shared->controlTable[currentActive].blockedTime.nanoseconds);
					int waitSec = shared->controlTable[currentActive].totalTimeInSystem.seconds;
					int waitNS = shared->controlTable[currentActive].totalTimeInSystem.nanoseconds;
					waitSec -= shared->controlTable[currentActive].totalCpuTime.seconds;
					waitNS -= shared->controlTable[currentActive].totalCpuTime.nanoseconds;
					waitSec -= shared->controlTable[currentActive].blockedTime.seconds;
					waitNS -= shared->controlTable[currentActive].blockedTime.nanoseconds;
					addClock(&waitTime,waitSec,waitNS);
				}

				//If child uses all its time
				if (strcmp(msg.message,"USED_ALL_TIME") == 0) {
					int prevPriority;
					if(shared->controlTable[currentActive].priority == 0) {
						prevPriority = shared->controlTable[currentActive].priority;
						shared->controlTable[currentActive].priority = 0;
						enqueue(queue0, shared->controlTable[currentActive].lpid);
						addClock(&(shared->time) ,0 ,cost0);
						addClock(&(shared->controlTable[currentActive].totalCpuTime),0 ,cost0);
					}
					else if(shared->controlTable[currentActive].priority == 1) {
						prevPriority = shared->controlTable[currentActive].priority;
						shared->controlTable[currentActive].priority = 2;
						enqueue(queue2, shared->controlTable[currentActive].lpid);
						addClock(&(shared->time), 0,cost1);
						addClock(&(shared->controlTable[currentActive].totalCpuTime),0, cost1);
					}
					else if(shared->controlTable[currentActive].priority == 2) {
						prevPriority = shared->controlTable[currentActive].priority;
						shared->controlTable[currentActive].priority = 3;
						enqueue(queue3, shared->controlTable[currentActive].lpid);
						addClock(&(shared->time) ,0, cost2);
						addClock(&(shared->controlTable[currentActive].totalCpuTime),0 ,cost2);
					}
					else if(shared->controlTable[currentActive].priority == 3) {
						prevPriority = shared->controlTable[currentActive].priority;
						shared->controlTable[currentActive].priority = 3;
						enqueue(queue3, shared->controlTable[currentActive].lpid);
						addClock(&(shared->time) ,0 ,cost3);
						addClock(&(shared->controlTable[currentActive].totalCpuTime),0, cost3);
					}
					if(lines < 10000) {
						fprintf(fp,"OSS: Moving process with pid %d from queue %d to queue %d\n", shared->controlTable[currentActive].lpid, prevPriority, shared->controlTable[currentActive].priority);
						lines++;
					}
				}

				// If only some time is used
				if (strcmp(msg.message,"USED_SOME_TIME") == 0) {
					msgrcv(toOSS, &msg, sizeof(msg),shared->controlTable[currentActive].pid,0);
					
					//Get percentage of time used as integer
					int p = atoi(msg.message);
					int time;	
					if(shared->controlTable[currentActive].priority == 0) {
						time = cost0  / p;
					}
					else if(shared->controlTable[currentActive].priority == 1) {
						time = cost1  / p;
					}
					else if(shared->controlTable[currentActive].priority == 2) {
						time = cost2 / p;
					}	
					else if(shared->controlTable[currentActive].priority == 3) {
						time = cost3  / p;
					}
					addClock(&(shared->time),0,time);
					addClock(&(shared->controlTable[currentActive].totalCpuTime),0,time);	
					if (lines < 10000) {
						fprintf(fp,"OSS: Process with PID %d has been preempted after %d nanoseconds\n",shared->controlTable[currentActive].lpid, time);
						lines++;
					}
					enqueue(blockedQueue,shared->controlTable[currentActive].lpid);
				}

			}
		}
	}
	pid_t wpid;
	while ((wpid = wait(&status)) > 0);	
	printf("The build process has been completed.\n");
}

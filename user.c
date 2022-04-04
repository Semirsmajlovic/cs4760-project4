#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include "sharedMem.h"

// Define integer variable
int shmid;

// Define struct
struct {
	long mtype;
	char message[100];
} msg;

void addTime(struct times* time, int sec, int ns) {
	time->seconds += sec;
	time->nanoseconds += ns;
	while(time->nanoseconds >= 1000000000){
		time->nanoseconds -=1000000000;
		time->seconds++;
	}
}

int main(int argc, char *argv[]) {
	const int chanceToDie = 30;
	const int chanceToTakeFullTime = 75;
	int toChild, toOSS;
	int id = getpid();
	struct times blockingTime;
	struct sharedRes *shared;
	int lpid = atoi(argv[0]);
	time_t t;
	time(&t);	
	srand((int)time(&t) % id);
	
	// Attach PCB and clock to shared mem
	key_t key;
	key = ftok(".",'a');
	if ((shmid = shmget(key,sizeof(struct sharedRes),0666)) == -1) {
		perror("shmget");
		exit(1);	
	}
	
	shared = (struct sharedRes*)shmat(shmid,(void*)0,0);
	if (shared == (void*)-1) {
		perror("Error on attaching memory");
		exit(1);
	}

	// Attach queues for communication between processes
	key_t msgkey;
	if((msgkey = ftok("queue_file1",925)) == -1) {
		perror("ftok");
		exit(1);
	}

	if((toChild = msgget(msgkey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");
		exit(1);
	}	
	
	if((msgkey = ftok("queue_file2",825)) == -1) {
		perror("ftok");
		exit(1);
	}

	if((toOSS = msgget(msgkey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");	
		exit(1);
	}

	while(1) {
		msgrcv(toChild,&msg,sizeof(msg), id, 0);
		int toDie = (rand()%100);
		if (toDie <= chanceToDie) {
			msg.mtype = id;
			strcpy(msg.message,"DIED");
			msgsnd(toOSS, &msg, sizeof(msg), 0);
			char temp[25];
			sprintf(temp,"%d",toDie);
			strcpy(msg.message,temp);
			msgsnd(toOSS,&msg, sizeof(msg),0);
			return 0;
		}
		int toUseFullTime = (rand()%100);
		if (toUseFullTime <= chanceToTakeFullTime) {
			msg.mtype = id;
			strcpy(msg.message,"USED_ALL_TIME");
			msgsnd(toOSS, &msg, sizeof(msg), 0);
		} else {
			blockingTime.seconds = shared->time.seconds;
			blockingTime.nanoseconds = shared->time.nanoseconds;
			int secs = (rand() % 5) + 1;
			int ns = ((rand() % 1000) + 1);
			addTime(&(blockingTime), secs, ns);
			addTime(&(shared->controlTable[lpid].blockedTime),secs, ns); 
			msg.mtype = id;
			strcpy(msg.message,"USED_SOME_TIME");
			msgsnd(toOSS, &msg, sizeof(msg), 0);
			char temp[25];
			sprintf(temp,"%d", (rand()%100) + 1);
			strcpy(msg.message, temp);			
			msgsnd(toOSS, &msg, sizeof(msg), 0);
			while(1) {
				if(((shared->time.seconds > blockingTime.seconds) || (shared->time.seconds == blockingTime.seconds && shared->time.nanoseconds >= blockingTime.nanoseconds))){
					break;
				}
			}
			msg.mtype = id;
			strcpy(msg.message,"FINISHED");
			msgsnd(toOSS, &msg, sizeof(msg), IPC_NOWAIT);
		}
	}
	shmdt((void*)shared);
	return 0;
}

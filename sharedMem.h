#ifndef SHAREDMEM_H_
#define SHAREDMEM_H_

// Define times struct
struct times {
	int nanoseconds;
	int seconds;
};

// Define PCB struct
struct PCB {
	struct times totalCpuTime;
	struct times  totalTimeInSystem;
	struct times timeLastBurst;
	struct times blockedTime;
	int processClass;
	int pid;
	int lpid;
	int priority;
};

// Define sharedRes struct
struct sharedRes {
	struct PCB controlTable[18];
	struct times time;
};

#endif
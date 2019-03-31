#ifndef SHARED_H

#include <semaphore.h>

const MAX_PROCS = 19;

typedef struct {
	unsigned int seconds;
	unsigned int ns;
} Time;

typedef struct {
	int priority;
	int pid;
    int loc_pid;
	Time tCpuTime;
	Time tSysTime;
	Time tBurTime;
} ProcBlock;

typedef struct {
	ProcBlock proc[MAX_PROCS];
	Time sysTime;
} Shared;

#define SHARED_H
#endif

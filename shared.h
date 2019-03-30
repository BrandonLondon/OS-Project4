#ifndef SHARED_H

#include <semaphore.h>

typedef struct {
	unsigned int seconds;
	unsigned int ns;
} Time;

typedef struct {
	int priority;
	int pid;
	Time tCpuTime;
	Time tSysTime;
	Time tBurTime;
} ProcBlock;

typedef struct {
	ProcBlock proc[19];
	Time sysTime;
} Shared;

#define SHARED_H
#endif

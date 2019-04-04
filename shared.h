#ifndef SHARED_H

#include <semaphore.h>

/*
*	Author: Vasyl Onufriyev
*	Project 4: Message queues and Process Scheduling
*	Date: 3/4/19
*	Purpose: Shared data between oss.c and user.c
*/

#define MAX_PROCS 19

typedef struct {
	unsigned int seconds;
	unsigned int ns;
} Time;

typedef struct {
	int realtime;
	int queueID;
	int pid;
	int loc_pid;
	Time tCpuTime;
	Time tSysTime;
	Time tBlockedTime;
	Time tWaitTime;
} ProcBlock;

typedef struct {
	ProcBlock proc[MAX_PROCS];
	Time sysTime;
} Shared;

#define SHARED_H
#endif

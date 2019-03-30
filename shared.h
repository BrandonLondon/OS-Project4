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

void AddTime(Time* time, int amount)
{
	int newnano = time->ns + amount; 
	while (newnano >= 1000000000) //nano = 10^9, so keep dividing until we get to something less and increment seconds
	{
		newnano -= 1000000000;
		(time->seconds)++;
	}
	time->ns = newnano;
}

void AddTimeSpec(Time* time, int sec, int nano)
{
	time->seconds += sec;
	AddTime(time, nano);
}

#define SHARED_H
#endif

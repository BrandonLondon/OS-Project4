#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <errno.h>
#include "shared.h"
#include <signal.h>
#include <sys/time.h>
#include "string.h"
#include <sys/types.h>
#include <sys/msg.h>

const int CHANCE_TO_DIE_PERCENT = 10;
const int CHANCE_TO_USE_ALL_TIME_PERCENT = 90;

Shared* data;
int toChildQueue;
int toMasterQueue;
int ipcid;
char* filen;

void ShmAttatch();
void QueueAttatch();
void AddTime(Time* time, int amount);
void AddTimeSpec(Time* time, int sec, int nano);

struct {
	long mtype;
	char mtext[100];
} msgbuf;

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

void QueueAttatch()
{
	key_t shmkey = ftok("shmsharemsg", 766);

	if (shmkey == -1) //check if the input file exists
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: Ftok failed");
		return;
	}

	toChildQueue = msgget(shmkey, 0600 | IPC_CREAT);

	if (toChildQueue == -1)
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: toChildQueue creation failed");
		return;
	}

	shmkey = ftok("shmsharemsg2", 767);

	if (shmkey == -1) //check if the input file exists
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: Ftok failed");
		return;
	}

	toMasterQueue = msgget(shmkey, 0600 | IPC_CREAT);

	if (toMasterQueue == -1)
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: toMasterQueue creation failed");
		return;
	}
}

void ShmAttatch() //same exact memory attach function from master minus the init for the semaphores
{
	key_t shmkey = ftok("shmshare", 312); //shared mem key

	if (shmkey == -1) //check if the input file exists
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: Ftok failed");
		return;
	}

	ipcid = shmget(shmkey, sizeof(Shared), 0600 | IPC_CREAT); //get shared mem

	if (ipcid == -1) //check if the input file exists
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: failed to get shared memory");
		return;
	}

	data = (Shared*)shmat(ipcid, (void*)0, 0); //attach to shared mem

	if (data == (void*)-1) //check if the input file exists
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: Failed to attach to shared memory");
		return;
	}
}

int main(int argc, int argv)
{
	ShmAttatch();
	QueueAttatch();

	int msgstatus = msgrcv(toChildQueue, &msgbuf, sizeof(msgbuf), getpid(), 0);

	if (msgstatus == -1) //check if the input file exists
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: Failed to read message toChildQueue");
		return;
	}

	printf("IM ALIVE!..but not for long %i\n", getpid());

	srand(time(NULL) ^ (getpid()<<16));
	if ((rand() % 100) <= CHANCE_TO_DIE_PERCENT)
	{
		msgbuf.mtype = getpid();
		strcpy(msgbuf.mtext, "USED_TERM");
		msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), 0);
		printf("Child exit");
		exit(21);
	}

	if ((rand() % 100) <= CHANCE_TO_USE_ALL_TIME_PERCENT)
	{
		msgbuf.mtype = getpid();
		strcpy(msgbuf.mtext, "USED_ALL");
		msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), 0);
		printf("Child exit");
		exit(21);
	}
	else
	{
		printf("Using only part...\n\n");

		Time unblockTime;
		unblockTime.seconds = data->sysTime.seconds;
		unblockTime.ns = data->sysTime.ns;

		int secstoadd = rand() % 6;
		int mstoadd = (rand() % 1001) * 1000000;

		AddTimeSpec(&unblockTime, secstoadd, mstoadd); //set unblock time to some value seconds value 0-5 and 0-1000ms but converted to ns to make my life easier

		//printf("Current Time: %i:%i    Unblock Time: %i:%i\n\n", data->sysTime.seconds, data->sysTime.ns, unblockTime.seconds, unblockTime.ns);

		while(data->sysTime.seconds <= unblockTime.seconds);
		while(data->sysTime.ns <= unblockTime.ns)
		{
			if((msgstatus = msgrcv(toChildQueue, &msgbuf, sizeof(msgbuf), getpid(), IPC_NOWAIT)) > -1)
			{
				printf("Blocking task!");
				secstoadd = unblockTime.seconds - data->sysTime.seconds;
				mstoadd = unblockTime.ns - data->sysTime.ns;

				msgbuf.mtype = getpid();
				strcpy(msgbuf.mtext, "USED_PART 5");
				msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), 0);

				msgstatus = msgrcv(toChildQueue, &msgbuf, sizeof(msgbuf), getpid(), 0);
				printf("Resuming task! \n\n");
				AddTimeSpec(&unblockTime, secstoadd, mstoadd);
			}
		}
		printf("Child exit");//printf("Unblock ns: %i\n\n", unblockTime.ns);
		//wait on some task and block
		exit(21);
	}
}

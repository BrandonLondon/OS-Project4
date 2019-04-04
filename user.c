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

int CHANCE_TO_DIE_PERCENT = 10;
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
int FindPID(int pid);

struct {
	long mtype;
	char mtext[100];
} msgbuf;

int FindPID(int pid)
{
	int i;
	for (i = 0; i < MAX_PROCS; i++)
		if (data->proc[i].pid == pid)
			return i;
	return -1;
}

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

	int pid = getpid();

	CHANCE_TO_DIE_PERCENT = (data->proc[FindPID(pid)].realtime == 1) ? CHANCE_TO_DIE_PERCENT * 2 : CHANCE_TO_DIE_PERCENT;

	int msgstatus;

	if (msgstatus == -1)
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: Failed to read message toChildQueue");
		return;
	}
	//printf("IM ALIVE! Setting up PID: \t%i\n", pid);

	int secstoadd = 0;
	int mstoadd = 0;
	int runningIO = 0;
	Time unblockTime;

	srand(time(NULL) ^ (pid << 16));

	while (1)
	{
		msgrcv(toChildQueue, &msgbuf, sizeof(msgbuf), pid, 0);

		if ((rand() % 100) <= CHANCE_TO_DIE_PERCENT && runningIO == 0)
		{
			msgbuf.mtype = getpid();
			strcpy(msgbuf.mtext, "USED_TERM");
			msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), 0);

			int rngTimeUsed = (rand() % 99) + 1;
			char* convert[15];
			sprintf(convert, "%i", rngTimeUsed);

			msgbuf.mtype = pid;
			strcpy(msgbuf.mtext, convert);
			//printf("Sending with mtype %d and string %s\n", msgbuf.mtype, msgbuf.mtext);
			msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), 0);

			exit(21);
		}


		if ((rand() % 100) <= CHANCE_TO_USE_ALL_TIME_PERCENT)
		{
			msgbuf.mtype = getpid();
			strcpy(msgbuf.mtext, "USED_ALL");
			msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), 0);
		}
		else
		{
			//printf("Using only part...\n\n");

			if (runningIO == 0)
			{
				unblockTime.seconds = data->sysTime.seconds;
				unblockTime.ns = data->sysTime.ns;
				secstoadd = rand() % 6;
				mstoadd = (rand() % 1001) * 1000000;
				runningIO = 1;
				AddTimeSpec(&unblockTime, secstoadd, mstoadd); //set unblock time to some value seconds value 0-5 and 0-1000ms but converted to ns to make my life easier

				AddTimeSpec(&(data->proc[FindPID(pid)].tBlockedTime), secstoadd, mstoadd);

				int rngTimeUsed = (rand() % 99) + 1;
				char* convert[15];
				sprintf(convert, "%i", rngTimeUsed);

				msgbuf.mtype = pid;
				strcpy(msgbuf.mtext, "USED_PART");
				msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);

				msgbuf.mtype = pid;
				strcpy(msgbuf.mtext, convert);
				//printf("Sending with mtype %d and string %s\n", msgbuf.mtype, msgbuf.mtext);
				fflush(stdout);
				msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), 0);


				while (1)
				{
					//printf("Unblock time: %i:%i Current time: %i:%i\n", unblockTime.seconds, unblockTime.ns, data->sysTime.ns, data->sysTime.seconds);
					if (data->sysTime.seconds >= unblockTime.seconds && data->sysTime.ns >= unblockTime.ns)
						break;
				}

				msgbuf.mtype = pid;
				strcpy(msgbuf.mtext, "USED_IO_DONE");
				msgsnd(toMasterQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);

				runningIO = 0;
			}
		}
	}
}

#include <stdio.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "shared.h"

const int CHANCE_TO_DIE_PERCENT = 10;
const int CHANCE_TO_USE_ALL_TIME_PERCENT = 60;

Shared* data;
int queue;
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

    queue = msgget(shmkey, 0600 | IPC_CREAT);

    if(queue == -1)
    {
        printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: queue creation failed");
		return;
    }
}

void ShmAttatch() //attach to shared memory
{
	key_t shmkey = ftok("shmshare", 765); //shared mem key

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

    int msgstatus = msgrcv(queue, &msgbuf, sizeof(msgbuf), getpid(), 0);

    if (msgstatus == -1) //check if the input file exists
	{
		printf("\n%s: ", filen);
		fflush(stdout);
		perror("Error: Failed to read message queue");
		return;
	}

    srand(time(NULL));
    printf("IM ALIVE!..but not for long %i\n", getpid());

    if((rand() % 100) <= CHANCE_TO_DIE_PERCENT)
        exit(21);
    
    if((rand() % 100) <= CHANCE_TO_USE_ALL_TIME_PERCENT)
    {
        //signal OSS all time used
        exit(21);
    }
    else
    {
		Time unblockTime;
		unblockTime.seconds = data->sysTime.seconds;
		unblockTime.ns = data->sysTime.ns;

		AddTimeSpec(&unblockTime, (rand() % 6), (rand() % 1001) * 1000000); //set unblock time to some value seconds value 0-5 and 0-1000ms but converted to ns to make my life easier

		while((data->sysTime.seconds >= unblockTime.seconds) && (data->sysTime.ns >= unblockTime.ns)) {}
        //wait on some task and block
        exit(21);
    }
}

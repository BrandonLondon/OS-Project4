#include <stdio.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>

const int CHANCE_TO_DIE_PERCENT = 10;
const int CHANCE_TO_USE_ALL_TIME_PERCENT = 60;

Shared* data;
int queue;

void ShmAttatch();
void QueueAttatch();

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

    queue = msgget(key_t shmkey, 0600 | IPC_CREAT);

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
    
    srand(time(NULL));
    printf("IM ALIVE!..but not for long");

    if((rand() % 100) <= CHANCE_TO_DIE_PERCENT)
        exit(21);
    
    if((rand() % 100) <= CHANCE_TO_USE_ALL_TIME_PERCENT)
    {
        //signal OSS all time used
        exit(21);
    }
    else
    {
        //wait on some task and block
        exit(21);
    }
}
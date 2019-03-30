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

int ipcid; //inter proccess shared memory
Shared* data; //shared memory data
int queue;
char* filen; //name of this executable
const int MAX_TIME_BETWEEN_NEW_PROCS_NS = 1;
const int MAX_TIME_BETWEEN_NEW_PROCS_SEC = 0;
const int SCHEDULER_CLOCK_ADD_INC = 10;

const int CHANCE_TO_BE_USER = 50;
const int QUEUE_BASE_TIME = 10;

/* Create prototypes for used functions*/
void Handler(int signal);
void DoFork(int value); 
void ShmAttatch();
void TimerHandler(int sig);
int SetupInterrupt();
int SetupTimer();
void DoSharedWork();
int FindEmptyProcBlock();
void SweepProcBlocks();
void AddTimeSpec(Time* time, int sec, int nano);
void AddTime(Time* time, int amount);
int FindPID(int pid);
void QueueAttatch();

struct{
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

void Handler(int signal) //handle ctrl-c and timer hit
{
	printf("%s: Kill Signal Caught. Killing children and terminating...", filen);
	fflush(stdout);

	shmctl(ipcid, IPC_RMID, NULL); //free shared mem

	kill(getpid(), SIGTERM); //kill self
}

void DoFork(int value) //do fun fork stuff here. I know, very useful comment.
{
	char* forkarg[] = { //null terminated args set
			"./user",
			NULL
	}; //null terminated parameter array of chars

	execv(forkarg[0], forkarg); //exec
	printf("Exec failed! Aborting."); //all is lost. we couldn't fork. Blast.
	Handler(1);
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

void TimerHandler(int sig) //3 second kill timer
{
	Handler(sig);
}

int SetupInterrupt() //setup interrupt handling
{
	struct sigaction act;
	act.sa_handler = TimerHandler;
	act.sa_flags = 0;
	return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

int SetupTimer() //setup timer handling
{
	struct itimerval value;
	value.it_interval.tv_sec = 3;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	return (setitimer(ITIMER_PROF, &value, NULL));
}

int FindEmptyProcBlock()
{
	int i;
	for(i = 0; i < 19; i++)
	{
		if(data->proc[i].pid == -1)
			return i; //return proccess table position of empty
	}

	return -1; //error: no proccess slot available
}

void SweepProcBlocks()
{
	int i;
	for(i = 0; i < 19; i++)
		data->proc[i].pid = -1;
}

int FindPID(int pid)
{
	int i;
	for(i = 0; i < 19; i++)
		if(data->proc[i].pid == pid)
			return i;
	return -1;
}

void DoSharedWork()
{
	int activeProcs = 0;
	int remainingExecs = 100;
	int exitCount = 0;
	int status;

	/* Set shared memory clock value */
	data->sysTime.seconds = 0;
	data->sysTime.ns = 0;

	/* Setup time for random child spawning */
	Time nextExec;
	nextExec.seconds = 0;
	nextExec.ns = 0;

	srand(time(0)); 

	while (1) {
		AddTime(&(data->sysTime), SCHEDULER_CLOCK_ADD_INC);

		pid_t pid; //pid temp
		int usertracker = -1; //updated by userready to the position of ready struct to be launched
		if (remainingExecs > 0 && activeProcs < 19 && (data->sysTime.seconds >= nextExec.seconds) && (data->sysTime.ns >= nextExec.ns))
		{
			pid = fork(); //the mircle of proccess creation

			if (pid < 0) //...or maybe not proccess creation if this executes
			{
				perror("Failed to fork, exiting");
				Handler(1);
			}

			remainingExecs--; //we have less execs now since we launched successfully
			if (pid == 0)
			{
				DoFork(pid); //do the fork thing with exec followup
			}
			
			printf("%s: PARENT: STARTING CHILD %i AT TIME SEC: %i NANO: %i\n", filen, pid, data->sysTime.seconds, data->sysTime.ns); //we are parent. We have made child at this time

			/* Setup the next exec for proccess*/
			printf("Before: %i %i\n\n", nextExec.seconds, nextExec.ns);
			nextExec.seconds = data->sysTime.seconds;
			nextExec.ns = data->sysTime.ns;
			printf("Current: %i %i\n\n", nextExec.seconds, nextExec.ns);

			int secstoadd = abs(rand() % (MAX_TIME_BETWEEN_NEW_PROCS_SEC + 1));
			int nstoadd = abs((rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_NS + 1));
			printf("Adding: %i %i\n\n", secstoadd, nstoadd);
			AddTimeSpec(&nextExec, secstoadd, nstoadd);
			printf("After: %i %i\n\n", nextExec.seconds, nextExec.ns);

/* Test unit block
			int failpoint = 0;
			printf("FAIL? %i\n", failpoint++);
*/
			/* Setup child block if one exists */
			int pos = FindEmptyProcBlock();
			if(pos > -1)
			{
				data->proc[pos].pid = pid;

				int userRoll = ((rand() % 100) < CHANCE_TO_BE_USER) ? 1 : 0;
				data->proc[pos].priority = userRoll;

				data->proc[pos].tCpuTime.seconds = 0;
				data->proc[pos].tCpuTime.ns = 0;

				data->proc[pos].tSysTime.seconds = 0;
				data->proc[pos].tSysTime.ns = 0;

				data->proc[pos].tBurTime.seconds = 0;
				data->proc[pos].tBurTime.ns = 0;

				activeProcs++; //increment active execs
				msgbuf.mtype = pid;
				msgsnd(queue, &msgbuf, sizeof(msgbuf), 0);
			}
			else
			{
				printf("%s: PARENT: CHILD FAILED TO FIND CONTROL BLOCK. TERMINATING CHILD\n\n");
				kill(pid, SIGTERM);
			}
		}

		if ((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0) //if a PID is returned
		{
			if (WIFEXITED(status))
			{
				//printf("\n%s: PARENT: EXIT: PID: %i, CODE: %i, SEC: %i, NANO %i", filen, pid, WEXITSTATUS(status), data->seconds, data->nanoseconds);
				if (WEXITSTATUS(status) == 21) //21 is my custom return val
				{
					exitCount++;
					activeProcs--;

					int position = FindPID(pid);
					if(position > -1)
						data->proc[position].pid = -1;

					printf("%s: CHILD PID: %i: RIP. fun while it lasted: %i sec %i nano.\n", filen, pid, data->sysTime.seconds, data->sysTime.ns);
				}
			}
		}

		if (remainingExecs <= 0 && exitCount >= 100) //only get out of loop if we run out of execs or we have maxed out child count
			break;
	}
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

int main(int argc, int** argv)
{
	filen = argv[0]; //shorthand for filename

	if (SetupInterrupt() == -1) //Handler for SIGPROF failed
	{
		perror("Failed to setup Handler for SIGPROF");
		return 1;
	}
	if (SetupTimer() == -1) //timer failed
	{
		perror("Failed to setup ITIMER_PROF interval timer");
		return 1;
	}

	ShmAttatch(); //attach to shared mem
	QueueAttatch();
	SweepProcBlocks();
	signal(SIGINT, Handler);

	DoSharedWork();

	return 0;
}

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

int ipcid; //inter proccess shared memory
Shared* data; //shared memory data
char* filen; //name of this executable


/* Create prototypes for used functions*/
void Handler(int signal);
void DoFork(int value); 
void ShmAttatch();
void TimerHandler(int sig);
int SetupInterrupt();
int SetupTimer();
void DoSharedWork();

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

void DoSharedWork()
{
	data->sysTime.seconds = 0;
	data->sysTime.ns = 0;
	Time nextExec;
	srand(time(0)); 

	while (1) {
		//printf("Parent Timer: %i %i\n", data->seconds, data->nanoseconds);
		AddTime(&(data->sysTime), timerinc);

		nextExec.seconds = data->sysTime.seconds;
		nextExec.ns = data->sysTime.ns;
		AddTime(&nextExec, (rand() * rand()) % 1000000000); //this works because RAND_MAX is at least 32767, whch is > 1 billion when squared

/*
		pid_t pid; //pid temp
		int usertracker = -1; //updated by userready to the position of ready struct to be launched
		if (activeExecs < childConcurMax && remainingExecs > 0 && userready(&usertracker) > 0)
		{
			pid = fork(); //the mircle of proccess creation

			if (pid < 0) //...or maybe not proccess creation if this executes
			{
				perror("Failed to fork, exiting");
				handler(1);
			}

			remainingExecs--; //we have less execs now since we launched successfully
			if (pid == 0)
			{
				DoFork(rows[usertracker].arg, output); //do the fork thing with exec followup
			}
			
			fprintf(o, "%s: PARENT: STARTING CHILD %i AT TIME SEC: %i NANO: %i\n", filen, pid, data->seconds, data->nanoseconds); //we are parent. We have made child at this time
			rows[usertracker].flag = 1337; //set usertracker to already executed so it is ignored
			cPids[cPidsPos] = pid; //add pid to pidlist
			cPidsPos++; //increment pid list
			activeExecs++; //increment active execs
		}

		if ((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0) //if a PID is returned
		{
			if (WIFEXITED(status))
			{
				//printf("\n%s: PARENT: EXIT: PID: %i, CODE: %i, SEC: %i, NANO %i", filen, pid, WEXITSTATUS(status), data->seconds, data->nanoseconds);
				if (WEXITSTATUS(status) == 21) //21 is my custom return val
				{
					exitcount++;
					activeExecs--;
					fprintf(o, "%s: CHILD PID: %i: RIP. fun while it lasted: %i sec %i nano.\n", filen, pid, data->seconds, data->nanoseconds);
				}
			}
		}

		if (exitcount == childMax && remainingExecs == 0) //only get out of loop if we run out of execs or we have maxed out child count
			break;*/
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
	signal(SIGINT, Handler);

	return 0;
}

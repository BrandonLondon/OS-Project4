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
#include "queue.h"

int ipcid; //inter proccess shared memory
Shared* data; //shared memory data
int toChildQueue;
int toMasterQueue;
int locpidcnt = 0;
char* filen; //name of this executable
const int MAX_TIME_BETWEEN_NEW_PROCS_NS = 150000;
const int MAX_TIME_BETWEEN_NEW_PROCS_SEC = 0;
const int SCHEDULER_CLOCK_ADD_INC = 10000;

const int CHANCE_TO_BE_REALTIME = 50;
const int QUEUE_BASE_TIME = 10; //in ms

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
int FindLocPID(int pid);
void QueueAttatch();

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

void Handler(int signal) //handle ctrl-c and timer hit
{
	printf("%s: Kill Signal Caught. Killing children and terminating...", filen);
	fflush(stdout);

	int i;
	for (i = 0; i < MAX_PROCS; i++)
		if (data->proc[i].pid != -1)
			kill(data->proc[i].pid, SIGTERM);

	shmctl(ipcid, IPC_RMID, NULL); //free shared mem
	msgctl(toChildQueue, IPC_RMID, NULL);
	msgctl(toMasterQueue, IPC_RMID, NULL);

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
	for (i = 0; i < MAX_PROCS; i++)
	{
		if (data->proc[i].pid == -1)
			return i; //return proccess table position of empty
	}

	return -1; //error: no proccess slot available
}

void SweepProcBlocks()
{
	int i;
	for (i = 0; i < MAX_PROCS; i++)
		data->proc[i].pid = -1;
}

int FindPID(int pid)
{
	int i;
	for (i = 0; i < MAX_PROCS; i++)
		if (data->proc[i].pid == pid)
			return i;
	return -1;
}


int FindLocPID(int pid)
{
	int i;
	for (i = 0; i < MAX_PROCS; i++)
		if (data->proc[i].loc_pid == pid)
			return i;
	return -1;
}

void DoSharedWork()
{
	/* General sched data */
	int activeProcs = 0;
	int remainingExecs = 100;
	int exitCount = 0;
	int status;

	/* Proc toChildQueue and message toChildQueue data */
	int activeProcIndex = -1;
	int procRunning = 0;
	int msgsize;

	/* Set shared memory clock value */
	data->sysTime.seconds = 0;
	data->sysTime.ns = 0;

	/* Setup time for random child spawning */
	Time nextExec;
	nextExec.seconds = 0;
	nextExec.ns = 0;

	/* Setup proccess timeslice */
	Time timesliceEnd;
	timesliceEnd.seconds = 0;
	timesliceEnd.ns = 0;

	/* Create queues */
	struct Queue* queue0 = createQueue(MAX_PROCS); //toChildQueue of local PIDS (fake/emulated pids)
	struct Queue* queue1 = createQueue(MAX_PROCS); //toChildQueue of local PIDS (fake/emulated pids)
	struct Queue* queue2 = createQueue(MAX_PROCS); //toChildQueue of local PIDS (fake/emulated pids)
	struct Queue* queue3 = createQueue(MAX_PROCS); //toChildQueue of local PIDS (fake/emulated pids)

	struct Queue* queueBlock = createQueue(MAX_PROCS);

	int queueCost0 = QUEUE_BASE_TIME * 1000000;
	int queueCost1 = QUEUE_BASE_TIME * 2 * 1000000;
	int queueCost2 = QUEUE_BASE_TIME * 3 * 1000000;
	int queueCost3 = QUEUE_BASE_TIME * 4 * 1000000;

	/* Message tracking */
	int pauseSent = 0;

	srand(time(0));

	while (1) {
		AddTime(&(data->sysTime), SCHEDULER_CLOCK_ADD_INC);

		pid_t pid; //pid temp
		int usertracker = -1; //updated by userready to the position of ready struct to be launched

		if (remainingExecs > 0 && activeProcs < MAX_PROCS && (data->sysTime.seconds >= nextExec.seconds) && (data->sysTime.ns >= nextExec.ns))
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
			//printf("Before: %i %i\n\n", nextExec.seconds, nextExec.ns);
			nextExec.seconds = data->sysTime.seconds;
			nextExec.ns = data->sysTime.ns;
			//printf("Current: %i %i\n\n", nextExec.seconds, nextExec.ns);

			int secstoadd = abs(rand() % (MAX_TIME_BETWEEN_NEW_PROCS_SEC + 1));
			int nstoadd = abs((rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_NS + 1));
			//printf("Adding: %i %i\n\n", secstoadd, nstoadd);
			AddTimeSpec(&nextExec, secstoadd, nstoadd);
			//printf("After: %i %i\n\n", nextExec.seconds, nextExec.ns);

			/* Test unit block
						int failpoint = 0;
						printf("FAIL? %i\n", failpoint++);
			*/
			/* Setup child block if one exists */
			int pos = FindEmptyProcBlock();
			if (pos > -1)
			{
				data->proc[pos].pid = pid;
				data->proc[pos].realtime = ((rand() % 100) < CHANCE_TO_BE_REALTIME) ? 1 : 0;

				data->proc[pos].tCpuTime.seconds = 0;
				data->proc[pos].tCpuTime.ns = 0;

				data->proc[pos].tSysTime.seconds = 0;
				data->proc[pos].tSysTime.ns = 0;

				data->proc[pos].tBurTime.seconds = 0;
				data->proc[pos].tBurTime.ns = 0;

				data->proc[pos].loc_pid = ++locpidcnt;

				if(data->proc[pos].realtime == 1)
				{
					data->proc[pos].queueID = 0;
					enqueue(queue0, data->proc[pos].loc_pid);
				}
				else
				{
					data->proc[pos].queueID = 1;
					enqueue(queue1, data->proc[pos].loc_pid);
				}

				activeProcs++; //increment active execs
			}
			else
			{
				printf("%s: PARENT: CHILD FAILED TO FIND CONTROL BLOCK. TERMINATING CHILD\n\n");
				kill(pid, SIGTERM);
			}
		}

		if (procRunning == 1)
		{
/*			if (data->sysTime.seconds >= timesliceEnd.seconds && data->sysTime.ns >= timesliceEnd.ns && pauseSent == 0)
			{
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
				pauseSent = 1;
			}*/

			if ((msgsize = msgrcv(toMasterQueue, &msgbuf, sizeof(msgbuf), data->proc[activeProcIndex].pid, 0)) > -1)
			{
				//printf("RECIEVED MESSAGE IN MASTER MESSAGE QUEUE! %s\n\n", msgbuf.mtext);
				if (strcmp(msgbuf.mtext, "USED_TERM") == 0)
				{
					printf("Proc dies!\n");
					//printf("Proc used part. Blocking...", data->proc[activeProcIndex].pid);
					msgrcv(toMasterQueue, &msgbuf, sizeof(msgbuf), data->proc[activeProcIndex].pid, 0);

					int i;
					sscanf(msgbuf.mtext, "%i", &i);	
					int cost;

					switch(data->proc[activeProcIndex].queueID)
					{
						case 0:
							cost = (int)((double)queueCost0 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
						case 1:
							cost = (int)((double)queueCost1 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
						case 2:
							cost = (int)((double)queueCost2 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
						case 3:
							cost = (int)((double)queueCost3 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
					}

					printf("Proc only used %i of its time, cost: %i\n", i, cost);
					AddTime(&(data->sysTime), cost);

					procRunning = 0;
				}
				else if (strcmp(msgbuf.mtext, "USED_ALL") == 0)
				{
					printf("Proc used all time!\n");

					switch(data->proc[activeProcIndex].queueID)
					{
						case 0:
							enqueue(queue0, data->proc[FindPID(msgbuf.mtype)].loc_pid);
							data->proc[FindPID(msgbuf.mtype)].queueID = 0;
							AddTime(&(data->sysTime), queueCost0);
							break;
						case 1:
							enqueue(queue2, data->proc[FindPID(msgbuf.mtype)].loc_pid);
							data->proc[FindPID(msgbuf.mtype)].queueID = 2;
							AddTime(&(data->sysTime), queueCost1);
							break;
						case 2:
							enqueue(queue3, data->proc[FindPID(msgbuf.mtype)].loc_pid);
							data->proc[FindPID(msgbuf.mtype)].queueID = 3;
							AddTime(&(data->sysTime), queueCost2);
							break;
						case 3:
							enqueue(queue3, data->proc[FindPID(msgbuf.mtype)].loc_pid);
							data->proc[FindPID(msgbuf.mtype)].queueID = 3;
							AddTime(&(data->sysTime), queueCost3);
							break;
					}
					
					/*This apparently costs more than calculating pi*/
					//printf("Proc used all of its time, cost: %i\n", queueCost0);

					procRunning = 0;
				}
				else if (strcmp(msgbuf.mtext, "USED_PART") == 0)
				{
					//printf("Proc used part. Blocking...", data->proc[activeProcIndex].pid);
					msgrcv(toMasterQueue, &msgbuf, sizeof(msgbuf), data->proc[activeProcIndex].pid, 0);

					int i;
					sscanf(msgbuf.mtext, "%i", &i);	
					int cost;

					switch(data->proc[activeProcIndex].queueID)
					{
						case 0:
							cost = (int)((double)queueCost0 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
						case 1:
							cost = (int)((double)queueCost1 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
						case 2:
							cost = (int)((double)queueCost2 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
						case 3:
							cost = (int)((double)queueCost3 * ((double)i/(double)100));
							printf("Proc only used %i of its time, cost: %i\n", i, cost);
							AddTime(&(data->sysTime), cost);
							break;
					}

					enqueue(queueBlock, data->proc[FindPID(msgbuf.mtype)].loc_pid);
					procRunning = 0;
				}
				//printf("Is queue empty? %i Proccess running? %i", isEmpty(queue0), procRunning);
			}
		}

		if(isEmpty(queueBlock) == 0)
		{
			if(procRunning == 0)
				AddTime(&(data->sysTime), 5000000);	//No process is running. Hyperspeed until the next process is ready!			

			int t;
			for(t = 0; t < getSize(queueBlock); t++) //I realize this is slightly inefficient, but the alternatives are worse. This is simpler.
			{
				int blockedProcID = FindLocPID(dequeue(queueBlock));
				if ((msgsize = msgrcv(toMasterQueue, &msgbuf, sizeof(msgbuf), data->proc[blockedProcID].pid, IPC_NOWAIT)) > -1 && strcmp(msgbuf.mtext, "USED_IO_DONE") == 0)
				{
					printf("Proc unblocked!\n");

					if(data->proc[blockedProcID].realtime == 1)
					{
						enqueue(queue0, data->proc[blockedProcID].loc_pid);
						data->proc[blockedProcID].queueID = 0;
					}
					else
					{
						enqueue(queue1, data->proc[blockedProcID].loc_pid);
						data->proc[blockedProcID].queueID = 1;
					}

					int schedCost = ((rand() % 9900) + 100);
					printf("Scheduler time cost to move to queue: %i\n", schedCost);
					AddTime(&(data->sysTime), schedCost);
				}
				else
				{
					enqueue(queueBlock, data->proc[blockedProcID].loc_pid); //proc not ready to be unblocked
				}
			}
		}

		if ((isEmpty(queue0) == 0 || isEmpty(queue1) == 0 || isEmpty(queue2) == 0 || isEmpty(queue3) == 0) && procRunning == 0)
		{
			int schedCost = ((rand() % 9900) + 100);
			printf("Scheduler time cost: %i\n", schedCost);
			AddTime(&(data->sysTime), schedCost);

			if(isEmpty(queue0) == 0)
			{
				activeProcIndex = FindLocPID(dequeue(queue0));
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			}
			else if(isEmpty(queue1) == 0)
			{
				activeProcIndex = FindLocPID(dequeue(queue1));
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			}
			else if(isEmpty(queue2) == 0)
			{
				activeProcIndex = FindLocPID(dequeue(queue2));
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			}
			else if(isEmpty(queue3) == 0)
			{
				activeProcIndex = FindLocPID(dequeue(queue3));
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			}

			procRunning = 1;
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
					if (position > -1)
						data->proc[position].pid = -1;

					printf("%s: CHILD PID: %i: (LOC_PID: %i)RIP. fun while it lasted: %i sec %i nano.\n", filen, pid, data->proc[position].loc_pid, data->sysTime.seconds, data->sysTime.ns);
				}
			}
		}

		if (remainingExecs <= 0 && exitCount >= 100) //only get out of loop if we run out of execs or we have maxed out child count
			break;
		fflush(stdout);
	}

	shmctl(ipcid, IPC_RMID, NULL);
	msgctl(toChildQueue, IPC_RMID, NULL);
	msgctl(toMasterQueue, IPC_RMID, NULL);
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

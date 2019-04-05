# README

***

Author: Vasyl Onufriyev
Date Finished: 4-4-19

***

## Purpose

Master will create processes during certain time intervals until it reaches 100 total created processes or 3 real life seconds at which point the program terminates.
During normal operation, the master, OSS will shift around child processes or "schedule" them within queues.

There are two tiers of processes:

1. Realtime - Run in the highest queue always, highest priority, have highest chance of termination (2x rate of user processes)
2. User - Lower priority than realtime, can get shifted to queues 1 - 3
```c
• Whenever a process terminates, the time used before termination is added to the clock and the child terminates.

• Whenever a process uses all time, its queue level quantum is added to the system clock 

• Whenever a process uses part of its time, it is assumed it is doing an IO operation and is placed in a blocked queue until such a time that it deems it wishes to wake up, then signals
the master to reschedule it.
```
Details of each processes runtimes, block time, etc, is tracked in the process block.

end of execution statistics are displayed on the screen. Both total and average times are shown. Blocking times may be many times longer than the "total time" statistic. This is because blocked processes
serve time "spinlock' concurrently, not consecutively.

There are some issues in the output file. some entries may not be in the proper order, or may seem to "overlap" in time even with a queue size of 1, but this is false. 
Each proccess follows the termination of the last in this case.

## How to Run
```
$ make
$ ./oss [options]
```

### Options:

```
-h -> show help menu
-n -> how many children should exist at any given time. Max 19
```

Output file:

output.log

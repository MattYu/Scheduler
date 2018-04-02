# Scheduler
Simulation of Linux 2.6's O(1) scheduler, using pthread and mutext


## 1. OBJECTIVE
Implementing and simulating a priority-based scheduler in a system with
a single processor. Based on Linux 2.6 O(1) scheduler.
## 2. PROCEDURE/DISCUSSION ( MAY INCLUDE BELOW POINTS)
### Overview
O(1) real time Scheduler similator based on linux 2.6's scheduler.
Takes mock process data from input file pbs_input.txt
Output results to pbs_output.txt and on screen
### Class Logger
Description:

• Used for logging current time, process, process status (arrived, paused, started, terminated), time quantum allocated and process priority change if applicable
• As the logger is shared by multiple threads (description to follow), semaphore is used to protect critical section
### Class Timer
Description:

• An atomic time chronometer with methods start(), reset() and get_elapsed_time().

• Method start() stores instantaneous time.

• Method get_elapsed_time() get the time, in millisecond since the Timer has been started

• Method restart() resets the stored start time to new instantaneous time.

### Class Thread and function do work
Description:

• Process is simulated as a thread according to project requirement specification

• threads are encapsulated in class Thread and calls upon function do_work

• Class Thread contains method for suspending and resuming it: Since Linux Pthread does not support preemption, semaphore are used to simulate pre-emption to pause and unpause thread execution while within function do_work.

• Class Thread stores thread info on burst time, priority and status. Simulate Process' PCB.

###Class Priority_Array
Description:

• Thread are hashed in array of size 139 by priority (thread have priority between 1-139). For collisions: Each index contains a double linked list storing thread pointers of the same priority. Index and lowest priority are track and updated after each insert (push) and delete (pop). All operations are performed at O(1).
• Critical section (i.e. push and pop) are protected by semaphore
• Int index stores the index of the last pop. When searching for the next pop, will get incremented until it reaches a new empty list.
• If a process priority is changed. index may be reset, to the lowest priority in the array list.

### Class RunQueue
Description:

• Linux 2.6 scheduler implementation.
• Has a class Timer
• Has a class Logger
• Has two Priority_Array instances. One for expired queue and one for active queue
• Method run_Scheduler initiate a seperate thread with "this" as parameters.
• Method get_Quantum calculates the time to allocate
• Method get_new_priority updates a process' priority. Is called if a process is executed twice in a row.
• Method add_2_expire add a new process to expire. Is used by simulator to add new process while scheduler runs on seperate thread or by scheduler to add a paused thread to the expire queue. A semaphore protection is used: scheduler cannot switch to expire while process is being added; has to wait. And vice-versa.
• Method Scheduler: Runs scheduler and is a private method: (May only be accessed via method run_Scheduler on seperate thread instance for security)
Program flow Method Scheduler:
a. Checks if current active queue is empty. If yes, flags expire queue to active queue and active queue to expire queue.
b. Use Priority_Array pop method to get next thread to run.
c. Get current time, popped process PID information for logging purposes. Save info in temporary.
d. Compare if the above thread with previously run process info to see if it has run twice times in a row. If yes, set a flag to update priority after next quantum finish execution.
e. Get current process priority.
f. Calculate quantum to allocate based on priority.
g. Output info to log.
h. Calls on class Thread's method resume(): unblocking the thread's semaphore and resuming the thread. Note that Method resume() has built in features which differentiate if it is the first time the thread is executing or has been previously paused and resume the thread accordingly.
i. A while loop uses class Timer's get elapsed_time_method, to compare current time with process resume() time logged above. loop is broken under 2 conditions: if process finished flag is set or if elapsed time is greater than the allocated quantum time slot.
j. If process finish flag is set, logs it. Thread finished. Destructor is called.
k. If not: Calls on class Thread's method pause(): blocking the running thread's semaphore and pausing the thread. Method pause also update the process' PBC info.
l. If priority_update flag has been set: calls on method get_new_priority to update the process's priority.
m. Calls on method add_2_expire to add process to expire queue.
n. Repeat all previous steps until simulation expected end

### Main()
Opens input and store it in a buffer string; Close input; Get the first line of input (i.e. number of expected simulated processes); Initiate an instance of scheduler with the above first line as parameter; Start scheduler on a seperate thread; While scheduler runs, simultanously insert new process at appropriate time

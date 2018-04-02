# Scheduler
Simulation of Linux 2.6's O(1) scheduler, using pthread and mutext


# 1. OBJECTIVE
Implementing and simulating a priority-based scheduler in a system with
a single processor. Based on Linux 2.6 O(1) scheduler.
# 2. PROCEDURE/DISCUSSION ( MAY INCLUDE BELOW POINTS)
## Overview
O(1) real time Scheduler similator based on linux 2.6's scheduler.
Takes mock process data from input file pbs_input.txt
Output results to pbs_output.txt and on screen
## Class Logger
Description:
• Used for logging current time, process, process status (arrived, paused, started, terminated), time quantum allocated and process priority change if applicable
• As the logger is shared by multiple threads (description to follow), semaphore is used to protect critical section
## Class Timer
Description:
• An atomic time chronometer with methods start(), reset() and get_elapsed_time().
• Method start() stores instantaneous time.
• Method get_elapsed_time() get the time, in millisecond since the Timer has been started
• Method restart() resets the stored start time to new instantaneous time.
Sample Code: public: std::chrono::high_resolution_clock::time_point start_time;
void reset(){ start_time = std::chrono::high_resolution_clock::now(); } int get_elapsed_time(){ std::chrono::high_resolution_clock::time_point time_now = std::chrono::high_resolution_clock::now(); int result = std::chrono::duration_cast<std::chrono::milliseconds>(time_now -this->start_time).count(); return result; }
## Class Thread and function do work
Description:
• Process is simulated as a thread according to project requirement specification
• threads are encapsulated in class Thread and calls upon function do_work
• Class Thread contains method for suspending and resuming it: Since Linux Pthread does not support preemption, semaphore are used to simulate pre-emption to pause and unpause thread execution while within function do_work.
• Class Thread stores thread info on burst time, priority and status. Simulate Process' PCB.
Sample code: void execute(){ sem_init(&semaphore, 0, 1); started = 1; pthread_create(&tid, NULL, do_work, this); } void resume(){ timer.reset(); if (started == 0) this->execute(); else sem_post(&semaphore); } void *do_work(void *argument){ while ((*(Thread *)(argument)).burst_time - (*(Thread *)(argument)).timer.get_elapsed_time()> 0) { sem_wait(&(*(Thread *)(argument)).semaphore); /*do work*/ sem_post(&(*(Thread *)(argument)).semaphore); } (*(Thread *)(argument)).finished = 1; }

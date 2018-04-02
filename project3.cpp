#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>
#include <sys/types.h>
#include <semaphore.h>
#include <queue>
#include <vector>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <ctime>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <fstream>
#include <cmath>
#include <sstream>
#include <chrono>
#include <list>

/*

Ming Tao Yu - 03/22/2018
COEN 346 - Project 3
*********************************************************
Description:

O(1) real time Scheduler similator based on linux 2.6's scheduler.

Takes mock process data from input file pbs_input.txt
Output results to pbs_output.txt and on screen
*********************************************************
File content
 
- class Logger: used to write scheduler log info in pbs_output.txt
- class Timer: time counter used by scheduler to allocate time quantum and by simulated process (i.e. thread) to decrement burst time

- class thread: simulate process PBS. Each thread instance creates a new threat when resumed/started and contains information on priority, waiting status, etc. 
- function do_work: called by each thread instance to simulate actual thread work. May be paused and unpaused by semaphore, simulating process preemption

- class Priority_Array: A perfect hash table of size 139, one for each priority (process priority can be between 1 and 139). Each location contains a double linked list storing pointers to simulated process (pointers to threads)
- Runqueue: Scheduler. When started, creates a second thread to run. First thread is used to insert newly arrived processes while second schedules the arrived processes.
- Main is responsable for reading the output, creating new processes and adding it to the running scheduler. 
*********************************************************
*/

void *do_work(void *argument);

class Logger{
//Log current time, process, process status (arrived, paused, started, terminated), time quantum allocated and process priority change if applicable
// As the logger is shared by multiple threads, semaphore is used to protect critical section
	public:

	std::ofstream log;
	sem_t semaphore;

	Logger(std::string filename){
		sem_init(&semaphore, 0, 1);
		log.open(filename);
	}
	void close(){
		log.close();
	}

	void write(std::string content){
		sem_wait(&semaphore);
		log << content << "\n";
		std::cout<< content << "\n";
		sem_post(&semaphore);
	}

};


class Timer{
//An atomic time chronometer with function start, reset and get_elapsed_time. 
//Method start() stores instantanous time. 
//Method get_elapsed_time() get the time, in millisecond since the Timer has been started
//Method restart() resets the stored start time to new instantanous time.

	public:

	std::chrono::high_resolution_clock::time_point start_time;

	Timer(){
		start_time = std::chrono::high_resolution_clock::now();
	}
	
	void start(){
		start_time = std::chrono::high_resolution_clock::now();
	}
	
	void reset(){
		start_time = std::chrono::high_resolution_clock::now();
	}

	int get_elapsed_time(){
		std::chrono::high_resolution_clock::time_point time_now = std::chrono::high_resolution_clock::now();
		int result = std::chrono::duration_cast<std::chrono::milliseconds>(time_now -this->start_time).count();
		return result;
	}
	
};


class Thread{  
//Process is simulated as a thread according to project requirement specification
//threads are encapsulated in class Thread
//Class Thread contains method for suspending and resuming it: Since linux Pthread does not support preemption, semaphore are used to similate preemption to pause and unpause thread execution. 
// Class Thread stores thread info on burst time, priority and status. Simulate Process' PCB.
	 
	public:
	
	
	pthread_t tid;
	std::string pid;
	int priority;
	int arrival_time;
	int waiting_time;
	int burst_time;
	int suspend_time;
	int resume_time;

	bool waiting;
	bool started;
	bool finished;
	sem_t semaphore;
	sem_t semaphore_priority;
	pthread_mutex_t waitingMutex;
	pthread_cond_t resumeCond;

	Timer timer;

	Thread(): timer(){}

	Thread(std::string pid, int arrival_time, int burst_time, int priority): timer()
	{
		this->pid = pid;
		this->arrival_time = arrival_time;
		this->burst_time = burst_time;
		this->priority = priority;
		suspend_time = arrival_time;
		started = 0;
		waiting = 0;
		finished = 0;
		sem_init(&semaphore_priority, 0, 1);
	}


	void execute(){
		sem_init(&semaphore, 0, 1);
		started = 1;
		pthread_create(&tid, NULL, do_work, this);
	}

	void suspend(){
		if (finished == 0) {
			sem_wait(&semaphore);
			burst_time = burst_time - timer.get_elapsed_time();
			if (burst_time <= 0){
				finished = 1;
				sem_post(&semaphore);
				(void) pthread_join(tid, NULL);
			}
		}
	
	}

	void resume(){
		timer.reset();
		if (started == 0) this->execute();
		else sem_post(&semaphore);
	}

	bool get_status(){
		return finished;
	}

	std::string get_pid(){
		return pid;

	}

	void update_priority(int value){
		sem_wait(&semaphore_priority);
		priority = value;
		sem_post(&semaphore_priority);
		return;
	}


	
};

void *do_work(void *argument){
// Function work similates a thread doing work, but since there are no work to be done in the simulation, each thread is given a predifined cpu burst time and the fnc simply decrements the given thread cpu burst time counter until it reaches 0. Exit when burst time = 0 and set the finished flag to true.

	while ((*(Thread *)(argument)).burst_time - (*(Thread *)(argument)).timer.get_elapsed_time()> 0){
		sem_wait(&(*(Thread *)(argument)).semaphore);
		/*do work*/
		sem_post(&(*(Thread *)(argument)).semaphore);
	}
	(*(Thread *)(argument)).finished = 1;
}



class Priority_Array{
// Thread are  hashed in array of size 139 by priority (thread have priority between 1-139). For collisions: Each index contains a double linked list storing thread pointers of the same priority. Index and lowest priority are track and updated after each insert (push) and delete (pop). All operations are performed at O(1).
// Critical section (i.e. push and pop) are protected by semaphore
// Int index stores the index of the last pop. When searching for the next pop, will get incremented until it reaches a new empty list.
// If a process priority is changed. index may be reset, to the lowest priority in the array list.

	public:
	
	std::list<Thread*> hashTable [139];
	bool active;
	int size;
	int index;
	int lowest_priority;
	sem_t semaphore;

	Priority_Array(){
		sem_init(&semaphore, 0, 1);
		size = 0;
		index = 0;
		lowest_priority = 0;
	}

	void push(Thread * thread){
		sem_wait(&semaphore);
		size++;
		if (thread->priority < index){
			index = thread->priority;
		}
		hashTable[thread->priority].push_back(thread);
		sem_post(&semaphore);
	}

	Thread* get_top(){
		//fetches top of priority_queue and pop's it
		sem_wait(&semaphore);
		while (hashTable[index].size() == 0 && index != 140){
			index++;
		}		
		size--;
		Thread* top = hashTable[index].front();
		hashTable[index].pop_front();
		if (index >= 140 || size == 0){
			index = 0;
		}
		sem_post(&semaphore);
		return top;
	}

	int get_size(){
		sem_wait(&semaphore);
		int result = size;
		sem_post(&semaphore);
		return result;
	}		
};


class RunQueue{
// Linux 2.6 scheduler implementation. 
// Has a class Timer
// Has a class Logger
// Has two Priority_Array instances. One for expired queue and one for active queue
// Method run_Scheduler initiate a seperate thread with "this" as parameters. 
// Method get_Quantum calculates the time to allocate
// Method get_new_priority updates a process' priority. Is called if a process is executed twice in a row.
// Method add_2_expire add a new process to expire. Is used by simulator to add new process while scheduler runs on seperate thread or by scheduler to add a paused thread to the expire queue. A semaphore protection is used: scheduler cannot switch to expire while process is being added; has to wait. And vice-versa.

/*****************************************
// Method Scheduler
*****************************************
Runs scheduler and is a private method: (May only be accessed via method run_Scheduler on seperate thread instance for security). Works as follow:
- Checks if current active queue is empty. If yes, flags expire queue to active queue and active queue to expire queue
- Use Priority_Array pop method to get next thread to run. 
- Get current time, popped process PID information for logging purposes. Save info in temporary.
- Compare if the above thread with previously run process info to see if it has run twice times in a row. If yes, set a flag to update priority after next quantum finish execution.
- Get current process priority
- Calculate quantum to allocate based on priority
- Output info to log
- Calls on class Thread's method resume(): unblocking the thread's semaphore and resuming the thread. Note that Method resume() has built in features which differentiate if it is the first time the thread is executing or has been previously paused and resume the thread accordingly.
- A while loop uses class Timer's get elapsed_time_method, to compare current time with process resume() time logged above.  loop is broken under 2 conditions: if process finished flag is set or if elapsed time is greater than the allocated quantum time slot.
- If process finish flag is set, logs it. Thread finished. Destructor is called.
- If not: Calls on class Thread's method pause(): blocking the running thread's semaphore and pausing the thread. Method pause also update the process' PBC info. 
- If priority_update flag has been set: calls on method get_new_priority to update the process's priority
- Calls on method add_2_expire to add process to expire queue. 
- Repeat all previous steps until simulation expected end is achieved. (i.e. all process terminated, no more anticipated processes in the future)


All operations are performed in O(1).
*/
	public:
	
	
	Priority_Array queue_1;
	Priority_Array queue_2;
	Priority_Array * active;
	Priority_Array * expired;
	pthread_t scheduler_thread;
	Timer timer;
	Logger logger;
	sem_t semaphore;

	int number_expected_process;
	std::string last_exec_pid;

	RunQueue(): queue_1(), queue_2(), timer(), logger("pds_output.txt")
	{
		sem_init(&semaphore, 0, 1);
		active = &queue_1;
		expired = &queue_2;
		last_exec_pid = "";
	}

	RunQueue(int expected_process): queue_1(), queue_2(), timer(), logger("pds_output.txt")
	{
		sem_init(&semaphore, 0, 1);
		number_expected_process = expected_process;
		active = &queue_1;
		expired = &queue_2;
		
	}

	int run_scheduler(){
		return (pthread_create(&scheduler_thread, NULL, scheduler_init, this) == 0);
	}
	
	int get_quantum(int priority){

		if (priority < 100) return (140-priority)*20;
		
		else return (140-priority)*5;
	}

	int get_new_priority(int waiting_time, int old_priority, int current_time, int arrival_time){
		int bonus = ceil(10* waiting_time/(current_time - arrival_time));
		return std::max(100, std::min(old_priority - bonus + 5, 139));
	}

	void add_2_expired(Thread * process, bool call_by_main = 0){
		int time = timer.get_elapsed_time();
		sem_wait(&semaphore);
		expired->push(process);
		sem_post(&semaphore);
		std::string log;
		log = "Time " + std::to_string(time - time%100) + ", " + process->get_pid() + ", ";
		if (call_by_main == 1){
			 std::string log;
			 log = "Time " + std::to_string(time - time%100) + ", " + process->get_pid() + ", " + "Arrived";
			 logger.write(log);
		}
	}

	void swap_active_expired(){
		sem_wait(&semaphore);
		Priority_Array * temp = active;
		active = expired;
		expired = temp;
		sem_post(&semaphore);
	}

	int get_time(){
		return timer.get_elapsed_time();
	}


	
	private:
	static void * scheduler_init(void * init_thread){
		((RunQueue *)(init_thread))-> scheduler();
		((RunQueue *)(init_thread))->timer.start();
		return NULL;
	}
		
	void scheduler(){
		while (number_expected_process > 0){
			if (active->get_size() == 0){
				swap_active_expired();
				continue;
			}
			int time = timer.get_elapsed_time();
			bool update_priority = false;
			Thread * current_process = active->get_top();
			if (last_exec_pid == current_process->get_pid()) update_priority = true;
			last_exec_pid = current_process->get_pid();
			current_process->waiting_time = current_process->waiting_time + time - current_process->suspend_time;
			int priority = current_process-> priority;
			int quantum = get_quantum(priority);
			std::string log = "Time " + std::to_string(time - time%100) + ", " + current_process->get_pid() + ", ";
			if (current_process->started == 0){
				log = log + "Started, Granted ";
			}
			else {
				log = log + "Resumed, Granted ";
			}
			log = log + std::to_string(quantum);
			logger.write(log);
			bool finished = 0;
			current_process->resume_time= time;
			current_process->resume();
			current_process->started = 1;
			while((timer.get_elapsed_time()- time)<=quantum){
				usleep(1);
				if (finished = current_process->get_status()) break;
			}
			current_process->suspend();
			time = timer.get_elapsed_time();
			log = "Time " + std::to_string(time - time%100) + ", " + current_process->get_pid() + ", ";				
			finished = current_process->get_status();
			if (finished == 1){
				number_expected_process--;
				log = log + "Terminated";
				logger.write(log);
			}
			else{
				std::string log_2 = log;
				log_2 = log_2 + "Paused";
				logger.write(log_2);
				current_process->suspend_time = time;
				add_2_expired(current_process);
				if (update_priority == true){
					int new_priority = get_new_priority(current_process->waiting_time, priority, time, current_process->arrival_time);
					current_process->update_priority(new_priority);
					log = log + "Priority updated to " + std::to_string(new_priority);
					logger.write(log);
					
				}
			}
		}
		logger.close();
	}

};

int main(int argc, char * argv[]){
	// Opens input and store it in a buffer string

	std::ifstream ifs("pbs_input.txt");
	std::string sim_input((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	// Close input. 
	ifs.close();
	// Get the first line of input (i.e. number of expected simulated processes)
	std::istringstream f(sim_input);
	std::string line;
	std::getline(f, line);
	//Initiate an instance of scheduler with the above first line as parameter
	RunQueue* scheduler = new RunQueue(std::stoi(line));
	//Start scheduler on a seperate thread
	scheduler->run_scheduler();
	//While scheduler runs, simultanously insert new process at appropriate time
	//Go line by line in input string 
	while(getline(f, line)){
		//fetch info of the process in input string: i.e. PID, arrival time, burst time, process priority
		std::string process_info;
		std::istringstream s(line);
		std::string pid;
		int arrival_time;
		int burst_time;
		int priority;
		getline(s, process_info, ' ');
		pid = process_info;
		getline(s, process_info, ' ');
		arrival_time = std::stoi(process_info);
		getline(s, process_info, ' ');
		burst_time = std::stoi(process_info);
		getline(s, process_info, ' ');
		priority = std::stoi(process_info);
		//create a new thread with correct variables and wait until arrival time before inserting it until expired queue of the scheduler
		Thread * process_to_insert = new Thread(pid, arrival_time, burst_time, priority);
		
		while (scheduler->get_time() < arrival_time){
			usleep(1);
		}
		scheduler->add_2_expired(process_to_insert, 1);
	}
	//wait for scheduler thread to complete and exit before exiting the main thread. 
	while(scheduler->number_expected_process !=0){

	}
	
	return 0;

}

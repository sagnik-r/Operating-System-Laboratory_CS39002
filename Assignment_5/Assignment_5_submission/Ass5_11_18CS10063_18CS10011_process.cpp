#include <iostream> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h> 
#include <pthread.h>
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <chrono>
#include <time.h> 
#include <random>
#include <thread>
#include <vector>
using namespace std; 

//Size of priority Queue 
#define PQ_SIZE 8

//Structure defining each job 
typedef struct{
	int proc_id;//Producer process id
	int prod_num;
	int priority;
	int comp_time;
	int job_id;
}job;

//Structure defining the shared memory
typedef struct{
	job  priority_q[PQ_SIZE];// Priority Queue (Internally implemented as a MAX-HEAP)
	int job_created;// Number of jobs created
	int job_completed;// Number of jobs completed
	int curr_size;// Current number of elements in the queue

	pthread_mutex_t mutex;// Mutex Lock associated to prevent race conditions
}shared_memory;

job create_job(int proc_id, int prod_num);
job remove_job(shared_memory *sm);
void insert_job(job new_job, shared_memory *sm);
void producer_generate_job(int proc_id, shared_memory *sm, int  prod_number, int jobs);
void consumer_finish_job(int proc_id, shared_memory *sm, int  con_number, int jobs);

// Function to create and return the shared memory segment
shared_memory *create_mem(int shmid)
{
	// Attaching shared memory to adddress space of parent
	shared_memory *sm = (shared_memory *)shmat(shmid, (void *)0, 0);
	// Initializing the priority queue
	sm->job_created = 0;
	sm->job_completed = 0;
	sm->curr_size = 0;

	// Initializing the mutex
	//sm->mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutexattr_t att;
	pthread_mutexattr_init(&att);
	pthread_mutexattr_setrobust(&att, PTHREAD_MUTEX_STALLED);
	pthread_mutexattr_setpshared(&att, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&sm->mutex, &att);

	return sm;
}

int main()
{
	//Taking in required input
	int cons, prods, jobs;
	cout<<"Enter the number of producers: ";
	cin>>prods;
	cout<<"Enter the number of consumers: ";
	cin>>cons;
	cout<<"Enter the number of jobs: ";
	cin>>jobs;

	key_t key = (key_t)1234;
	// Creating the shared memory
	int shmid = shmget(key, sizeof(shared_memory), 0666|IPC_CREAT);
	if (shmid<0)
	{
		cout<<"Shared memory creation failed!!!"<<endl;
		exit(1);
	}
	shared_memory *sm = create_mem(shmid);

	pid_t pid;
	// Recording the start time
	auto start_time = std::chrono::high_resolution_clock::now();

	// Vectors to hold the pids of the child processes
	vector<pid_t> prod_pid;
	vector<pid_t> cons_pid;

	// Creating the producer processes
	for(int i=1; i<=prods; i++)
	{
		pid = fork();
		if(pid<0)//Error
		{
			cout<<"ERROR!! CANT CREATE PRODUCER!!!";
			exit (1);
		}
		else if(pid==0)// Child Process
		{
			sm = (shared_memory *)shmat(shmid, (void *)0, 0);// Attaching shared memory to address space of child 
			int proc_id = getpid();
			producer_generate_job(proc_id, sm, i, jobs);
			shmdt(sm);// Detaching shared memory from address space of child 
			return 0;
		}
		else// Parent process
		{
			prod_pid.push_back(pid);
		}
	}

	// Creating the consumer processes
	for(int i=1; i<=cons; i++)
	{
		pid = fork();
		if(pid<0)// Error
		{
			cout<<"ERROR!! CANT CREATE CONSUMER!!!";
			exit(1);
		}
		else if(pid==0)// Child Process
		{
			sm = (shared_memory *)shmat(shmid, (void *)0, 0);// Attaching the shared memory to address space of child
			int proc_id = getpid();
			consumer_finish_job(proc_id, sm, i, jobs);
			shmdt(sm);// Detaching shared memory from address space of child
			return 0;
		}
		else// Parent process
		{
			cons_pid.push_back(pid);
		}
	}
	
	while(1)
	{
		pthread_mutex_lock(&sm->mutex);
		if(sm->job_created==jobs && sm->job_completed==jobs)// Check if required number of jobs have been created and completed
		{

			auto end_time = std::chrono::high_resolution_clock::now();
    		auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);// Calculating the total time elapsed
    		cout<<"Total time taken: "<<elapsed.count()*1e-9<<endl;
    		pthread_mutex_unlock(&sm->mutex);
    		for(int i=0; i<prod_pid.size();i++)// Killing all the producer processes
    		{
    			int status;
    			kill(prod_pid[i], SIGTERM);
    			waitpid(prod_pid[i], &status, WNOHANG);
    		}
    		
    		for(int i=0; i<cons_pid.size(); i++)// Killing all the consumer processes
    		{
    			int status;
    			kill(cons_pid[i], SIGTERM);
    			waitpid(cons_pid[i], &status, WNOHANG);
    		}
			break;
		}
		else// Not all jobs have been created/completed
			pthread_mutex_unlock(&sm->mutex);

	}
	//cout<<prod_pid.size()<<" "<<cons_pid.size()<<endl;
	shmdt(sm);// Detaching shared memory from address space of parent
	return 0;
}

// Function to create jobs by the producer
void producer_generate_job(int proc_id, shared_memory *sm, int  prod_number, int jobs)
{
		
	while(1)
	{
		pthread_mutex_lock(&sm->mutex);// Prevent race
		
		if(sm->job_created>=jobs)// All jobs created
		{
			pthread_mutex_unlock(&sm->mutex);
			break;
		}

		if(sm->curr_size<PQ_SIZE-1)// Ensure that priority queue is not full
		{
			//int wait = rand()%4;
			//sleep(wait);
			job new_job = create_job(proc_id, prod_number);
			sm->job_created++;

			pthread_mutex_unlock(&sm->mutex);

			std::random_device rd;  //Will be used to obtain a seed for the random number engine
    		std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    		std::uniform_int_distribution<> distrib3(0, 3); // random number in [1,4]
			int wait = distrib3(gen);
			sleep(wait);// Random sleep

			int flag = 1;
			while(flag)
			{
				pthread_mutex_lock(&sm->mutex);

				if(sm->curr_size<PQ_SIZE-1)
				{// Display details of job created
					cout<<"Created Job Details :"<<endl;
					cout<<"Producer Number: "<<prod_number<<" Producer pid: "<<proc_id<<" Job ID: "<<new_job.job_id;
					cout<<" Priority: "<<new_job.priority<<" Compute Time: "<<new_job.comp_time<<endl;
					
					insert_job(new_job, sm);// Insert job into queue
					flag = 0;
				}

				pthread_mutex_unlock(&sm->mutex);
			}
		}
		else
			pthread_mutex_unlock(&sm->mutex);// Release the lock for the shared memory
	}
	//pthread_mutex_unlock(&sm->mutex);
	return;
}

// Function to consume jobs by the consumer
void consumer_finish_job(int proc_id, shared_memory *sm, int  con_number, int jobs)
{
	while(1)
	{
		std::random_device rd;  //Will be used to obtain a seed for the random number engine
    	std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    	std::uniform_int_distribution<> distrib3(1, 4); // random number in [1,4]
		int wait = distrib3(gen);
		sleep(wait);// Random sleep
		pthread_mutex_lock(&sm->mutex);// Preventing race conditions
		
		if(sm->job_completed>=jobs)// All jobs completed
		{
			pthread_mutex_unlock(&sm->mutex);
			break;
		}

		if(sm->curr_size>0)
		{
			
			job rem_job = remove_job(sm);// Extract highest priority job from priority queue
			
			// Details of removed job
			cout<<"Job removed, details: "<<endl;
			cout<<"Consumer No: "<<con_number<<" Consumer pid: "<<proc_id<<" Producer No: "<<rem_job.prod_num;
			cout<<" Producer pid: "<<rem_job.proc_id<<" Priority: "<<rem_job.priority<<" Compute Time: "<<rem_job.comp_time;
			cout<<" Job ID: "<<rem_job.job_id<<endl;
			sm->job_completed++;
			
			pthread_mutex_unlock(&sm->mutex);
			int comp_time = rem_job.comp_time;
			sleep(comp_time);// Sleep for the comp_time duration

			//pthread_mutex_unlock(&sm->mutex);
		}
		else
			pthread_mutex_unlock(&sm->mutex);// Releasing the lock for the shared memory
	}
	return;	
}

// Function to create a new job
job create_job(int proc_id, int prod_num)
{
	job new_job;
	new_job.proc_id = proc_id;
	new_job.prod_num = prod_num;
	std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib1(1, 100000);// Random number in [1, 100000]
    std::uniform_int_distribution<> distrib2(1, 10);// Random number in [1, 10]
    std::uniform_int_distribution<> distrib3(1, 4);// Random number in [1, 4]
	new_job.job_id = distrib1(gen);
	new_job.priority = distrib2(gen);
	new_job.comp_time = distrib3(gen);

	return new_job;
}

// Insert new job into priority queue of shared memory
void insert_job(job new_job, shared_memory *sm)
{
	if(sm->curr_size==0)// Base condition
	{
		sm->curr_size++;
		int size = sm->curr_size;
		sm->priority_q[size] = new_job;
	}
	else// Standard MAX-HEAP Insertion
	{
		sm->curr_size++;
		int size = sm->curr_size;
		sm->priority_q[size] = new_job;
		int index = size;
		while(index>1)
		{
			if(sm->priority_q[index/2].priority<sm->priority_q[index].priority)
			{
				job  swap = sm->priority_q[index];
				sm->priority_q[index] = sm->priority_q[index/2];
				sm->priority_q[index/2] = swap;
				index = index/2;
			}
			else
				break;
		}
	}
}

// Remove highest priority job from priority queue
job remove_job(shared_memory *sm)
{
	if(sm->curr_size==1)// Base case
	{
		int size = sm->curr_size;
		job rem = sm->priority_q[1];
		sm->curr_size = 0;
		return rem;
	}
	else// Standard MAX-HEAP extractMax operation
	{
		int size = sm->curr_size;
		job rem = sm->priority_q[1];
		job last = sm->priority_q[size];
		sm->curr_size = size-1;
		sm->priority_q[1] = last;
		int index = 1;
		while(index<size)
		{
			int target = index;
			if(2*index<=sm->curr_size && sm->priority_q[target].priority<sm->priority_q[2*index].priority)
				target = 2*index;
			if(2*index+1<=sm->curr_size && sm->priority_q[target].priority<sm->priority_q[2*index+1].priority)
				target = 2*index+1;
			if(target==index)
				break;
			else
			{
				job swap = sm->priority_q[target];
				sm->priority_q[target] = sm->priority_q[index];
				sm->priority_q[index] = swap;

				index = target;
			}
		}
		return rem;
	}
}

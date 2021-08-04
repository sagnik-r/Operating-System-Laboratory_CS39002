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
#include <vector>
#include <thread>
using namespace std; 

// Size of priority queue
#define PQ_SIZE 8

// Structure for Jobs
typedef struct{
	pthread_t thrd_id;
	int prod_num;
	int priority;
	int comp_time;
	int job_id;
}job;

// Shared memory across threads
typedef struct{
	job  priority_q[PQ_SIZE];
	int job_created;
	int job_completed;
	int curr_size;
	int jobs;
}shared_memory;

job create_job(int proc_id, int prod_num);
job remove_job(shared_memory *sm);
void insert_job(job new_job, shared_memory *sm);
void *producer_generate_job(void *num);
void *consumer_finish_job(void *num);

/* Decalaring shared memory and mutex as global variables fpr thread sharing */
shared_memory *sm;
pthread_mutex_t mutex;

int main()
{
	// Taking required input
	int cons, prods, jobs;
	cout<<"Enter the number of producers: ";
	cin>>prods;
	cout<<"Enter the number of consumers: ";
	cin>>cons;
	cout<<"Enter the number of jobs: ";
	cin>>jobs;

	// Initializing the shared memory in main()
	sm = (shared_memory *)malloc(sizeof(shared_memory));
	sm->job_created = 0;
	sm->job_completed = 0;
	sm->curr_size = 0;
	sm->jobs = jobs;
	pthread_mutex_init(&mutex, NULL);// Initializing mutex

	// Array of producer and consumer threads
	pthread_t prod_threads[prods];
	pthread_t cons_threads[cons];

	// Getting default attributes
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	// For passing the producer and consumer number as parameter to the thread function
	int prod_number[prods];
	int cons_number[cons];

	auto start_time = std::chrono::high_resolution_clock::now();// Recording the start time

	// Creating the producer threads
	for(int i=0; i<prods; i++)
	{
		prod_number[i] = i+1;
		// Creating the threads and parameter is the producer number
		pthread_create(&prod_threads[i], &attr, producer_generate_job, (void *)&prod_number[i]);
	}

	// Creating the consumer threads
	for(int i=0; i<cons; i++)
	{
		cons_number[i] = i+1;
		// Creating the threads and the parameter is the consumer number
		pthread_create(&cons_threads[i], &attr, consumer_finish_job, (void *)&cons_number[i]);
	}
	
	while(1)
	{
		pthread_mutex_lock(&mutex);// Preventing race conditions
		if(sm->job_created==jobs && sm->job_completed==jobs)// All jobs created and completed
		{

			auto end_time = std::chrono::high_resolution_clock::now();
    		auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);// Getting total time
    		cout<<"Total time taken: "<<elapsed.count()*1e-9<<endl;
    		pthread_mutex_unlock(&mutex);
    		for(int i=0; i<prods;i++)// Terminating producer threads
    		{
    			pthread_cancel(prod_threads[i]);
    		}
    		
    		for(int i=0; i<cons; i++)// Terminating consumer threads
    		{
    			pthread_cancel(cons_threads[i]);
    		}
			break;
		}
		else
			pthread_mutex_unlock(&mutex);// Releasing the lock if all threads not yet created/consumed

	}
	//cout<<prod_pid.size()<<" "<<cons_pid.size()<<endl;
	return 0;
}

// Function associated with producer threads
void *producer_generate_job(void *num)
{
	int p_num = *((int *)num);	// Getting the producer number
	while(1)
	{
		pthread_mutex_lock(&mutex);// Preventing race conditions
		
		if(sm->job_created>=sm->jobs)
		{
			pthread_mutex_unlock(&mutex);// All jobs created
			break;
		}

		if(sm->curr_size<PQ_SIZE-1)
		{
			pthread_t thrd_id = pthread_self(); 
			job new_job = create_job(thrd_id, p_num);// Create a new job
			sm->job_created++;
			pthread_mutex_unlock(&mutex);

			thread_local std::random_device rd;  //Will be used to obtain a seed for the random number engine
    		thread_local std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    		std::uniform_int_distribution<> distrib3(0, 3); // random number in [1,4]
			int wait = distrib3(gen);
			sleep(wait);// Random sleep

			int flag = 1;
			while(flag)
			{
				pthread_mutex_lock(&mutex);
				if(sm->curr_size<PQ_SIZE-1)
				{
					cout<<"Created Job, Details :"<<endl;
					cout<<"Producer Number: "<<p_num<<" Producer tid: "<<thrd_id<<" Job ID: "<<new_job.job_id;
					cout<<" Priority: "<<new_job.priority<<" Compute Time: "<<new_job.comp_time<<endl;
					
					insert_job(new_job, sm);// Insert into priority queue of shared memory
					flag = 0;
				}

				pthread_mutex_unlock(&mutex);	
			}
		}
		else
			pthread_mutex_unlock(&mutex);// Releasing the lock
	}
	//pthread_mutex_unlock(&sm->mutex);
	return 0;
}

// Thread associated with consumer processes
void *consumer_finish_job(void *num)
{
	int c_num = *((int *)num);
	while(1)
	{
		thread_local std::random_device rd;  //Will be used to obtain a seed for the random number engine
    	thread_local std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    	std::uniform_int_distribution<> distrib3(1, 4); // random number in [1,4]
		int wait = distrib3(gen);
		sleep(wait);// Random sleep
		pthread_mutex_lock(&mutex);
		if(sm->job_completed>=sm->jobs)// All jobs completed
		{
			pthread_mutex_unlock(&mutex);
			break;
		}

		if(sm->curr_size>0)
		{	
			job rem_job = remove_job(sm);// Extract highest priority job
			
			cout<<"Job removed, details: "<<endl;
			cout<<"Consumer No: "<<c_num<<" Consumer tid: "<<pthread_self()<<" Producer No: "<<rem_job.prod_num;
			cout<<" Producer tid: "<<rem_job.thrd_id<<" Priority: "<<rem_job.priority<<" Compute Time: "<<rem_job.comp_time;
			cout<<" Job ID: "<<rem_job.job_id<<endl;
			sm->job_completed++;

			pthread_mutex_unlock(&mutex);			
			int comp_time = rem_job.comp_time;
			sleep(comp_time);// Sleep for comp_time

		}
		else
			pthread_mutex_unlock(&mutex);// Release the lock
	}
	return 0;	
}

// Create a new job
job create_job(int thrd_id, int prod_num)
{
	job new_job;
	new_job.thrd_id = thrd_id;
	new_job.prod_num = prod_num;
	thread_local std::random_device rd;  //Will be used to obtain a seed for the random number engine
    thread_local std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib1(1, 100000);// Random number in [1, 100000]
    std::uniform_int_distribution<> distrib2(1, 10);// Random number in [1, 10]
    std::uniform_int_distribution<> distrib3(1, 4);// Random number in [1, 4]
	new_job.job_id = distrib1(gen);
	new_job.priority = distrib2(gen);
	new_job.comp_time = distrib3(gen);

	return new_job;
}

// Insert a new job in the priority queue
void insert_job(job new_job, shared_memory *sm)
{
	if(sm->curr_size==0)// Base condition
	{
		sm->curr_size++;
		int size = sm->curr_size;
		sm->priority_q[size] = new_job;
	}
	else// Standard MAX Heap insertion
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

// Extract maximum priority jib from priority queue
job remove_job(shared_memory *sm)
{
	if(sm->curr_size==1)// Base Case
	{
		int size = sm->curr_size;
		job rem = sm->priority_q[1];
		sm->curr_size = 0;
		return rem;
	}
	else// Standard Max Heap Extract Max Operation
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

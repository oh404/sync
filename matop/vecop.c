/* A2 Vector Operations
 * Decription: For the purpose of convenience and code readability, the "stocks" from 
 * the template are moved to the vectorop-uils.h, in particular: definition of constats,
 * variables, and other functions that are initially given. Generally, the functions inside
 * vectorop-utils.h are unchanged, except the delaration of monitor variables and adding mutex
 * to show_buffer() - to make the printing the buffer+sanity check properly (i.e. atomic)
 * 
 * This runable main file contains the edited functions: main, thread runner,
 * monitor's initiation, destroy, API, etc.;
 */

#include "vecop-utils.h"

// To exit from the FOREVER loop, using MAX_ITERATIONS to exit and test the cleanup (destroy):
#define MAX_ITERATIONS 100
// either of two needs to be uncommented:
#define FOREVER for(;;)                                   // option #1: run forever
//#define FOREVER for(int i = 0; i < MAX_ITERATIONS; i++) // option #2: force to exit after # iters
sem_t semaphore;                // semaphore to force exit as one of the threads reach MAX_ITERS

// Defining Synchronization Rule: 0 - FCFS: 1 - SVF 2 - LVF
#define FCFS 0
#define SVF 1
#define LVF 2
#define QUEUERULE FCFS

// IMPLEMENTATION OF MONITOR API
// download copies a vector of size up to k to V
void download(monitor_t *mon, int k, vector_t *V) {
	pthread_mutex_lock(&mon->m);

    // creating separate queue, depending on the size of operating vectors (10, 5, 3):
    // the next size shall not be zero, and needs to be suitable for thread:
	if (k == 10)
	{
		mon->counter_d_q_10++; // queue counters
		while (next_size(mon) == 0 || next_size(mon) > k) pthread_cond_wait(&mon->download_q_10, &mon->m);
		mon->counter_d_q_10--;
	}
	else if (k == 5)
	{
		mon->counter_d_q_5++;
		while (next_size(mon) == 0 || next_size(mon) > k) pthread_cond_wait(&mon->download_q_5, &mon->m);
		mon->counter_d_q_5--;
	}
	else if (k == 3)
	{
		mon->counter_d_q_3++;
		while (next_size(mon) == 0 || next_size(mon) > k) pthread_cond_wait(&mon->download_q_3, &mon->m);
		mon->counter_d_q_3--;
	}
	else abort();

    // perform download:
	from_buffer(mon,V);
    // priting queue, pending threads counters:
	printf("====================================================================== DL: size_10: %i; size_5: %i; size_3: %i; =====\n", mon->counter_d_q_10, mon->counter_d_q_5, mon->counter_d_q_3);

	// Queueing rules signaling: depending on the size.
    if (QUEUERULE == LVF)
    {
        if (mon->counter_u_q_10 > 0) pthread_cond_signal(&mon->upload_q_10);
        else if (mon->counter_u_q_5 > 0) pthread_cond_signal(&mon->upload_q_5);
        else if (mon->counter_u_q_3 > 0) pthread_cond_signal(&mon->upload_q_3);
    }
    else if (QUEUERULE == SVF)
    {
        if (mon->counter_u_q_3 > 0) pthread_cond_signal(&mon->upload_q_3);
        else if (mon->counter_u_q_5 > 0) pthread_cond_signal(&mon->upload_q_5);
        else if (mon->counter_u_q_10 > 0) pthread_cond_signal(&mon->upload_q_10);
    }

	pthread_mutex_unlock(&mon->m);
}

void upload(monitor_t *mon, vector_t *V) {
	pthread_mutex_lock(&mon->m);

    // Depending on the queuing policy: going to required queue:
    if (QUEUERULE == FCFS)
    {
		if(size_of(V)-1 == 10) mon->counter_u_q_10++;
		else if (size_of(V)-1 == 5) mon->counter_u_q_5++;
		else if (size_of(V)-1 == 3) mon->counter_u_q_3++;

        // Start and end of the queue, each arriving thread gets the lowest position:
		int thread_index = mon->counter_fcfsq_end % N_THREADS;
		mon->counter_fcfsq_end++;

		while (size_of(V) > capacity(mon)) {
			pthread_cond_wait(&mon->fcfs_q[thread_index], &mon->m);
		}
        
		if(size_of(V)-1 == 10) mon->counter_u_q_10--;
		else if (size_of(V)-1 == 5) mon->counter_u_q_5--;
		else if (size_of(V)-1 == 3) mon->counter_u_q_3--;
    }
    else if (QUEUERULE == LVF)
    {
		if(size_of(V)-1 == 10)
		{
            // queueuing depending on the operating size:
            mon->counter_u_q_10++;
			while(size_of(V) > capacity(mon)) pthread_cond_wait(&mon->upload_q_10, &mon->m);
            mon->counter_u_q_10--;
		}
		else if (size_of(V)-1 == 5)
		{
            mon->counter_u_q_5++;
			while(size_of(V) > capacity(mon)) pthread_cond_wait(&mon->upload_q_5, &mon->m);
            mon->counter_u_q_5--;
		}
		else if (size_of(V)-1 == 3)
		{
            mon->counter_u_q_3++;
			while(size_of(V) > capacity(mon)) pthread_cond_wait(&mon->upload_q_3, &mon->m);
            mon->counter_u_q_3--;
		}
		else abort();
    }
    else if (QUEUERULE == SVF)
    {
		if(size_of(V)-1 == 3)
		{
            mon->counter_u_q_3++;
			while(size_of(V) > capacity(mon)) pthread_cond_wait(&mon->upload_q_3, &mon->m);
            mon->counter_u_q_3--;
		}
		else if (size_of(V)-1 == 5)
		{
            mon->counter_u_q_5++;
			while(size_of(V) > capacity(mon)) pthread_cond_wait(&mon->upload_q_5, &mon->m);
            mon->counter_u_q_5--;
		}
		else if (size_of(V)-1 == 10)
		{
            mon->counter_u_q_10++;
			while(size_of(V) > capacity(mon)) pthread_cond_wait(&mon->upload_q_10, &mon->m);

            mon->counter_u_q_10--;
		}
		else abort();
    }

    // perform upload:
	to_buffer(mon,V);
    
    // pring queue counter, pending threads:
	if (QUEUERULE == FCFS) printf("=========================================================================== UP FCFS: size_10: %i; size_5: %i; size_3: %i; =====\n", mon->counter_u_q_10, mon->counter_u_q_5, mon->counter_u_q_3);
	else printf("====================================================================== UP: size_10: %i; size_5: %i; size_3: %i; =====\n", mon->counter_u_q_10, mon->counter_u_q_5, mon->counter_u_q_3);

    // signal the queues: the largest size-operating thread will be first:
    if (mon->counter_d_q_10 > 0) pthread_cond_signal(&mon->download_q_10);
    else if (mon->counter_d_q_5 > 0) pthread_cond_signal(&mon->download_q_5);
    else if (mon->counter_d_q_3 > 0) pthread_cond_signal(&mon->download_q_3);

	if (QUEUERULE == FCFS)
	{
        // signaling next arived thread in FCFS queue to wake up:
		pthread_cond_signal(&mon->fcfs_q[(mon->counter_fcfsq_start) % N_THREADS]);
    	mon->counter_fcfsq_start++; // incrementing position of end of queue
	}

	pthread_mutex_unlock(&mon->m);
}

void init_buffer(monitor_t *mon) {
    /* Initially, the buffer is empty, hence, the threads won't run until something is put into
    * the monitor's buffer. Generating randomly vectors and putting them into buffer up until
    * some arbitrary capacity value: */

	vector_t Vinited;
	while(capacity(mon) >= 12)
	{
		init_vector(&Vinited);
		to_buffer(mon, &Vinited);
	}
}

void monitor_init(monitor_t *mon) {
    // initializing the monitor variables:
	mon->in = 0;
	mon->out = 0;
	mon->next_size = 0;
	mon->capacity = BUFFER_SIZE;

    mon->counter_u_q_10 = 0;
    mon->counter_u_q_5 = 0;
    mon->counter_u_q_3 = 0;
    mon->counter_d_q_10 = 0;
    mon->counter_d_q_5 = 0;
    mon->counter_d_q_3 = 0;
	mon->counter_fcfsq_end = 0;
    mon->counter_fcfsq_start = 0;

	// CONDVARS:
	pthread_mutex_init(&mon->m,NULL);
	for (int i = 0; i < N_THREADS; i++) pthread_cond_init(&mon->fcfs_q[i], NULL);
    pthread_cond_init(&mon->upload_q_3, NULL);
    pthread_cond_init(&mon->upload_q_5, NULL);
    pthread_cond_init(&mon->upload_q_10, NULL);
    pthread_cond_init(&mon->download_q_3, NULL);
    pthread_cond_init(&mon->download_q_5, NULL);
    pthread_cond_init(&mon->download_q_10, NULL);
}


void monitor_destroy(monitor_t *mon) {
	// perform clean-up:
	for (int i = 0; i < N_THREADS; i++) pthread_cond_destroy(&mon->fcfs_q[i]);
    pthread_cond_destroy(&mon->upload_q_3);
    pthread_cond_destroy(&mon->upload_q_5);
    pthread_cond_destroy(&mon->upload_q_10);
    pthread_cond_destroy(&mon->download_q_10);
    pthread_cond_destroy(&mon->download_q_3);
    pthread_cond_destroy(&mon->download_q_5);
    pthread_mutex_destroy(&mon->m);
}

int main(void) {
    pthread_t my_threads[N_THREADS];
    thread_name_t my_thread_names[N_THREADS];
    srand(42); // random seed

    sem_init(&semaphore, 1, 0); // shared semaphore for exiting
    monitor_init(&mon);
    init_buffer(&mon);          // initializing the buffer values

    for(int i = 0; i < N_THREADS; i++) 
    {
        sprintf(my_thread_names[i],"#%i",i);
        // create N_THREADS thread with same entry point 
        // these threads are distinguishable thanks to their argument (their name: "#1", "#2", ...)
        // thread names can also be used inside threads to show output messages
        pthread_create(&my_threads[i], NULL, thread, my_thread_names[i]);
    }
    
    sem_wait(&semaphore); // in case if it is not running forever, at reaching MAX_ITERS forcing to exit

    // not a very good practice to exit, but since the exit condition and requrements are not
    // specified by the task, just forcing to exit in this way:
    for(int i = 0; i < N_THREADS; i++) pthread_cancel(my_threads[i]);

    // cleanup, release and exit:
	monitor_destroy(&mon);
    return EXIT_SUCCESS;
}

// Thread runner funtion:
void *thread(void *arg) {
    char *name=(char *)arg;     // name of the thread
	vector_t Vin, Vout;         // working vector
	int k=rand_size(), o=rand_size();
	matrix_t M;

	init_matrix(&M,k,o);
	show_matrix(&M);

    FOREVER
    {
		download(&mon,k,&Vin);
		printf("Thread %s downloaded ", name); show_vector(&Vin);
		multiply(&M,&Vin,&Vout);
		printf("Thread %s obtained ", name); show_vector(&Vout);
		upload(&mon,&Vout);
		printf("Thread %s updated buffer ", name); show_vector(&Vout);
		show_buffer(&mon);
		spend_some_time(MIN_LOOPS+rand()%(WAIT_LOOPS+1)); // optionally, to add some randomness and slow down output
    }

    sem_post(&semaphore); // in case if we have forced exit

    pthread_exit(NULL);
}


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <semaphore.h>
#include <unistd.h>

// CONSTANTS AND MACROS
// for readability
#define N_THREADS 15

#define BUFFER_SIZE 30 // buffer size
#define MAX_VSIZE 10 // max size of vectors (possible sizes: 3, 5, 10)

#define WAIT_LOOPS 10
#define MIN_LOOPS 5

// for simplicity, vectors are always size 10 and matrices are 10x10
// then, part of the space will be unused, no big deal
typedef struct vector_t {
	int size;
	int data[MAX_VSIZE];
} vector_t;

typedef struct matrix_t {
	int n, m;
	int data[MAX_VSIZE][MAX_VSIZE];
} matrix_t;

// DEFINITIONS OF NEW DATA TYPES
// for readability
typedef char thread_name_t[10];

// acronyms for policies
typedef enum boolean {FALSE, TRUE} boolean;

 // monitor also defined as a new data types
typedef struct monitor_t {
    // shared data to manage
    int buffer[BUFFER_SIZE];
    int in, out;
    // the following integers are for better readability
    int next_size; // the size of the next vector; 0 if empty buffer
    int capacity; // the size of the longest V that can be uploaded to the buffer

    /* add here anything else you need to be in the monitor for synchronization */

	// counters:
    int counter_u_q_10, counter_u_q_5, counter_u_q_3;
	int counter_d_q_10, counter_d_q_5, counter_d_q_3;

    //FCFS:
    int counter_fcfsq_start;
    int counter_fcfsq_end;

	// condvars:
	pthread_cond_t fcfs_q[N_THREADS];
    pthread_cond_t upload_q_3, upload_q_5, upload_q_10;
	pthread_cond_t download_q_10, download_q_5, download_q_3;

	pthread_mutex_t m;
} monitor_t;

// GLOBAL VARIABLES
// the monitor should be defined as a global variable
monitor_t mon;
int next_value=1;

//  MONITOR API
void download(monitor_t *mon, int k, vector_t *V);
void upload(monitor_t *mon, vector_t *V);
void monitor_init(monitor_t *mon);
void monitor_destroy(monitor_t *mon);
void *thread(void *arg);
double spend_some_time(int);

int next_size(monitor_t *mon) {
	return mon->next_size;
}
// returns the max length of a buffer that can be uploaded to the vector
int capacity(monitor_t *mon) {
	return mon->capacity;
}
// returns the size of a vector
int size_of(vector_t *V) {
	return V->size+1;
}

// puts a vector in the buffer; assumes that there is enough capacity
void to_buffer(monitor_t *mon, vector_t *V) {
	int i;
	// if buffer is empty, set next_size to size of V's data
	if(mon->next_size==0)
		mon->next_size=V->size;
	// subtract from the buffer's capacity the size of V
	mon->capacity-=size_of(V);
	// first slot is size of V's data
	mon->buffer[mon->in]=V->size;
	// then copy each value to buffer
	for(i=0;i<V->size;i++) {
		mon->in=(mon->in+1)%BUFFER_SIZE;
		mon->buffer[mon->in]=V->data[i];
	}
	// set in to next empty slot in buffer
	mon->in=(mon->in+1)%BUFFER_SIZE;
	//printf("produced V_%d\n", V->size);
}

// takes a vector from the buffer; assumes that the buffer is not empty
void from_buffer(monitor_t *mon, vector_t *V) {
	int i;
	// initialize size with first integer to be read from buffer
	V->size=mon->buffer[mon->out];
	// copy one by one all values from buffer to V
	for(i=0;i<V->size;i++) {
		mon->out=(mon->out+1)%BUFFER_SIZE;
		V->data[i]=mon->buffer[mon->out];
	}
	// set out to next position in buffer
	mon->out=(mon->out+1)%BUFFER_SIZE;
	// increase the buffer's capacity by the size of V
	mon->capacity+=size_of(V);
	// if the buffer is empty, set next_size to 0
	if(mon->capacity==BUFFER_SIZE)
		mon->next_size=0;
	// otherwise, set next_size to the size of the next vector in the buffer
	else
		mon->next_size=mon->buffer[mon->out];
	//printf("consumed V_%d\n", V->size);
}

// generate a random vector size
int rand_size() {
	int r;
	r = rand()%3;
	if(r==0) return 3;
	else if(r==1) return 5;
	else return 10;
}

// initialize a vector with consecutive numbers
void init_vector(vector_t *V) {
	int i, size;

	size=rand_size();
	V->size=size;
	for(i=0;i<size;i++)
		V->data[i]=next_value++;
}

// initialize a matrix, more or less randomly

void init_matrix(matrix_t *M, int n, int m) {
    M->n = n;
    M->m = m;
    for(int i = 0; i < m; i++) {
        for(int j = 0; j < n; j++)
            M->data[i][j] = rand() % 2 ? -1 : 1;
    }

    for(int i = m; i < MAX_VSIZE; i++) {
        for(int j = 0; j < MAX_VSIZE; j++)
            M->data[i][j] = 0;
    }
}

void show_vector(vector_t *V) {
	int i;
	printf("vector of size %d:\n", V->size);
	for(i=0;i<V->size;i++)
		printf("%d\t", V->data[i]);
	puts("");
}

void show_matrix(matrix_t *M) {
	int i,j;
	printf("matrix %dx%d:\n", M->m, M->n);
	for(i=0;i<M->m;i++) {
		for(j=0;j<M->n;j++)
			printf("%d\t", M->data[i][j]);
		puts("");
	}
}

// not quite a multiplication, but something similar to it
void multiply(matrix_t *M, vector_t *Vin, vector_t *Vout) {
	int i,j,max=0;
	Vout->size=M->m;
	for(i=0;i<Vout->size;i++)
		Vout->data[i]=0;
	for(i=0;i<M->m;i++) {
		for(j=0;j<M->n;j++)
			Vout->data[i]+=M->data[i][j]*Vin->data[j];
		if(Vout->data[i]>max)
			max=Vout->data[i];
	}
	max = max/2;
	if(max)
		for(i=0;i<M->m;i++)
			Vout->data[i]=(int)(Vout->data[i]/max);
}

boolean sanity_check(monitor_t *mon) {
	int steps, index, skip;
	boolean result=TRUE;
	if(mon->next_size==0) {
		if(mon->capacity!=BUFFER_SIZE)
			result = FALSE;
	}
	else {
		steps = BUFFER_SIZE-mon->capacity;
		index = mon->out;
		while(steps>0) {
			printf("sanity_check: position %d", index);
			skip = mon->buffer[index];
			printf(" d_%d\n", skip);
			if(( skip!=3 )&& (skip !=5 )&&( skip != 10) )
				result = FALSE;
			else {
				steps-=(skip+1);
				index=(index+skip+1)%BUFFER_SIZE;
			}
		}
		if(steps!=0)
			result = FALSE;
	}
	return result;
}

// display the state of the monitor
void show_buffer(monitor_t *mon) {
	pthread_mutex_lock(&mon->m);

	int i=BUFFER_SIZE-mon->capacity, j=mon->out;
	printf("Remaining capacity: %d (%.0f%%)\nContent:\n", mon->capacity, (double)100*mon->capacity/BUFFER_SIZE);
	while(i>0) {
		printf("%d\t",mon->buffer[j]);
		j=(j+1)%BUFFER_SIZE;
		i--;
	}
    puts("");

	printf("Monitor sanity checked %s\n", sanity_check(mon)?"passed":"failed");
	pthread_mutex_unlock(&mon->m);
}

double spend_some_time(int max_steps) {
    double x, sum=0.0, step;
    long i, N_STEPS=rand()%(max_steps*1000000);
    step = 1/(double)N_STEPS;
    for(i=0; i<N_STEPS; i++) {
        x = (i+0.5)*step;
        sum+=4.0/(1.0+x*x);
    }
    return step*sum;
}

/*
 * slave.c
 *
 *  Created on: Jan 14, 2024
 *      Author: bd
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>

#define N_THREADS 5 // threads num

// Init:
sem_t semaphore;                                    // semaphore
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER; // condvar

int thread_counter = 0;     // counter of the threads
int terminated = 0;         // termination flag
int buffer[10];             // our buffer
int N;                      // Number to write into buffer


// static allocaton of the mutex helps us for sure to be sure hat our mutex is init,
// because it's must be initialized before we run our threds, and other manipulations

void *writer(void *arg) {
    /* Writers' code: 
    While termination flag is unchanged, each of the N_THREADS writers wakes up, 
    requests the mutex and performs the writing operation based on the received 
    number */
    while(!terminated)
    {
        pthread_mutex_lock(&mutex);
        
        // Conidional variable:
        while (thread_counter == 0)
        {
            pthread_cond_wait(&cond_var,&mutex);
        }
        thread_counter--; // decrementing from 5

        if (N != -1) // peforming write into buffer:
        {
            int random = rand() % 10;
            printf("buffer[%i] += %i\n", random, N);
            buffer[random] = buffer[random] + N;
        }

        if (thread_counter == 0) // upon the last writer finishing, signaling semaphore
        {
            puts("Five writers finished\n");
            sem_post(&semaphore);
        }

        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL); // exiting thread upon changing flag
}

int main(int argc, char *argv[]) {
    // setting up random:
    srand(1);

    pthread_t tid[N_THREADS];   // reader threads
    int fd;                     // filedescriptor
    char *fifo_buffer = "/tmp/RTSAbd";  // FIFO pipe location

    puts("=== SLAVE Process: started ===\n");
    // zeroing the buffer, compilers usually do it by themselves, but still:
    for (int i = 0; i < 10; i++) buffer [i] = 0; 

    // semaphore to ensure exactly five threads are performing at on input:
    sem_init(&semaphore, 1, 1); 
    
    for (int i = 0; i < N_THREADS; i++) 
    {
        // thread creation:
        if(pthread_create(&tid[i], NULL, writer, NULL) != 0)
        {
            perror("Error Creating Threads!");
            abort();
        }
    }

    // opening the buffer in the read only mode:
    fd = open(fifo_buffer, O_RDONLY);

    while(!terminated)
    {
        int received_data;

        // waiting to semaphore to signal, the signal is performed upon N_THREADS finish:
        sem_wait(&semaphore);
        
        // reading the value from buffer and printing it:
        read(fd, &received_data, sizeof(received_data)); 
        printf("FIFO: rec %d\n", received_data);

        // requesting mutex:
        pthread_mutex_lock(&mutex);
        if (received_data == -1)
        {
            // -1: used as termination value:
            puts("-1: termination");
            terminated = 1;
        }

        N = received_data;
        thread_counter = N_THREADS;        // to wake up exactly 5 threads
        pthread_cond_broadcast(&cond_var); // broadcasting to threads the condvar
        puts("--wakeup sent--");
        pthread_mutex_unlock(&mutex);
    }

    // upon chaniging flag, perofming exit:
    puts("=== Slave: Termination: ===\n");
    close(fd);

	for (int i = 0; i < N_THREADS; i++) 
    {
        // Joining the writing threads:
        if (pthread_join(tid[i], NULL) != 0)
        {
            perror("Error Jointing Threads!");
            abort();
        }
	}

    // Finally printing the buffer:
    puts("Final buffer:");
    for (int i = 0; i < 10; i++) {
        printf("buffer[%i] = %i\n",i, buffer[i]);
	}

    // Perofrming  cleanup and exiting:
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_var);
    sem_destroy(&semaphore);
    puts("=== SLAVE Process: exit & cleanup ===\n");
    return EXIT_SUCCESS;
}

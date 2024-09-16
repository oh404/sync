/*
 * master.c
 *
 *  Created on: Jan 14, 2024
 *      Author: bd
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>

// Init:
sem_t semaphore;    // semaphore
pid_t pid;          // child

int terminated = 0; // termination flag

void handler(int signo) {
    /* Handles the Signal that will be received from the user terminal */
	if(signo==SIGUSR1) {
        printf("Signal: SIGUSR1, signaling SEM");
        sem_post(&semaphore); 
	}
	else {
		perror("Wrong signal number");
		abort();
	}
}

int main(int argc, char *argv[]) {
    // Random Seed:
    srand(1);

    // Initializing the semaphore at 0 for signal handling:
    sem_init(&semaphore, 1, 0); 

    puts("=== MASTER Process: startup ===\n");
    pid = fork(); // perform fork, create child parent-child processes:

    if (pid == -1)
    {
        // fork() return -1: error of creation
        perror("Error Creating Child\n");
        exit(1);
    }
    else if (pid == 0)
    {
        // fork() return 0: the child performing
        int fd;                             // filedescriptor
        int accumulator = 0, payload = 0;   // accumulator of values generated and sent
        char *fifo_pipe    = "/tmp/RTSAbd"; // FIFO buffer location
        
        /* Initializing the FIFO named pipe: unlinking first, in case if not properly
        closed on the previous runs, to remove and begin from clean buffer:  */
        unlink(fifo_pipe);
        mkfifo(fifo_pipe, 0666);            // allowing execution
        fd = open(fifo_pipe, O_WRONLY);     // opening on write only mode

        while (!terminated)
        {
            /* executing this part until termination flag changes to zero:
            1) the SIGUSR1 is awaited and the main is put on wait with semaphore 
            2) upon receiving SIGUSR1, the handler signals the semaphore allowing 
            to proceed further the code
            3) generating random number in range of [0-19] (int payload)
            4) accumulating the generated number
            5) write the generated number into FIFO
            6) check whether the accumulator reached desired value or no
            7)
            if reached:
                - sending -1 to thorugh the FIFO to signal the finish
                - change the termination flag 
            if no:
                - go back with the loop 
            8) perform exit and close filedesc, removing and cleaning pipe
            */
            signal(SIGUSR1, handler);
            sem_wait(&semaphore);
            puts("\nSEM: Generating number");

            payload = rand()%20;
            accumulator += payload;
            
            write(fd, &payload, sizeof(payload));
            printf("FIFO: sent %d\n\n", payload);

            if (accumulator > 100)
            {
                terminated = 1;
                payload = -1;
                write(fd, &payload, sizeof(payload));
            }
        }

        close(fd);
        unlink(fifo_pipe);
        puts("=== MASTER Process: child exit ===\n");
        return EXIT_SUCCESS;
    }
    else
    {
        // fork() return PID of child: the parent performing
        printf("Type <kill -%d %d> to generate # and send payload\n", SIGUSR1, pid);
        /* Parent waits print the PID of the child in the terminal and signum,
        after that goes into waiting of child termination. Upon termination, 
        it exits while loop, destroys the sem and performs exit finishing exec */
        while(waitpid(pid, NULL, WNOHANG) == 0);

        // cleanup:
        sem_destroy(&semaphore);

        puts("=== MASTER Process: parent exit & cleanup ===\n");
    }

	return EXIT_SUCCESS;
}

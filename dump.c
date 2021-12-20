#include <errno.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SHM_SIZE 4
#define SHM_NAME "memory"
#define PROJ_ID 0xDEADBEEF
#define N_SEMAPHORES 11

enum
{
    THE_ONLY_SENDER   	= 0,
    THE_ONLY_RECEIVER 	= 1,
    SENDER_CONNECT    	= 2,
    RECEIVER_CONNECT  	= 3,
    SUM_RECEIVERS     	= 4,
    SUM_SENDERS       	= 5,
    SUM_BOTH          	= 6,
	SUM_RECEIVERS_CONST = 7,
	SUM_SENDERS_CONST	= 8,
    EMPTY             	= 9,
    FULL              	= 10
};

void dump(int sem_id)
{
    printf("THE_ONLY_SENDER     = %d\n", \
				semctl(sem_id, THE_ONLY_SENDER,     GETVAL));

    printf("THE_ONLY_RECEIVER   = %d\n", \
				semctl(sem_id, THE_ONLY_RECEIVER,   GETVAL));

    printf("SENDER_CONNECT      = %d\n", \
				semctl(sem_id, SENDER_CONNECT,      GETVAL));

    printf("RECEIVER_CONNECT    = %d\n", \
				semctl(sem_id, RECEIVER_CONNECT,    GETVAL));

    printf("SUM_RECEIVERS       = %d\n", \
				semctl(sem_id, SUM_RECEIVERS,       GETVAL));

    printf("SUM_SENDERS         = %d\n", \
				semctl(sem_id, SUM_SENDERS,         GETVAL));

    printf("SUM_BOTH            = %d\n", \
				semctl(sem_id, SUM_BOTH,            GETVAL));

    printf("SUM_RECEIVERS_CONST = %d\n", \
				semctl(sem_id, SUM_RECEIVERS_CONST, GETVAL));

    printf("SUM_SENDERS_CONST   = %d\n", \
				semctl(sem_id, SUM_SENDERS_CONST,   GETVAL)); 

    printf("EMPTY               = %d\n", \
				semctl(sem_id, EMPTY,               GETVAL));

    printf("FULL                = %d\n", \
				semctl(sem_id, FULL,                GETVAL));
}

int main()
{   
    key_t key = ftok(SHM_NAME, PROJ_ID);
    if (key == -1)
    {
        perror("ftok ");
        return EXIT_FAILURE;
    }

    int sem_id = semget(key, N_SEMAPHORES, IPC_CREAT | 0666);
    if (sem_id < 0)
    {
        perror("semget ");
        return EXIT_FAILURE;
    }

    dump(sem_id);

    return EXIT_SUCCESS;
}

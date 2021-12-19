#include <errno.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHM_NAME "memory"
#define PROJ_ID 0xDEADBEEF
#define N_SEMAPHORES 9

int main()
{   
    key_t key = ftok(SHM_NAME, PROJ_ID);
    if (key == -1)
    {
        perror("ftok(): ");
        return EXIT_FAILURE;
    }

    int sem_id = semget(key, N_SEMAPHORES, IPC_CREAT | 0666);
    if (sem_id < 0)
    {
        perror("semget(): ");
        return EXIT_FAILURE;
    }

    if ( semctl (sem_id, 0, IPC_RMID, 0) < 0 )
    {
        printf("clear(): the semaphores have not been removed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

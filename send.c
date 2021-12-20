#include <errno.h>
#include <sys/sem.h>
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

const int POISON    = -1;
const int MAX_CNTR  = 100; // maximum possible value of the
                           // (sender + receiver)'s counter

enum
{
    THE_ONLY_SENDER     = 0,
    THE_ONLY_RECEIVER   = 1,
    SENDER_CONNECT      = 2,
    RECEIVER_CONNECT    = 3,
    SUM_RECEIVERS       = 4,
    SUM_SENDERS         = 5,
	SUM_BOTH			= 6,
    SUM_RECEIVERS_CONST = 7,
	SUM_SENDERS_CONST   = 8,
    EMPTY               = 9,
    FULL                = 10
};

union semun 
{
    int val;                
    struct semid_ds* buf;   
    unsigned short* array; 
};

int initSem(int sem_id, int sem_num, int val)
{
    union semun thisSem;
    thisSem.val = val;
    return semctl(sem_id, sem_num, SETVAL, thisSem);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s [filename]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = POISON;

    key_t key = ftok(SHM_NAME, PROJ_ID);
    if (key == -1)
    {
        perror("ftok(): ");
        return EXIT_FAILURE;
    }

    int shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shm_id < 0)
    {
        perror("shmget(): ");
        return EXIT_FAILURE;
    }

    int sem_id = semget(key, N_SEMAPHORES, IPC_CREAT | 0666);
    if (sem_id < 0)
    {
        perror("semget(): ");
        return EXIT_FAILURE;
    }

    struct sembuf theOnlySender[2];

    theOnlySender[0].sem_num = THE_ONLY_SENDER; // wait until there're
    theOnlySender[0].sem_op  = 0;             // no other senders
    theOnlySender[0].sem_flg = 0;             // so the semaphore
                                              // THE_ONLY_SENDER is
                                              // equal to
                                              // zero (available)

    theOnlySender[1].sem_num = THE_ONLY_SENDER; // to point out that
    theOnlySender[1].sem_op  = 1;             // this sender is
    theOnlySender[1].sem_flg = SEM_UNDO;      // working (the semaphore
                                              // THE_ONLY_SENDER is
                                              // equal to
                                              // one (unavailable))

                                              // SEM_UNDO will 
                                              // automatically return
                                              // it back to zero
                                              // when we are done

///////////////////////////////////////////////resource - shmem (start)
///////////////////////////////////////////////senders with each other

    if ( semop(sem_id, theOnlySender, 2) < 0 )
    {
        perror("the only sender: ");
        return EXIT_FAILURE;
    }

    struct sembuf ready2connect[3]; 

    ready2connect[0].sem_num = SUM_SENDERS;    // all-time
    ready2connect[0].sem_op  = 1;              // connections' counter
    ready2connect[0].sem_flg = 0;              // (senders only)

    ready2connect[1].sem_num = SUM_BOTH;       // the same
    ready2connect[1].sem_op  = 1;              // but receivers are
    ready2connect[1].sem_flg = 0;              // counted too

    ready2connect[2].sem_num = SENDER_CONNECT; // I AM READY
    ready2connect[2].sem_op  = 1;              // TO CONNECT :3
    ready2connect[2].sem_flg = SEM_UNDO;

    semop(sem_id, ready2connect, 3);

///////////////////////////////////////////resource - semaphores (start)
///////////////////////////////////////////sender and receiver

    initSem(sem_id, FULL, 0);            // kinda MUTEX
    initSem(sem_id, EMPTY, 1);           //  <--------

///////////////////////////////////////////resource - semaphores (end)
///////////////////////////////////////////sender and receiver

    char* shm_ptr = shmat (shm_id, NULL, 0);
    if ( (void*)shm_ptr == (void*)(-1) )
    {
        perror("shmat(): ");
        return EXIT_FAILURE;
    }

    if ( ( fd = open(argv[1], O_RDONLY) ) < 0)
    {
        perror("open(): ");
        printf("[%s]\n", argv[1]);
        return EXIT_FAILURE;
    }

    char dataBuf[SHM_SIZE + sizeof(int)];
    int nBytes = POISON;

    struct sembuf waitForReceiver[2];

    waitForReceiver[0].sem_num = RECEIVER_CONNECT;  //
    waitForReceiver[0].sem_op  = -1;                //
    waitForReceiver[0].sem_flg = 0;                 //
                                                    // blocks until the
    waitForReceiver[1].sem_num = RECEIVER_CONNECT;  // partner is ready
    waitForReceiver[1].sem_op  = 1;                 //
    waitForReceiver[1].sem_flg = 0;                 //

    semop(sem_id, waitForReceiver, 2);

    struct sembuf handleOverflow[7];

    handleOverflow[0].sem_num =  SUM_BOTH;    // if SUM_BOTH's value is
    handleOverflow[0].sem_op  = -MAX_CNTR;    // greater than MAX_CNTR,
    handleOverflow[0].sem_flg =  IPC_NOWAIT;  // execute the following
                                              // commands (2 - 6).
    handleOverflow[1].sem_num =  SUM_BOTH;
    handleOverflow[1].sem_op  =  MAX_CNTR;    // otherwise, nothing
    handleOverflow[1].sem_flg =  0;           // happens.

    handleOverflow[2].sem_num =  RECEIVER_CONNECT; // if the partner is
    handleOverflow[2].sem_op  = -1;                // still here,
    handleOverflow[2].sem_flg =  IPC_NOWAIT;       // execute the
                                                   // following commands
    handleOverflow[3].sem_num =  RECEIVER_CONNECT; // (4 - 6).
    handleOverflow[3].sem_op  =  1;                //
    handleOverflow[3].sem_flg =  0;                // otherwise, nothing
                                                   // happens

/* the following commands (4 - 6) set all the sum counters to zero */

    handleOverflow[4].sem_num =  SUM_BOTH;
    handleOverflow[4].sem_op  = -semctl(sem_id, SUM_BOTH, GETVAL);
    handleOverflow[4].sem_flg =  0;

    handleOverflow[5].sem_num =  SUM_RECEIVERS;
    handleOverflow[5].sem_op  = -semctl(sem_id, SUM_RECEIVERS, GETVAL);
    handleOverflow[5].sem_flg =  0;

    handleOverflow[6].sem_num =  SUM_SENDERS;
    handleOverflow[6].sem_op  = -semctl(sem_id, SUM_SENDERS, GETVAL);
    handleOverflow[6].sem_flg =  0;

    semop(sem_id, handleOverflow, 7);

/* ----------------------------CONNECTED----------------------------- */

    struct sembuf commands[6];

    const int SUM_RECEIVERS_val = semctl(sem_id, SUM_RECEIVERS, GETVAL);

    for(;;)
    {

/*------------------------------------------------------*/
/* handles the situation when our current partner dies  */
/*          and the next one replaces it:               */

//init SUM_RECEIVERS_CONST with the correct receiver's number:

		commands[0].sem_num =  SUM_RECEIVERS_CONST;
		commands[0].sem_op  = -semctl(sem_id, \
							          SUM_RECEIVERS_CONST, GETVAL);
		commands[0].sem_flg =  0;

		commands[1].sem_num =  SUM_RECEIVERS_CONST;
		commands[1].sem_op  = +SUM_RECEIVERS_val;
		commands[1].sem_flg =  0;

//abort if it's less than the actual receiver's number:

		commands[2].sem_num =  SUM_RECEIVERS_CONST;
		commands[2].sem_op  = -semctl(sem_id, SUM_RECEIVERS, GETVAL);
		commands[2].sem_flg =  IPC_NOWAIT;

/* otherwise, executes the following commands (3 - 5)   */
/*------------------------------------------------------*/


/*------------------------------------------------------*/
/*   if our partner has disconnected for some reason,   */
/*                      aborts                          */

        commands[3].sem_num = RECEIVER_CONNECT;
        commands[3].sem_op  = -1;
        commands[3].sem_flg = IPC_NOWAIT;

        commands[4].sem_num = RECEIVER_CONNECT;
        commands[4].sem_op  = 1;
        commands[4].sem_flg = 0;

/*    otherwise (if everything's good), blocks until    */
/*    the receiver reads a portion of data from the     */
/*                     shared memory                    */

/*  (or if it is the first iteration, just keeps going) */

        commands[5].sem_num = EMPTY;
        commands[5].sem_op  = -1;
        commands[5].sem_flg = 0;

/*------------------------------------------------------*/

///////////////////////////////////////////////resource - shmem (start)
///////////////////////////////////////////////sender and receiver

        if ( semop(sem_id, commands, 6) < 0 )
        {
            perror("the sender before reading from the file"\
                   " - the receiver has disconnected ");
            return EXIT_FAILURE;
        }

        nBytes = read(fd, dataBuf + sizeof(int), SHM_SIZE);
        if (nBytes < 0)
        {
            perror("read(): ");
            return EXIT_FAILURE;
        }

        *((int*) shm_ptr) = nBytes;
        strncpy(shm_ptr + sizeof(int), dataBuf + sizeof(int), SHM_SIZE);

/*------------------------------------------------------*/
/* handles the situation when our current partner dies  */
/*          and the next one replaces it:               */

//init SUM_RECEIVERS_CONST with the correct receiver's number:

		commands[0].sem_num =  SUM_RECEIVERS_CONST;
		commands[0].sem_op  = -semctl(sem_id, \
							          SUM_RECEIVERS_CONST, GETVAL);
		commands[0].sem_flg =  0;

		commands[1].sem_num =  SUM_RECEIVERS_CONST;
		commands[1].sem_op  = +SUM_RECEIVERS_val;
		commands[1].sem_flg =  0;

//abort if is's less than the actual receiver's number:

		commands[2].sem_num =  SUM_RECEIVERS_CONST;
		commands[2].sem_op  = -semctl(sem_id, SUM_RECEIVERS, GETVAL);
		commands[2].sem_flg =  IPC_NOWAIT;

/* otherwise, executes the following commands (3 - 5)   */
/*------------------------------------------------------*/


/*------------------------------------------------------*/
/*   if our partner has disconnected for some reason,   */
/*                      aborts                          */

        commands[3].sem_num = RECEIVER_CONNECT;
        commands[3].sem_op  = -1;
        commands[3].sem_flg = IPC_NOWAIT;

        commands[4].sem_num = RECEIVER_CONNECT;
        commands[4].sem_op  = 1;
        commands[4].sem_flg = 0;

/*      otherwise (if everything's good), unblocks      */
/*          the receiver, so it can read from           */
/*               the shared memory again                */

        commands[5].sem_num = FULL;
        commands[5].sem_op  = 1;
        commands[5].sem_flg = 0;

/*------------------------------------------------------*/

        if ( semop(sem_id, commands, 6) < 0 )
        {
            perror("the sender after reading from the file"\
                   " - the reader has disconnected ");
            return EXIT_FAILURE;
        }

////////////////////////////////////////////////resource - shmem (end)
////////////////////////////////////////////////sender and receiver

        if (!nBytes) // if the whole file has been sent successfully
            break;   // we are done
    }

    if ( shmdt(shm_ptr) < 0 )
    {
        perror("shmdt(): ");
        return EXIT_FAILURE;
    }

    close(fd);
    
    return EXIT_SUCCESS;
}

/*                  the sender disconnects                  */

///////////////////////////////////////////////resource - shmem (end)
///////////////////////////////////////////////senders with each other

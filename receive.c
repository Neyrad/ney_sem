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
#define N_SEMAPHORES 9

const int POISON = -1;

enum
{
    THE_ONLY_SENDER   = 0,
    THE_ONLY_RECEIVER = 1,
    SENDER_CONNECT    = 2,
    RECEIVER_CONNECT  = 3,
    SUM_RECEIVERS     = 4,
    SUM_SENDERS       = 5,
    SUM_BOTH          = 6,
    EMPTY             = 7,
    FULL              = 8
};

int main(int argc, char* argv[])
{
    if (argc != 1)
    {
        printf("Usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

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

    struct sembuf theOnlyReceiver[2];

    theOnlyReceiver[0].sem_num = THE_ONLY_RECEIVER; // wait until
    theOnlyReceiver[0].sem_op  = 0;           // there's no another
    theOnlyReceiver[0].sem_flg = 0;           // receiver, so
                                              // the semaphore
                                              // THE_ONLY_RECEIVER
                                              // is equal to
                                              // zero (available)

    theOnlyReceiver[1].sem_num = THE_ONLY_RECEIVER; // to point out 
    theOnlyReceiver[1].sem_op  = 1;           // that this receiver is
    theOnlyReceiver[1].sem_flg = SEM_UNDO;    // working (the semaphore
                                              // THE_ONLY_RECEIVER is
                                              // equal to
                                              // one (unavailable))

                                              // SEM_UMDO will
                                              // automatically return
                                              // it back to zero
                                              // when we are done

/////////////////////////////////////////resource - shmem#1 start

    if ( semop(sem_id, theOnlyReceiver, 2) < 0 )
    {
        perror("the only receiver: ");
        return EXIT_FAILURE;
    }

    struct sembuf ready2connect[3]; 

    ready2connect[0].sem_num = SUM_RECEIVERS;    // all-time
    ready2connect[0].sem_op  = 1;                // connections' counter
    ready2connect[0].sem_flg = 0;                // (receivers only)

    ready2connect[1].sem_num = SUM_BOTH;         // the same
    ready2connect[1].sem_op  = 1;                // but senders are
    ready2connect[1].sem_flg = 0;                // counted too

    ready2connect[2].sem_num = RECEIVER_CONNECT; // I AM READY
    ready2connect[2].sem_op  = 1;                // TO CONNECT :3
    ready2connect[2].sem_flg = SEM_UNDO;
    
    semop(sem_id, ready2connect, 3);
    
    char* shm_ptr = shmat(shm_id, NULL, 0);
    if ( (void*)shm_ptr == (void*)(-1) )
    {
        perror("shmat(): ");
        return EXIT_FAILURE;
    }

    char data_buf[SHM_SIZE];
    int nBytes = POISON;

    struct sembuf waitForSender[2];

    waitForSender[0].sem_num = SENDER_CONNECT;  //
    waitForSender[0].sem_op = -1;               //
    waitForSender[0].sem_flg = 0;               //
                                                // blocks until the
    waitForSender[1].sem_num = SENDER_CONNECT;  // partner is ready
    waitForSender[1].sem_op = 1;                //
    waitForSender[1].sem_flg = 0;               //
    
    semop(sem_id, waitForSender, 2);

/* -----------------------------CONNECTED---------------------------- */

    struct sembuf commands[5]; 

    int SUM_SENDERS_value = semctl(sem_id, SUM_SENDERS, GETVAL);

    for(;;) 
    {

/*------------------------------------------------------*/
/* handles the situation when our current partner dies  */
/*          and the next one replaces it:               */
/*   aborts if SUM_SENDERS' value > (SUM_BOTH's / 2)    */

        commands[0].sem_num =  SUM_BOTH;
        commands[0].sem_op  = -SUM_SENDERS_value * 2;
        commands[0].sem_flg =  IPC_NOWAIT;

        commands[1].sem_num =  SUM_BOTH;
        commands[1].sem_op  =  SUM_SENDERS_value * 2;
        commands[1].sem_flg =  0;

/* otherwise, executes the following commands (2 - 4)   */
/*------------------------------------------------------*/


/*------------------------------------------------------*/
/*   if our partner has disconnected for some reasons,  */
/*                      aborts                          */

        commands[2].sem_num = SENDER_CONNECT;
        commands[2].sem_op  = -1;
        commands[2].sem_flg = IPC_NOWAIT;

        commands[3].sem_num = SENDER_CONNECT;
        commands[3].sem_op  = 1;
        commands[3].sem_flg = 0;

/*    otherwise (if everything's good), blocks until    */
/*    the sender writes a new portion of data to the    */
/*                    shared memory                     */

        commands[4].sem_num = FULL;
        commands[4].sem_op  = -1;
        commands[4].sem_flg = 0;

/*------------------------------------------------------*/

///////////////////////////////////////////////resource - shmem#2 start     
        if ( semop(sem_id, commands, 5) < 0 )
        {
            nBytes = *((int*) shm_ptr);

            if (nBytes) // if the sender has disconnected but we haven't
                        // received the whole file yet
            {
                perror("the receiver before reading from"\
                       " the shared memory"\
                       " - the writer has disconnected ");
                return EXIT_FAILURE;
            }
        }
        
        nBytes = *((int*) shm_ptr);
        shm_ptr += sizeof(int); 

        strncpy(data_buf, shm_ptr, nBytes);

        if (nBytes > 0)
            printf("%s", shm_ptr);  //correct printing

        shm_ptr -= sizeof(int);

/*------------------------------------------------------*/
/* handles the situation when our current partner dies  */
/*          and the next one replaces it:               */
/*   aborts if SUM_SENDERS' value > (SUM_BOTH's / 2)    */

        commands[0].sem_num =  SUM_BOTH;
        commands[0].sem_op  = -SUM_SENDERS_value * 2;
        commands[0].sem_flg =  IPC_NOWAIT;

        commands[1].sem_num =  SUM_BOTH;
        commands[1].sem_op  =  SUM_SENDERS_value * 2;
        commands[1].sem_flg =  0;

/* otherwise, executes the following commands (2 - 4)   */
/*------------------------------------------------------*/


/*------------------------------------------------------*/
/*   if our partner has disconnected for some reasons,  */
/*                      aborts                          */

        commands[2].sem_num = SENDER_CONNECT;
        commands[2].sem_op  = -1;
        commands[2].sem_flg = IPC_NOWAIT;

        commands[3].sem_num = SENDER_CONNECT;
        commands[3].sem_op  = 1;
        commands[3].sem_flg = 0;

/*      otherwise (if everything's good), unblocks      */
/*          the sender, so it can write to the          */
/*                  shared memory again                 */

        commands[4].sem_num = EMPTY;
        commands[4].sem_op  = 1;
        commands[4].sem_flg = 0;

/*------------------------------------------------------*/

        if ( semop(sem_id, commands, 5) < 0 && nBytes)
          // if the sender has disconnected but we haven't
          // received the whole file yet
        {
            printf("nBytes is %d\n", nBytes);
            perror("the receiver after reading from the shared memory"\
                   " - the sender has disconnected ");
            return EXIT_FAILURE;
        }

////////////////////////////////////////////////resource - shmem#2 end

        if (!nBytes) // if the whole file has been received successfully
            break;   // we are done
    }

    if ( shmdt(shm_ptr) < 0 )
    {
        perror("shmdt(): ");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*                  the receiver disconnects                    */

/////////////////////////////////////////////resource - shmem#1 end
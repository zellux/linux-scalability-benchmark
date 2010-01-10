#include <sys/types.h>
#include <unistd.h>   
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

static int
ping(int semid)
{
    struct sembuf ops[2];
    int ret;
    
    ops[0].sem_num = 0;
    ops[0].sem_op = -1;
    ops[0].sem_flg = 0;
    ops[1].sem_num = 1;
    ops[1].sem_op = 1;
    ops[1].sem_flg = 0;
    ret = semop(semid, ops, 2);

    if (ret) {
        perror("ping");
        exit(-1);
    }
    return 0;
}

static int
pong(int semid)
{
    struct sembuf ops[2];
    int ret;
    
    ops[0].sem_num = 0;
    ops[0].sem_op = 1;
    ops[0].sem_flg = 0;
    ops[1].sem_num = 1;
    ops[1].sem_op = -1;
    ops[1].sem_flg = 0;
    ret = semop(semid, ops, 2);

    if (ret) {
        perror("pong");
        exit(-1);
    }
    return 0;
}

int 
main(int argc, char *argv[]) 
{
    int semid;
    key_t semkey;
    pid_t pid;
    int i;

    if (argc > 1) {
        semkey = atoi(argv[1]);
        semid = semget(semkey, 2, 0666);
    } else {
        semkey = 1001;
        if ((semid = shmget(semkey, 2, 0666 | IPC_CREAT)) < 0)
            exit(-1);
        /* Init semephore array with {1, 0}, so the ping process will run first */
        semctl(semid, 0, SETVAL, 1);
        semctl(semid, 1, SETVAL, 0);
    }

    printf("pingpong begin with semkey=%d, semid=%d\n", semkey, semid);
    pid = fork();
    if (pid == 0) {
        for (i = 0; i < 10; i++) {
            printf("ping\n");
            ping(semid);
        }
    } else {
        for (i = 0; i < 10; i++) {
            printf("pong\n");
            pong(semid);
        }
    }

    return 0;
}

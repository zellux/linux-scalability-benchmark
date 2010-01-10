#define _GNU_SOURCE

#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <netdb.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sched.h>

#include "bench.h"

int 
main(int argc, char *argv[]) {
    int nchilds = 1;
    int i;
    int semid[32], semkey[32];
    key_t key;
    uint64_t start, end, usec;
    pid_t pid[32];
    char buf[1024];

    if (argv[1] != NULL) {
        nchilds = atoi(argv[1]);
    }

    key = 1000;
    for (i = 0; i < nchilds; i++) {
        printf("Getting semaphore #%d...", i);
        while ((semid[i] = shmget(key, 2, 0666 | IPC_CREAT)) < 0)
            key ++;
        printf("<%d>\n", key);
        semkey[i] = key;
        /* Init semephore array with {1, 0}, so the ping process will run first */
        semctl(semid[i], 0, SETVAL, 1);
        semctl(semid[i], 1, SETVAL, 0);
    }

    printf("Running ping pong programs...\n");
    start = read_tsc();
    for (i = 0; i < nchilds; i++) {
        pid[i] = fork();
        if (pid[i] == 0) {
            sprintf(buf, "%d", semkey[i]);
            execlp("./pingpong", "./pingpong", buf, (char *) NULL);
            perror("execlp");
            exit(-1);
        }
    }

    for (i = 0; i < nchilds; i++) {
        if (waitpid(pid[i], (int *) NULL, 0) < 0)
            perror("waitpid");
    }
    
    end = read_tsc();
    usec = (end - start) * 1000000 / get_cpu_freq();
    printf("usec: %ld\t\n", usec);

    return 0;
}

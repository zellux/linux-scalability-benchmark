#define _GNU_SOURCE

#include <sys/types.h>
#include <unistd.h>   
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <stdint.h>
#include <sched.h>

#include "config.h"
#include "bench.h"

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
    int semid[MAX_CORES];
    key_t semkey[MAX_CORES], key;
    pid_t pid[MAX_CORES * 2];
    int i, id;
    uint64_t start, end, usec;
    int ncores;

    if (argc > 1) {
        ncores = atoi(argv[1]);
    } else {
        printf("Usage: ./pingpong <number of cores>\n");
        exit(-1);
    }

    /* Init semephores */
    key = 1000;
    for (i = 0; i < ncores; i++) {
        do {
            key ++;
        } while ((semid[i] = semget(key, 2, 0666 | IPC_CREAT)) < 0);
        /* Init semephore array with {1, 0}, so the ping process will run first */
        semkey[i] = key;
        semctl(semid[i], 0, SETVAL, 1);
        semctl(semid[i], 1, SETVAL, 0);
    }
    
    start = read_tsc();
    for (id = 0; id < ncores; id++) {
        if ((pid[id * 2] = fork()) == 0) {
            affinity_set(id);
            /* printf("ping begin with semkey=%d, semid=%d on core#%d\n", semkey[id], semid[id], id); */
            for (i = 0; i < NR_PINGPONGS; i++) {
                ping(semid[id]);
            }
            exit(0);
        } else {
            if ((pid[id * 2 + 1] = fork()) == 0) {
                affinity_set(id);
                /* printf("pong begin with semkey=%d, semid=%d on core#%d\n", semkey[id], semid[id], id); */
                for (i = 0; i < NR_PINGPONGS; i++) {
                    pong(semid[id]);
                }
                exit(0);
            }
        }
    }

    for (i = 0; i < ncores * 2; i++) {
        waitpid(pid[i], NULL, 0);
    }

    end = read_tsc();
    usec = (end - start) * 1000000 / get_cpu_freq();
    printf("usec: %ld\t\n", usec);

    return 0;
}

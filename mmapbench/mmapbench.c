/**
 * @file   mmap.c
 * @author Wang Yuanxuan <zellux@gmail.com>
 * @date   Fri Jan  8 21:23:31 2010
 * 
 * @brief  An implementation of mmap bench mentioned in OSMark paper
 * 
 * 
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

#include "config.h"
#include "bench.h"

/* int nbufs = 128000; */
int nbufs = 64000;
char *shared_area = NULL;
int flag[32];
int ncores = 4;
char *filename = "share.dat";

void *
worker(void *args)
{
    int id = (long) args;
    int ret = 0;
    int i;

    affinity_set(id);

    for (i = 0; i < nbufs; i++)
        ret += shared_area[i *4096];
    
    //printf("potato_test: thread#%d done.\n", core);

    return (void *) (long) ret;
}

int
main(int argc, char **argv)
{
    int i, fd;
    pthread_t tid[32];
    uint64_t start, end, usec;

    for (i = 0; i < ncores; i++) {
        flag[i] = 0;
    }

    if (argc > 1) {
        ncores = atoi(argv[1]);
    }

    fd = open(filename, O_RDONLY);
    shared_area = mmap(0, (1 + nbufs) * 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    
    start = read_tsc();
    for (i = 0; i < ncores; i++) {
        pthread_create(&tid[i], NULL, worker, (void *) (long) i);
    }

    for (i = 0; i < ncores; i++) {
        pthread_join(tid[i], NULL);
    }
    
    end = read_tsc();
    usec = (end - start) * 1000000 / get_cpu_freq();
    printf("usec: %ld\t\n", usec);

    close(fd);
    return 0;
}

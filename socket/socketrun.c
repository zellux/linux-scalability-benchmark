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

#define CONNECTS 10000
#define MAX_LINE 1024
#define LISTENQ 1024

const char *serverip[] = {"10.131.1.133"};
int nservers = sizeof(serverip) / sizeof(char *);
int port[sizeof(serverip) / sizeof(char *)];

void *
server_work(void *args)
{
    int id = (long) args;
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    unsigned short p;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_addr.s_addr = inet_addr(serverip[id]);
    /* servaddr.sin_addr.s_addr = htonl(inet_addr(serverip[id])); */
    servaddr.sin_port        = 0;
    /* servaddr.sin_port        = htons(port); */

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }
    
    for (p = 12345; p < 13000; p ++) {
        servaddr.sin_port = htons(p);
        if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
            continue;
        }
        port[id] = p;
        assert(p == ntohs(servaddr.sin_port));
        break;
    }
    if (port[id] < 0) {
        fprintf(stderr, "Error calling bind()\n");
        exit(EXIT_FAILURE);
    }

    printf("Server #%d port:%d\n", id, p);

    if (listen(listenfd, LISTENQ) < 0 ) {
        fprintf(stderr, "Error calling listen()\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if ( (connfd = accept(listenfd, NULL, NULL) ) < 0 ) {
            fprintf(stderr, "Error calling accept()\n");
            exit(EXIT_FAILURE);
        }

        if (close(connfd) < 0) {
            fprintf(stderr, "Error calling close()\n");
            exit(EXIT_FAILURE);
        }
    }

    return (void *) 0;
}

void *
client_work(void *args)
{
    int id = (long) args;
    int clientfd;
    struct in_addr ip;
    struct sockaddr_in serveraddr;
    int i;

    if (!inet_pton(AF_INET, serverip[id], &ip)) {
        perror("inet_aton");
        exit(EXIT_FAILURE);
    }
    
    printf("Client #%d, connecting to %s:%d\n", id, serverip[id], port[id]);
    
    /* if ((hp = gethostbyaddr((const void *) &ip, sizeof(ip), AF_INET)) == NULL) { */
    /*     perror("gethostbyaddr"); */
    /*     exit(EXIT_FAILURE); */
    /* } */

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(serverip[id]);
    /* bcopy((char *) hp->h_addr, (char *) &serveraddr.sin_addr.s_addr, hp->h_length); */
    serveraddr.sin_port = htons(port[id]);
    
    for (i = 0; i < CONNECTS; i++) {
        if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        if (connect(clientfd, (const struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
            printf("Time #%d, fd=%d", i, clientfd);
            perror("connect");
            exit(EXIT_FAILURE);
        }
        close(clientfd);
    }
    
    return (void *) 0;
}

int 
main(int argc, char *argv[]) {
    int i;
    pthread_t tid[nservers];
    int flag;

    printf("Starting servers...\n");
    for (i = 0; i < nservers; i++) {
        port[i] = -1;
        pthread_create(&tid[i], NULL, server_work, (void *) (long) i);
    }

    while (1) {
        flag = 1;
        for (i = 0; i < nservers; i++) {
            flag &= (port[i] != -1);
        }
        if (flag) break;
    }

    printf("Starting clients...\n");
    printf("[Timing begins]\n");
    for (i = 0; i < nservers; i++) {
        pthread_create(&tid[i], NULL, client_work, (void *) (long) i);
    }
    
    for (i = 0; i < nservers; i++) {
        pthread_join(tid[i], NULL);
    }
    printf("[Timing ends]\n");
    
    
    return 0;
}

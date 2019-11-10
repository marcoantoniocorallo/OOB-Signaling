#ifndef WORKERMASK_H
#define WORKERMASK_H

#include <util.h>
#include <pthread.h>

//Ogni thread che si occupa di un client memorizza in questa struttura:
typedef struct workerMask{
    int         fd;                //FD con un client
    pthread_t   tid;               //ID del thread che se ne occupa
    int         end;               //Flag di chiusura FD
    int         attivo;            //Ancora in uso
} workerMask_t;

#define INIZIALIZZA(m)                  \
        m.fd=-1;                        \
        m.tid=-1;                       \
        m.end=0;                        \
        m.attivo=0;

void inizializza(workerMask_t m[], int size){
    int i=0;
    for (int i = 0; i < size; i++)
        INIZIALIZZA(m[i])
}

int getIndex(workerMask_t m[],int fd){
    for (int i = 0; i < MAXCONNECTIONS; i++)
        if (m[i].fd == fd) return i;
    return -1;
}

#endif //WORKERMASK_H

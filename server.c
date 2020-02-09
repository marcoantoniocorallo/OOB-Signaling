#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <sys/select.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#include <util.h>
#include <workerMask_t.h>
#include <netinet/in.h>

void* worker(void* fd);
void sigPipeHandler();
int MCDCalc(int* A, int size);

//Ci saranno al più tanti threads quanto il numero massimo di clients contemporanei
//Probabilmente meno, per limite di risorse
workerMask_t mask[MAXCONNECTIONS];

volatile sig_atomic_t terminate=0;          //Variabile di condizione del loop
int snum;                                   //Numero identificativo del server
char name[15];                              //Indirizzo per la socket

int main(int argc, char* argv[]) {
    snum = strtol(argv[1], NULL, 0);        //Numero identificativo del server.
    int pfd = strtol(argv[2], NULL, 0);     //file descriptor della pipe con il supervisor.
    int err = -1;
    pthread_t tid = -1;
    char *msg;                              //Comunicazione per il supervisor
    int  index=0;                           //Indice array di workers

    long fd_socket = -1, fd_c = -1;
    int *args = malloc(sizeof(int) * 2);    //Parametri da passare al thread worker
    struct sockaddr_un sa;
    fd_set set, rdset;
    FD_ZERO(&set);
    FD_ZERO(&rdset);

    //Gestione segnali
    struct sigaction intHandler, pipeHandler;
    memset(&intHandler, 0, sizeof(intHandler));
    memset(&pipeHandler,0, sizeof(pipeHandler));
    intHandler.sa_handler = SIG_IGN;
    pipeHandler.sa_handler = sigPipeHandler;
    sigaction(SIGINT, &intHandler, NULL);
    sigaction(SIGPIPE,&pipeHandler,NULL);

    inizializza(mask,MAXCONNECTIONS);

    //Chiudo estremità opposta della pipe
    if (close(pfd-1) == -1)
        perror("Chiusura fd lato server fallita.");
    printf("SERVER %d ACTIVE\n", snum);

    //Apre socket
    SYS((fd_socket=socket(AF_UNIX,SOCK_STREAM,0)),-1,"Connection lost.")

    //Associa indirizzo alla socket
    sa.sun_family=AF_UNIX;
    sprintf(name, "OOB-server-%d", snum);
    strncpy(sa.sun_path,name, sizeof(name));
    unlink(name);   //Se ci sono file socket aperti li elimino
    SYS((bind(fd_socket, (struct sockaddr*) &sa, sizeof(sa))), -1, \
        "Associazione indirizzo socket fallita")

    //Segnaliamo che siamo disposti ad ascoltare...
    SYS(listen(fd_socket,MAXCONNECTIONS-1),-1,"listen failed")
    FD_SET(fd_socket, &set);

    //Riveste il ruolo di dispatcher
    while (!terminate) {
        struct timeval timeout={0,150}; //Timer per la select
        rdset=set;

        while((select(fd_socket+1,&rdset,NULL,NULL,&timeout))==-1 && errno==EINTR);

        if (FD_ISSET(fd_socket,&rdset)){
            SYS((fd_c = accept(fd_socket, NULL, 0)), -1, "Error on accept connection")

            //Registra FD nella maschera
            while (mask[index].attivo==1)   index++;
            mask[index].fd=fd_c;
            mask[index].end=0;

            //Avvia un thread passandogli il FD di interesse e quello per la socket
            if ((err = pthread_create(&tid, NULL, &worker, (void*)fd_c)) != 0) {
                fprintf(stderr, "Worker failure.\n");
                exit(EXIT_FAILURE);
            }

            //Registra il thread ID di chi se ne occupa nella maschera e segna come attivo
            mask[index].tid=tid;
            mask[index].attivo=1;

        }

        //Controlla che ci siano threads pronti alla join
        for (int i = 0; i < MAXCONNECTIONS; i++) {
            if (mask[i].end == 1) {

                pthread_join(mask[i].tid, (void **) &msg);
                if (strtol(msg,NULL,0)!=-1) {

                    //Inoltra messaggio al supervisor
                    write(pfd, (void *) msg, MAXTOWRITE);

                    // chiudi FD e libera memoria
                    close(mask[i].fd);
					if (msg)
	                    free(msg);
                }
                INIZIALIZZA(mask[i])
            }
        }
    }
	if (args)
	    free(args);
    close(pfd);
    unlink(name);
    return 0;
}

/**
 * Funzione di elaborazione di ogni singolo thread:
 * Si occupa di gestire il client che manda messaggi sulla socket a lui affidata.
 * Calcola una stima del tempo di attesa del suo client e la restituisce al chiamante.
 * @param fd : file descriptor della socket su cui comunica il client
 * @return   : Stringa "ID,stima"
 */
void* worker(void* fd)
{
    char tmp[MAXTOWRITE];
    int retread=-1; //Valore restituito dalla read
    struct timeval currenttime;
    struct timeval *times=NULL;
    struct timeval diff;
    long long int id=-1;
    int i=0;
    int stima=0;
    int *stime=NULL;
    char *msg=malloc(sizeof(char)*MAXTOWRITE);
    int index=getIndex(mask,(long)fd);

    SYS(index,-1,"Indice della maschera non trovato")

    printf("SERVER %d CONNECT FROM CLIENT\n",snum);
    fflush(stdout);

    //Attende messaggio dal client
    retread = read((long)fd, &tmp, sizeof(tmp));
    do{
        //Cattura il tempo attuale.
        //Operazione con massima priorità per diminuire la probabilità di errore
        gettimeofday(&currenttime, NULL);

        //Gestisce eventuali errori
        SYS(retread, -1, "Error in read")

        //Salva tempo arrivo messaggio in un array di tempi
        times = realloc(times, sizeof(struct timeval)*(i+1));
        times[i++] = currenttime;

        id = ntohl(strtol(tmp, NULL, 0));
        printf("SERVER %d INCOMING FROM %x @ %ld.%ld\n",\
        snum, (int)id, currenttime.tv_sec%60, currenttime.tv_usec/1000);
        fflush(stdout);
    }   while ((retread = read((long)fd, &tmp, sizeof(tmp))));

    //Se è arrivato un solo messaggio, restituisci -1
    if (i==1) {
        printf("SERVER %d CLOSING %lld ESTIMATES %d\n",snum,id,-1);fflush(stdout);
        sprintf(msg,"%d",-1);
        mask[index].end=1;
        pthread_exit(msg);
    }
    else {

        //Calcola differenze tra coppie di tempi
        int j;
        stime=malloc(sizeof(int)*i);
        for (j = 0; j < i - 1; j++) {
            timeval_subtract(&diff,&times[j+1], &times[j]);

            //Scrive la differenza tra i due tempi - espressa come <sec,u-sec> - in millisecondi
            stima = (diff.tv_sec*1000)+(diff.tv_usec/1000);
            stime[j] = stima;
        }   if (times)
            free(times);
	
	//Calcola MCD
	stima = MCDCalc(stime,j);

	if (stime)
            free(stime);

        //Esci restituendo la coppia <ID,Stima>
        memset(msg,'\0', MAXTOWRITE);
		if (stima == -1)
			sprintf(msg,"%d",-1);
		else    
		    sprintf(msg, "%lld,%d", id, stima);
        printf("SERVER %d CLOSING %x ESTIMATES %d\n", snum, (int)id, stima);
        fflush(stdout);
        mask[index].end = 1;
        pthread_exit(msg);
    }
}

void sigPipeHandler()
{
    terminate=1;
}

int MCDCalc(int* A, int size){

//	#---VECCHIO CALCOLO MCD---#
/*
        //Considera ogni differenza di orari, facendo il MCD a due a due
		int stima=0;
        for (int k = 0; k < size; k++)
            stima = approximate(A[k], stima);
		return stima;
*/

	//#---NUOVO CALCOLO MCD---#

	//Se c'è un solo elemento, restituiscilo direttamente
	//if(size==1) return A[0];

	//Calcola l'MCD di tutte le possibili coppie di stime

		//Dimension = C(n,2) = #MCDS
		int num = factorial(size);
		int den = 2*factorial(size-2);
		int	dimension = num/den;
		
		//Calcolo tutti i possibili MCD
		int mcds[dimension];
        for (int k = 0; k < size-1; k++) {
            for (int l = k+1; l < size; l++) {
                mcds[EL(k,size,l)]=approximate(A[k],A[l]);
            }
        }
		
		//Seleziona l'MCD più ricorrente
		int migliorStima = mostFrequent(mcds, dimension);
		int result = mcds[migliorStima];

		return result;
}

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <util.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define USAGE\
        "usage: %s howmanyservers totalservers howmanymesages\n"\
        "1 <= #servers < #totalservers\n"\
        "#messages > 3(#servers)\n"

struct timespec initializeSecret(int secret);

int main(int argc, char* argv[]){
    int p, k, w; p=k=w=-1;
    long long int  id;
    int secret;
    struct timeval timek;
    gettimeofday(&timek, NULL);
    int* servers=NULL;
    int* choices=NULL;

    struct sockaddr_un sa;
    sa.sun_family=AF_UNIX;
    int *sock=NULL;

    char msg[8]={0};                //Messaggio contenente l'id in NBO

    struct timespec timetowait;

    //Controllo parametri
    if (argc!=4){
        fprintf(stderr,\
        USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }
    else {
        p=strtol(argv[1],NULL,10);
        k=strtol(argv[2],NULL,10);
        w=strtol(argv[3],NULL,10);
        if (p < 1  ||
            p >= k ||
            w <= 3*p)
        {
            fprintf(stderr, USAGE, argv[0]);
            exit(EXIT_FAILURE);
        }
        else {
            char name1[15], name2[15];
            sprintf(name1,"OOB-server-%d",k-1);
            sprintf(name2,"OOB-server-%d",k);

            //Se non trova la socket k-esima, o se trova la k+1-esima, allora K è errato!
            if ((access(name1, F_OK) == -1) || (access(name2, F_OK)!=-1)){
                fprintf(stderr,USAGE,argv[0]);
                exit(EXIT_FAILURE);
            }
        }
    }

    //Generazione ID e #Secret
    unsigned long seed = mix(clock(), time(NULL), getpid());
    srand(seed);
    id=rand();
    secret = rand()%3000;

    printf("CLIENT %x SECRET %d\n",(int)id,secret);

    //Sceglie casualmente i p server e vi si collega
    timetowait=initializeSecret(secret);
    sock=malloc(sizeof(int)*p);
    servers=malloc(sizeof(int)*p);
    for (int i = 0; i < p; i++) {
        int t=-1;
        do {
            t = rand() % k;
        }
        while (member(servers,t,p));
        servers[i]=t;

        char name[15];
        sprintf(name,"OOB-server-%d",servers[i]);
        strncpy(sa.sun_path,name,15);
        sock[i]=socket(AF_UNIX,SOCK_STREAM,0);
        connect(sock[i],(const struct sockaddr *)&sa,15);
    }

    //Sceglie un server tra i p pre-selezionati
    //Nota: viene fatto previo ingresso nel ciclo per rendere l'attesa più accurata
    choices=malloc(sizeof(int)*w);
    for (int j = 0; j < w; j++) {
        int t = rand()%p;
        choices[j]=t;
    }

    sprintf(msg,"%ud",htonl(id));       //Messaggio da inviare
    for (int l = 0; l < w; l++) {

        //Invia ID in NBO
        write(sock[choices[l]],msg,15);

        //Attende secret ms
        nanosleep(&timetowait, (struct timespec*) NULL);
    }

    for (int i=0; i<p; i++)
        close(sock[i]);
    free(sock);
    free(servers);
    free(choices);

    printf("CLIENT %x DONE\n", (int)id);

    return 0;

}

//Converte secret millisecondi in una struttura timespec,
// formato richiesto dalla nanosleep
struct timespec initializeSecret(int secret)
{
    struct timespec res = {0};
    res.tv_sec = (int)(secret/1000);
    res.tv_nsec = (secret % 1000) * 1e6; //1 ns = 1e6 millisecondi
    return res;
}

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <util.h>
#include <icl_hash.h>
#include <estimate_t.h>
#include <fcntl.h>

int checkargs(int argc, char* argv[]);
void* scompatta(long long int*, int*, char*);
void printTable(int fd, icl_hash_t* ht);
void sigIntHandler();
void sigAlarmHandler();

//Variabili globali
icl_hash_t *htable=NULL;            //Tabella hash
volatile sig_atomic_t terminate=0;  //Condizione di uscita dal ciclo Select
int *pid=NULL;                      //Array di PID dei servers attivati

int main(int argc, char* argv[]){
    int k=0;                        //Numero di servers totali
    int retread=0;
    char tmp[MAXTOWRITE];           //buffer temporaneo in cui salvo la stima letta dalla pipe
    fd_set set;     FD_ZERO(&set);  //Insieme di file descriptor attivi
    fd_set rdset;   FD_ZERO(&rdset);//Copia di set da passare alla select
    int fd_max=-1;                  //Numero di fd attivi

    struct sigaction sInt, sAlarm;
    memset(&sInt, 0, sizeof(sInt));    //Mi accerto che s sia stato inizializzato a 0
    memset(&sAlarm, 0, sizeof(sAlarm));
    sInt.sa_handler=sigIntHandler;    //Registro gestore SIGINT
    sAlarm.sa_handler=sigAlarmHandler;//Registro gestore SIGALRM

    //maschera segnali fino all'installazione del gestore
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGALRM);

    //Controllo argomenti
    k=checkargs(argc,argv);
    printf("SUPERVISOR STARTING %d\n",k);

    //Alloco l'hash table
    if ((htable=icl_hash_create(NBUCKETS,NULL,compareInt))==NULL){
        fprintf(stderr, "Allocating hash table");
        exit(EXIT_FAILURE);
    }

    //Array di pipe: alla posizione i trovo la pipe con il server i.
    //pipes[i][1] può essere usato per scrivere sulla pipe
    //pipes[i][0] può essere usato per leggere dalla pipe
    int **pipes=NULL;
    ALLOCA(pipes, sizeof(int*)*k)

    //Array contenente i pid dei servers
    pid=malloc(sizeof(int)*k);

    //Installo gestore segnali
    SYS(sigaction(SIGINT, &sInt, NULL), -1, "Signal handler doesn't works. Exiting...")
    SYS(sigaction(SIGALRM,&sAlarm, NULL),-1,"Siglan handler doesn't works. Exiting...")
    sigemptyset(&sigset);

    //Avvia K servers distinti, aprendo una pipe con ogni server.
    char* s1 = NULL; ALLOCA(s1, sizeof(int))
    char* s2 = NULL; ALLOCA(s2, sizeof(int))
    for (int i = 0; i < k; i++) {
        ALLOCA(pipes[i], sizeof(int)*2)
        SYS((pipe(pipes[i])),-1,"opening pipes")

        //Se una fork dovesse fallire, terminiamo bruscamente
        if ((pid[i]=fork())==-1){
            perror("in fork: ");
            exit(EXIT_FAILURE);
        }
        else if (pid[i]==0) {       //figlio: esegue codice server
            sprintf(s1,"%d",i);
            sprintf(s2,"%d", pipes[i][1]);
            execl("server", "server", s1, s2, NULL);
            perror("exec returned");
            exit(EXIT_FAILURE);
        }
        close(pipes[i][1]);
        FD_SET(pipes[i][0],&set);   //Registra i fd delle pipe nella maschera per la Select

        if (pipes[i][0] > fd_max)   fd_max = pipes[i][0];

        //Settiamo il FD per non bloccarsi alla read
        fcntl(pipes[i][0], F_SETFL,O_NONBLOCK);
    }
    rdset=set;
    free(s1);
    free(s2);

    //Attende per una pipe pronta. Esce dal while solo con un doppio SIGINT
    while (!terminate){
        struct timeval timeout={0,150}; //Timer per la select

        //Non effettuiamo il controllo sul valore di ritorno della select
        //Per non disturbare il gestore di segnali installato.
        errno=0;
        while ((select(fd_max+1,&rdset,NULL,NULL,&timeout)==-1) && errno==EINTR);
        rdset=set;  //Necessario! la select modifica per indicare chi è pronto

        //Scandisce i FD per gestire quello pronto
        for (int i = 0; i < k; i++) {
            if (FD_ISSET(pipes[i][0],&rdset)) {

                //Si rimette in lettura se viene interrotto da un segnale
                errno=0;
                while (((retread=read(pipes[i][0], tmp, MAXTOWRITE))==-1) && (errno==EINTR));

                //Gestisce errori read
                //Nota: EAGAIN vuol dire che la read non è ancora pronta
                //Avendo impostato il FD come O_NON_BLOCK è necessario il controllo
                if (retread==-1) {
                    if (errno == EAGAIN) continue;
                    else {perror("failure on read"); exit(errno);}
                }

                //EOF arriva quando il server ha chiuso il descrittore
                //          Questo non deve avvenire!
                SYS(retread,0,"Un server ha chiuso la connessione!")

                //Per come funziona il server, manderà: "ID,STIMASECRET"
                //Splitta in due variabili separate
                long long int   *id=NULL;   ALLOCA(id, sizeof(long long int))
                int             *stima=NULL; ALLOCA(stima, sizeof(int))
                if (scompatta(id,stima,tmp)==NULL)
                    fprintf(stderr, "Message from server unreadable: %s\n", tmp);
                printf("SUPERVISOR ESTIMATE %d FOR %x FROM %d\n", \
                        *stima, (int)*id, i);
                fflush(stdout);

                //Controlla se esistono già stime per il client ID
                //Se non trova chiave -> inserisce nell'hash
                estimate_t *t = NULL;
                if ((t=icl_hash_find(htable, (void*) id))==NULL) {
                    estimate_t* e=NULL; ALLOCA_ELEMENTO(e)
                                        *(e->id)      =*id;    free(id);
                    *(e->estimate)=*stima; free(stima);
                    *(e->nservers)=1;
                    if ((icl_hash_insert(htable, (void *)(e->id), (void *)e)) == NULL)
                        fprintf(stderr, "Inserimento: una stima di %lld non è stata inserita.\n" \
                                   "La stima potrebbe non essere corretta. \n", *(e->id));
                }

                //Altrimenti, elimina vecchi dati ID e
                // inserisci una stima tra i dati vecchi e quelli nuovi
                else{
                    estimate_t* e=NULL; ALLOCA_ELEMENTO(e)
                    *(e->id)=*id;                                      free(id);
                    *(e->estimate)=approximate(*stima, *(t->estimate));free(stima);
                    *(e->nservers)=*(t->nservers)+1;

                    if (icl_hash_delete(htable,(void*)(e->id),free,freeElement)==-1)
                        fprintf(stderr, "Aggiornamento: una stima di %lld non è stata inserita.\n" \
                                   "La stima potrebbe non essere corretta. \n", *(e->id));

                    if (icl_hash_insert(htable,(void*)(e->id),(void*)e) == NULL)
                        fprintf(stderr, "Aggiornamento: una stima di %lld non è stata inserita.\n" \
                                       "La stima potrebbe non essere corretta. \n", *(e->id));
                }//End inserimento

                memset(tmp,'\0', sizeof(tmp));
            }   //End-FD_ISSET
        }   //END-FOR

    }

    //SIGALRM arrivato: stampa tabella
    printTable(1,htable);
    printf("SUPERVISOR EXITING\n");

    //Chiudi pipes e libera memoria.
    icl_hash_destroy(htable, free, freeElement);
    for (int j = 0; j < k; j++) {
        kill(pid[j],SIGPIPE);
        close(pipes[j][0]);
        free(pipes[j]);
    }
    free(pid);

    if (pipes)
        free(pipes);

    return 0;
}

/**
 * Effettua controllo iniziale sugli argomenti del main
 * @param argc numero di elementi in argv[]
 * @param argv argomenti del programma
 * @pre : argc>0,
 *        argv[]!=NULL && for each s in argv.(s!=NULL)
 * @return k: #server da creare
 */
int checkargs(int argc, char* argv[])
{
    int k=0;

    //Controlli sulle requires
    if (argc<=0 || argv==NULL){
        fprintf(stderr, "Requires violate in checkargs\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < argc; i++) {
        if (argv[i]==NULL){
            fprintf(stderr, "Requires violate in checkargs\n");
            exit(EXIT_FAILURE);
        }
    }

    //Se non viene passato k, si richiede in input.
    if (argc!=2) {
        char* tmp = NULL;
        ALLOCA(tmp, (sizeof(int)))
        fgets(tmp, sizeof(int),stdin);
        if ((k=strtol(tmp,NULL,0))<=0){
            fprintf(stderr,"%s: argomento non valido.\n", tmp);
            free(tmp);
            exit(EXIT_FAILURE);
        }
        free(tmp);
    }

        //Se l'argomento passato non è un intero, termina.
    else {
        if ((k = strtol(argv[1], NULL, 0)) <= 0) {
            fprintf(stderr, "%s: argomento non valido.\n", argv[1]);
            exit(EXIT_FAILURE);
        }
    }
    return k;
}

/**
 * Separa i campi l ed i da s
 * @param s: stringa del tipo l,i
 * @param i: puntatore in cui salvare la seconda parte di s
 * @param l: puntatore in cui salvare la prima parte di s
 * @return : 1 (success), NULL altrimenti
 */
void* scompatta(long long int* l, int* i, char* s)
{
    char* tok=NULL;
    tok=strtok(s,",");
    if(tok==NULL)   return NULL;
    *l=(long long int)strtol(tok,NULL,0);
    tok=strtok(NULL,",");
    if (tok==NULL)  return NULL;
    *i=strtol(tok,NULL,0);

    if ((*l)<0 || (*i)<0)
        return NULL;
    return (void*)1;
}

/**
 * Print <key, value> for each couple in the hash table.
 * @param fd : where to print
 * @param ht : htable to print
 * @pre      : fd>=0 &&
 */
void printTable(int fd, icl_hash_t* ht)
{
    int iterator=0; icl_entry_t *pointer=NULL; long long int* key=0; estimate_t* data=0;
    icl_hash_foreach(ht,iterator,pointer,key,data)
    {
        if ((dprintf(fd,"SUPERVISOR ESTIMATE %d FOR %x BASED ON %d\n", \
        (int) *(data->estimate), (int)*(long long int*)key, (int) *(data->nservers))) < 0 ){
            fprintf(stderr, "File in cui stampare non trovato. \n");
            exit(EXIT_FAILURE);
        }
        fflush(stdout);
    }
}

/**Gestisce l'interruzione settando un allarme ad un secondo.
 *Se è la seconda interruzione in un secondo
 *  -> Esci dal loop, stampando la tabella nello stdout e deallocando la memoria.
 */
void sigIntHandler()
{
    int alarmsnumber;

    //Se non ci sono allarmi già settati, avvia il timer ed esci dalla funzione.
    if ((alarmsnumber=alarm(1))==0) return;
    else terminate=1;
}

/**Gestisce l'allarme avviato dal SIGINT: stampa la tabella sullo stderr    */
void sigAlarmHandler()
{
    printTable(2,htable);
}
#ifndef ESTIMATE_T_H
#define ESTIMATE_T_H

typedef struct estimate_element{
    long long int   *id;      //ID del client
    int             *estimate;//Miglior stima, aggiornata ogni volta che se ne riceve una nuova
    int             *nservers;//Numero di servers che hanno contribuito a calcolare la stima corrente
} estimate_t;

#define ALLOCA_ELEMENTO(ptr)                    \
        ptr=malloc(sizeof(estimate_t));         \
        ptr->id=malloc(sizeof(long long int));  \
        ptr->estimate=malloc(sizeof(int));      \
        ptr->nservers=malloc(sizeof(int));

//key compare function
int compareInt(void* a, void* b)    {   return (*(int*)a==*(int*)b);    }

//Delete an element
void freeElement(void* e){
    free(((estimate_t*)e)->estimate);
    free(((estimate_t*)e)->nservers);
    free(e);
}

#endif //ESTIMATE_T_H

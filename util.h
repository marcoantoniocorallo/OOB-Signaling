#ifndef UTIL_H
#define UTIL_H

#include<stdio.h>
#include<stdlib.h>
#include <stdint.h>
#include <sys/time.h>

//Dimensione buffer in cui il server trasmette al supervisor l'id del client e la sua stima.
//l'ID è memorizzato in una variabile uint64, la stima in un integer, più separatore e terminatore
#define MAXTOWRITE \
        (sizeof(long long int)+sizeof(int)+(sizeof(char)*3))

#define NBUCKETS    1024

#define MAXCONNECTIONS  64

#define ALLOCA(ptr, size)				\
		if((ptr=malloc(size))==NULL){   \
            perror("in malloc");        \
            exit(EXIT_FAILURE);         \
        }

#define SYS(sys,err,str)	\
		if (sys==err){			\
			perror(#str);		\
			exit(EXIT_FAILURE);	\
		}

#define EL(a,b,c)	\
		((a*(b-1))+(c-a-1))
/* Funzione di elaborazione presa sul web.
 * Fonte: https://www.linuxquestions.org/questions/programming-9/
 *       how-to-calculate-time-difference-in-milliseconds-in-c-c-711096/
 */
int timeval_subtract (result, x, y)
        struct timeval *result, *x, *y;
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

int MCD(int a, int b){
    if (a == 0)
        return b;
    return MCD(b%a, a);
}

int factorial( int N )
{
  int product = 1;
  for ( int j=1; j<=N; j++ )
    product *= j;
  product = product>0? product:1;
  return product;
}

//Calcola il numero più frequente
int mostFrequent(int *A, int n){

    int max=-1;
    int index=-1;
    for (int i = 0; i < n; i++) {
        int occurrences=1;
        for (int j = i+1; j < n; j++) {
            if (A[i]==A[j])
                occurrences++;
        }
        if (occurrences>max) {
            max=occurrences;
            index=i;
        }

    }

    return index;
}

//Esegue un controllo pre e post MCD:
//Se (a,b)==(1,x) -> x
//          (x,1) -> x
//Altrimenti calcola MCD e
//Se MCD==1       -> min(a,b)
int approximate(int a, int b){
    int k;

    if (a==1) return b;
    if (b==1) return a;

    if ((k=MCD(a,b))==1)
        k = a>b ? b:a;
    return k;
}

//Classica funzione member di verifica di appartenenza
int member(int* A, int k, int size){
    for (int i = 0; i < size; i++)
        if (A[i]==k) return 1;
    return 0;
}

//Funzione di inizializzazione del generatore pseudo-casuale in maniera dettagliata.
//Presa da stackoverflow, scritta da Jonathan Wright.
unsigned long mix(unsigned long a, unsigned long b, unsigned long c)
{
    a=a-b;  a=a-c;  a=a^(c >> 13);
    b=b-c;  b=b-a;  b=b^(a << 8);
    c=c-a;  c=c-b;  c=c^(b >> 13);
    a=a-b;  a=a-c;  a=a^(c >> 12);
    b=b-c;  b=b-a;  b=b^(a << 16);
    c=c-a;  c=c-b;  c=c^(b >> 5);
    a=a-b;  a=a-c;  a=a^(c >> 3);
    b=b-c;  b=b-a;  b=b^(a << 10);
    c=c-a;  c=c-b;  c=c^(b >> 15);
    return c;
}

#endif //UTIL_H

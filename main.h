#ifndef MAINH
#define MAINH
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "util.h"
/* boolean */
#define TRUE 1
#define FALSE 0
#define SEC_IN_STATE 1
#define STATE_CHANGE_PROB 10

#define ROOT 0

extern int rank;
extern int size;
extern int lamportClock;
extern int pistols;
extern int wins;
extern int localValue;
extern int tokenReady;
extern int killers[MAX_SIZE / 2];
extern int role;
extern int ackCount;
extern int complete;
extern int cycle;
extern int pairingReady;
typedef enum {REST, WAIT, PAIRING, INWANT, INSECTION, FINISHED} state_t;
extern state_t stan;
extern pthread_t threadKom, threadMon;

extern pthread_mutex_t stateMut;

extern pthread_mutex_t lamportMut;

extern pthread_mutex_t tokenMut;
extern pthread_cond_t tokenCond;

extern pthread_mutex_t pairingMut;
extern pthread_cond_t pairingCond;

extern pthread_mutex_t ackCountMut;
extern pthread_cond_t ackCond;

extern pthread_mutex_t endMut;
extern pthread_cond_t endCond;

extern pthread_mutex_t reqLogMut;

extern pthread_mutex_t sentLogMut;

extern pthread_mutex_t waitQueueMut;




/* macro debug - działa jak printf, kiedy zdefiniowano
   DEBUG, kiedy DEBUG niezdefiniowane działa jak instrukcja pusta 
   
   używa się dokładnie jak printfa, tyle, że dodaje kolorków i automatycznie
   wyświetla rank

   w związku z tym, zmienna "rank" musi istnieć.

   w printfie: definicja znaku specjalnego "%c[%d;%dm [%d]" escape[styl bold/normal;kolor [RANK]
                                           FORMAT:argumenty doklejone z wywołania debug poprzez __VA_ARGS__
					   "%c[%d;%dm"       wyczyszczenie atrybutów    27,0,37
                                            UWAGA:
                                                27 == kod ascii escape. 
                                                Pierwsze %c[%d;%dm ( np 27[1;10m ) definiuje styl i kolor literek
                                                Drugie   %c[%d;%dm czyli 27[0;37m przywraca domyślne kolory i brak pogrubienia (bolda)
                                                ...  w definicji makra oznacza, że ma zmienną liczbę parametrów
                                            
*/
#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [%d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamportClock, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

// makro println - to samo co debug, ale wyświetla się zawsze
#define println(FORMAT,...) printf("%c[%d;%dm [%d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamportClock, ##__VA_ARGS__, 27,0,37);

void changeState( state_t );

#endif

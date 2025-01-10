#ifndef UTILH
#define UTILH
#include "main.h"

#define MAX_QUEUE 20

/* typ pakietu */
typedef struct {
    int ts;       /* timestamp (zegar lamporta) */
    int src;      /* identyfikator nadawcy */
    int role;     /* rola wątku (0 = ofiara, 1 = zabójca) */
    int pair;     /* identyfikator pary */
} packet_t;
/* packet_t ma trzy pola, więc NITEMS=4. Wykorzystane w inicjuj_typ_pakietu */

typedef struct {
    packet_t queue[MAX_QUEUE]; // Kolejka oczekujących
    int size;
} WaitQueue;

// TO-DO - lamportClock niepotrzebny?
extern int lamportClock;   // Zegar Lamporta
extern int ackCount;       // Licznik zgód
extern int pistols;        // Liczba pistoletów
// TO-DO - Czy myTimestamp jest potrzebne?
//extern int myTimestamp;    // Znacznik czasowy mojego żądania
extern WaitQueue waitQueue;

#define NITEMS 4

/* Typy wiadomości */
#define APP_PKT 1
#define FINISH 2
#define ROLE_ASSIGN 3
#define REQ 4
#define ACK 5
#define DUEL 6

extern MPI_Datatype MPI_PAKIET_T;
void inicjuj_typ_pakietu();

/* wysyłanie pakietu, skrót: wskaźnik do pakietu (0 oznacza stwórz pusty pakiet), do kogo, z jakim typem */
void sendPacket(packet_t *pkt, int destination, int tag);

/* inkrementacja zegara Lamporta */
void incrementLamportClock();

/* aktualizacja stanu zegara Lamporta */
void updateLamportClock(int receivedTs);

/* funkcja przydzielająca role zabójcy/ofiary i dobierająca w parę */
packet_t assignRoleAndPair();

/* wysłanie REQ o wejście do sekcji krytycznej do reszty procesów */
void requestAccess();

int comparePriority(packet_t a, packet_t b);

/* dodanie procesu do kolejki oczekujących na dostęp */
void addToWaitQueue(int ts, int src); 

/* obsługa otrzymanego REQ od innego procesu */
void handleRequest(int ts, int src);

/* obsługa otrzymanego ACK od procesu */
void handleAck();

/* zwolnienie sekcji krytycznej i wysłanie ACK do procesów w kolejce */
void releaseAccess();

/* funkcje obsługujące pojedynek */
void duel(int pair);

void handleDuel(int pair);
#endif

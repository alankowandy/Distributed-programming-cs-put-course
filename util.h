#ifndef UTILH
#define UTILH
#include "main.h"

/* typ pakietu */
typedef struct {
    int ts;       /* timestamp (zegar lamporta) */
    int src;      /* identyfikator nadawcy */
    int role;     /* rola wątku (0 = ofiara, 1 = zabójca) */
    int pair;     /* identyfikator pary */
} packet_t;
/* packet_t ma trzy pola, więc NITEMS=4. Wykorzystane w inicjuj_typ_pakietu */

typedef struct {
    request_t queue[100]; // Kolejka oczekujących
    int size;
} WaitQueue;

extern int lamportClock;   // Zegar Lamporta
extern int ackCount;       // Licznik zgód
extern int pistols;        // Liczba pistoletów
extern int myTimestamp;    // Znacznik czasowy mojego żądania
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

void incrementLamportClock();

/* aktualizacja stanu zegara Lamporta */
void updateLamportClock(int receivedTs);

packet_t assignRoleAndPair();

/* funkcja przydzielająca role zabójcy/ofiary */
int assignRole();

void requestAccess();

void releaseAccess();

void handleRequest(packet_t *pkt, int src);
void handleReply();
void handleRelease(packet_t *pkt);

void duel();
#endif

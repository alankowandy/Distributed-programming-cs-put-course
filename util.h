#ifndef UTILH
#define UTILH
#include "main.h"

/* typ pakietu */
typedef struct {
    int ts;       /* timestamp (zegar lamporta */
    int src;  

    int role;     /* rola wątku (0 = ofiara, 1 = zabójca) */
    int pair;     /* identyfikator pary */
} packet_t;
/* packet_t ma trzy pola, więc NITEMS=4. Wykorzystane w inicjuj_typ_pakietu */

extern int lamportClock;
extern int ackCount;
extern int requestedPistols;
extern int availablePistols;

#define NITEMS 4

/* Typy wiadomości */
#define APP_PKT 1
#define FINISH 2
#define ROLE_ASSIGN 3
#define PAIR_REQ 4
#define PAIR_ACK 5
#define WEAPON_REQ 6
#define WEAPON_ACK 7
#define RELEASE 8
#define DUEL 9

extern MPI_Datatype MPI_PAKIET_T;
void inicjuj_typ_pakietu();

/* wysyłanie pakietu, skrót: wskaźnik do pakietu (0 oznacza stwórz pusty pakiet), do kogo, z jakim typem */
void sendPacket(packet_t *pkt, int destination, int tag);

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

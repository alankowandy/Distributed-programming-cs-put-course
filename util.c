#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

int ackCount = 0;
int requestedPistols = 0;

struct tagNames_t{
    const char *name;
    int tag;
} tagNames[] = { { "pakiet aplikacyjny", APP_PKT }, { "finish", FINISH}};

const char const *tag2string( int tag )
{
    for (int i=0; i <sizeof(tagNames)/sizeof(struct tagNames_t);i++) {
	if ( tagNames[i].tag == tag )  return tagNames[i].name;
    }
    return "<unknown>";
}
/* tworzy typ MPI_PAKIET_T
*/
void inicjuj_typ_pakietu()
{
    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    int       blocklengths[NITEMS] = {1,1,1,1};
    MPI_Datatype typy[NITEMS] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint     offsets[NITEMS]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
    offsets[3] = offsetof(packet_t, pair);

    MPI_Type_create_struct(NITEMS, blocklengths, offsets, typy, &MPI_PAKIET_T);

    MPI_Type_commit(&MPI_PAKIET_T);
}

/* opis patrz util.h */
void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    lamportClock++;
    pkt->ts = lamportClock;
    pkt->src = rank;
    MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    debug("Wysyłam %s do %d\n", tag2string( tag), destination);
    if (freepkt) free(pkt);
}

void changeState( state_t newState )
{
    pthread_mutex_lock( &stateMut );
    if (stan==InFinish) { 
	pthread_mutex_unlock( &stateMut );
        return;
    }
    stan = newState;
    pthread_mutex_unlock( &stateMut );
}

void updateLamportClock(int receivedTs) {
    lamportClock = (lamportClock > receivedTs ? lamportClock : receivedTs) + 1;
}

// void assignRole(int role, int pair) {
//     packet_t pkt = {0};
//     pkt.data = role;
//     pkt.pair = pair;
//     sendPacket(&pkt, ROOT, ROLE_ASSIGN);
//     debug("Przypisuję rolę: %d, paruję z: %d", role, pair);
// }

void requestAccess() {
    lamportClock++;
    //requestedPistols++;
    ackCount = 0;

    packet_t pkt = {lamportClock, rank, requestedPistols};
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&pkt, i, WEAPON_REQ);
        }
    }
    debug("Proces %d wysyła żądanie o pistolet", rank);
}

void releaseAccess() {
    lamportClock++;
    packet_t pkt = {lamportClock, rank, 0};
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&pkt, i, RELEASE);
        }
    }
    debug("Proces %d zwalnia pistolet", rank);
}

void handleRequest(packet_t *pkt, int src) {
    updateLamportClock(pkt->ts);
    debug("Proces %d otrzymał żądanie od procesu %d", rank, src);

    if (requestedPistols < availablePistols || 
        (requestedPistols == availablePistols && rank < src)) {
        packet_t reply = {lamportClock, rank, 0};
        sendPacket(&reply, src, WEAPON_ACK);
    }
}

void handleReply() {
    ackCount++;
    debug("Proces %d otrzymał WEAPON_ACK", rank);
}

void handleRelease(packet_t *pkt) {
    updateLamportClock(pkt->ts);
    availablePistols++;
    debug("Proces %d otrzymał RELEASE od %d", rank, pkt->src);
}

void duel() {
    sendPacket(NULL, pair, DUEL);
    int perc = random()%100;
    if (perc < 50) {
        wins++;
        debug("Proces %d wygrywa pojedynek", rank);
    } else {
        debug("Proces %d przegrywa pojedynek", rank);
    }
}

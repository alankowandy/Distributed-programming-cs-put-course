#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

int lamportClock = 0;
int ackCount = 0;
int myTimestamp = -1;
WaitQueue waitQueue;

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
/* tworzy typ MPI_PAKIET_T */
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
    offsets[2] = offsetof(packet_t, role);
    offsets[3] = offsetof(packet_t, pair);

    MPI_Type_create_struct(NITEMS, blocklengths, offsets, typy, &MPI_PAKIET_T);

    MPI_Type_commit(&MPI_PAKIET_T);
}

/* opis patrz util.h */
void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    incrementLamportClock();
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

void incrementLamportClock() {
    lamportClock++;
}

void updateLamportClock(int receivedTs) {
    pthread_mutex_lock( &lamportClock );
    lamportClock = (lamportClock > receivedTs ? lamportClock : receivedTs) + 1;
    pthread_mutex_unlock( &lamportClock );
}

packet_t assignRoleAndPair() {
    srandom(time(NULL) + rank); // Unikalne ziarno generatora
    int localValue = random() % 1000; // Wylosowana wartość
    int values[size]; // Tablica wartości od wszystkich procesów
    values[rank] = localValue;

    // Wysłanie własnej wartości do wszystkich procesów
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&localValue, i, 0);
        }
    }

    // Odbieranie wartości od innych procesów
    for (int i = 0; i < size - 1; i++) {
        MPI_Status status;
        int receivedValue;
        MPI_Recv(&receivedValue, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        values[status.MPI_SOURCE] = receivedValue;
    }

    // Sortowanie wartości
    int sortedRanks[size];
    for (int i = 0; i < size; i++) sortedRanks[i] = i;

    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (values[sortedRanks[i]] > values[sortedRanks[j]]) {
                int temp = sortedRanks[i];
                sortedRanks[i] = sortedRanks[j];
                sortedRanks[j] = temp;
            }
        }
    }

    // Przydzielenie roli i dobór par
    packet_t result;
    int half = size / 2;
    result.role = (rank < half) ? 1 : 0; // Zabójcy mają najniższe wartości

    if (result.role == 1) { // Zabójcy wybierają ofiary
        int victimIndex = half + (rank % half); // Wybór ofiary
        result.pair = sortedRanks[victimIndex];
        debug("Proces %d jest zabójcą i dobiera ofiarę %d", rank, result.pair);
    } else { // Ofiary czekają na atak
        result.pair = -1; // Ofiara na razie nie ma przypisanej pary
        debug("Proces %d jest ofiarą", rank);
    }
    return result;
}

// Wysłanie REQ do wszystkich
void requestAccess() {
    incrementLamportClock();
    myTimestamp = lamportClock;
    ackCount = 0;

    packet_t req = {myTimestamp, rank};
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&req, i, REQ);
        }
    }
    debug("Proces %d wysłał REQ", rank);
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

// Obsługa otrzymanego REQ
void handleRequest(int ts, int src) {
    updateLamportClock(ts);
    debug("Proces %d otrzymał REQ od %d", rank, src);

    // Jeśli moje żądanie ma niższy priorytet
    if (ts < myTimestamp || (ts == myTimestamp && src < rank)) {
        packet_t ack = {lamportClock, rank};
        sendPacket(&ack, src, ACK);
        debug("Proces %d wysłał ACK do %d", rank, src);
    } else {
        addToWaitQueue(ts, src);
        debug("Proces %d dodał %d do kolejki oczekujących", rank, src);
    }
}

// Obsługa otrzymanego ACK
void handleAck() {
    ackCount++;
    debug("Proces %d otrzymał ACK (ackCount=%d)", rank, ackCount);
}

// Zwolnienie sekcji krytycznej i wysłanie ACK do procesów w kolejce
void releaseAccess() {
    myTimestamp = -1;
    debug("Proces %d zwalnia sekcję krytyczną", rank);

    // Wysłanie ACK do procesów z kolejki
    for (int i = 0; i < waitQueue.size; i++) {
        packet_t ack = {lamportClock, rank};
        sendPacket(&ack, waitQueue.queue[i].src, ACK);
        debug("Proces %d wysłał ACK do %d z kolejki", rank, waitQueue.queue[i].src);
    }
    waitQueue.size = 0;
}

// TO-DO
void duel(int pair) {
    int perc = random()%100;
    if (perc < 50) {
        wins++;
        debug("Proces %d wygrywa pojedynek", rank);
    } else {
        debug("Proces %d przegrywa pojedynek", rank);
    }
}

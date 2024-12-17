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

/* funkcja przydzielająca role zabójcy/ofiary */
int assignRole() {
    srandom(time(NULL) + rank); // Unikalne ziarno generatora
    int localValue = random() % 1000; // Wylosowana wartość
    int values[size]; // Tablica do przechowywania wartości od wszystkich procesów
    values[rank] = localValue; // Zapis własnej wartości

    // Wysłanie własnej wartości do wszystkich innych procesów
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            MPI_Send(&localValue, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }

    // Odbieranie wartości od innych procesów
    for (int i = 0; i < size - 1; i++) { // size - 1, bo własna wartość już jest zapisana
        MPI_Status status;
        int receivedValue;
        MPI_Recv(&receivedValue, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        values[status.MPI_SOURCE] = receivedValue;
    }

    // Sortowanie wartości lokalnie
    int sortedRanks[size];
    for (int i = 0; i < size; i++) sortedRanks[i] = i; // Inicjalizacja tablicy ranków

    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (values[sortedRanks[i]] > values[sortedRanks[j]]) {
                int temp = sortedRanks[i];
                sortedRanks[i] = sortedRanks[j];
                sortedRanks[j] = temp;
            }
        }
    }

    // Określenie roli: pierwsza połowa to zabójcy, druga połowa to ofiary
    for (int i = 0; i < size / 2; i++) {
        if (sortedRanks[i] == rank) {
            debug("Proces %d wylosował %d i jest zabójcą", rank, localValue);
            return 1; // Zabójca
        }
    }
    debug("Proces %d wylosował %d i jest ofiarą", rank, localValue);
    return 0; // Ofiara
}

void requestAccess() {
    lamportClock++;
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

    packet_t reply = {lamportClock, rank, 0};
    sendPacket(&reply, src, WEAPON_ACK);
}

void handleReply() {
    ackCount++;
    debug("Proces %d otrzymał WEAPON_ACK", rank);
}

void handleRelease(packet_t *pkt) {
    updateLamportClock(pkt->ts);
    debug("Proces %d otrzymał RELEASE od %d", rank, pkt->src);
}

void duel(int pair) {
    sendPacket(NULL, pair, DUEL);
    int perc = random()%100;
    if (perc < 50) {
        wins++;
        debug("Proces %d wygrywa pojedynek", rank);
    } else {
        debug("Proces %d przegrywa pojedynek", rank);
    }
}

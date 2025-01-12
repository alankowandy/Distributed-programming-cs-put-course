#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

int lamportClock = 0;
int ackCount = 0;
WaitQueue waitQueue = { .size = 0 }; // Inicjalizacja kolejki

struct tagNames_t{
    const char *name;
    int tag;
} tagNames[] = { { "token do uzupełnienia", INITIAL_TOKEN }, { "uzupełniony token", FINAL_TOKEN}, { "REQ", REQ }, { "ACK", ACK }, { "DUEL", DUEL } };

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
    int       blocklengths[NITEMS] = {1,1,1,1,1};
    MPI_Datatype typy[NITEMS] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint     offsets[NITEMS]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, role);
    offsets[3] = offsetof(packet_t, pair);
    offsets[4] = offsetof(packet_t, token);

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
    if (stan==FINISHED) { 
	pthread_mutex_unlock( &stateMut );
        return;
    }
    stan = newState;
    pthread_mutex_unlock( &stateMut );
}

void incrementLamportClock() {
    pthread_mutex_lock( &lamportMut );
    lamportClock++;
    pthread_mutex_unlock( &lamportMut );
}

void updateLamportClock(int receivedTs) {
    pthread_mutex_lock( &lamportMut );
    lamportClock = (lamportClock > receivedTs ? lamportClock : receivedTs) + 1;
    pthread_mutex_unlock( &lamportMut );
}

packet_t assignRoleAndPair() {
    changeState(PAIRING);
    srandom(time(NULL) + rank); // Unikalne ziarno generatora
    int localValue = random() % 1000; // Wylosowana wartość
    int token[size];  // Token do przesyłania wartości w pierścieniu
    int tokenReady = 0; // Gotowość tokenu
    packet_t tokenValues; // Token z wartościami

    if (rank == 0) {
        // Proces 0 inicjuje token i zapisuje swoją wartość
        token[0] = localValue;
        for (int i = 1; i < size; i++) {
            token[i] = -1; // Inicjalizacja pustych miejsc
        }

        tokenValues.token = token;
        incrementLamportClock();
        tokenValues.ts = lamportClock;
        tokenValues.src = rank;
        // Przekazanie tokenu do następnego procesu
        MPI_Send(tokenValues, 1, MPI_PAKIET_T, 1, INITIAL_TOKEN, MPI_COMM_WORLD);
        debug("Wysłałem token do procesu 1");

        // Odbiór finalnego tokenu z wartościami od ostatniego procesu
        MPI_Recv(tokenValues, 1, MPI_PAKIET_T, size - 1, FINAL_TOKEN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        updateLamportClock(tokenValues.ts);
        debug("Odebrałem finalny token od procesu %d", size - 1);

        token = tokenValues.token;
        incrementLamportClock();
        tokenValues.ts = lamportClock;
        tokenValues.src = rank;
        // Przekazanie wypełnionego tokenu do następnego procesu
        MPI_Send(tokenValues, 1, MPI_PAKIET_T, 1, INITIAL_TOKEN, MPI_COMM_WORLD);
        debug("Wysłałem posortowany token do procesu 1");
    } else {
        while (!tokenReady) {
            sleep(SEC_IN_STATE); // Czekanie na gotowość tokenu
        }
    }

    // Sortowanie wartości
    debug("Sortuję wartości...");
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (token[i] > token[j]) {
                int temp = token[i];
                token[i] = token[j];
                token[j] = temp;
            }
        }
    }

    // Znalezienie swojej pozycji w posortowanej tablicy
    int myPosition = -1;
    for (int i = 0; i < size; i++) {
        if (token[i] == rank) {
            myPosition = i;
            break;
        }
    }

    // Przydzielenie roli i dobór par
    packet_t result;
    int half = size / 2;

    if (myPosition < half) {
        result.role = 1; // Zabójca
        result.pair = token[half + (myPosition % half)]; // Dobór ofiary
        debug("Jestem zabójcą. Dobieram ofiarę %d", result.pair);
    } else {
        result.role = 0; // Ofiara
        result.pair = -1; // Ofiara nie dobiera pary
        debug("Jestem ofiarą");
    }

    // if (rank == 0) {
    //     debug("Finalna posortowana tablica:");
    //     for (int i = 0; i < size; i++) {
    //         debug("[%d] = %d", sortedIndices[i], values[sortedIndices[i]]);
    //     }
    // }

    return result;
}

// Wysłanie REQ do wszystkich
void requestAccess() {
    incrementLamportClock();
    ackCount = 0;

    packet_t req = {lamportClock, rank};
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            sendPacket(&req, i, REQ);
        }
    }
    debug("Wysłałem REQ");
    changeState(WAIT);
}

int comparePriority(packet_t a, packet_t b) {
    if (a.ts < b.ts) return -1; // Mniejszy znacznik czasu = wyższy priorytet
    if (a.ts > b.ts) return 1;
    return (a.src < b.src) ? -1 : 1; // Przy równych znacznikach czasu niższy rank = wyższy priorytet
}

/* dodanie procesu do kolejki oczekujących na dostęp */
void addToWaitQueue(int ts, int src) {
    if (waitQueue.size < 100) {
        packet_t pkt = { .ts = ts, .src = src };

        // Dodajemy nowy element na koniec kolejki
        waitQueue.queue[waitQueue.size] = pkt;
        waitQueue.size++;

        // Sortowanie kolejki według priorytetu
        for (int i = waitQueue.size - 1; i > 0; i--) {
            if (comparePriority(waitQueue.queue[i - 1], waitQueue.queue[i]) > 0) {
                // Zamiana elementów, jeśli poprzedni ma niższy priorytet
                packet_t temp = waitQueue.queue[i - 1];
                waitQueue.queue[i - 1] = waitQueue.queue[i];
                waitQueue.queue[i] = temp;
            } else {
                break; // Kolejka jest już uporządkowana
            }
        }

        debug("Dodałem REQ od %d (ts=%d) do kolejki priorytetowej", src, ts);
    } else {
        debug("Kolejka oczekujących jest pełna, nie można dodać REQ od %d", src);
    }
}

// Obsługa otrzymanego REQ
void handleRequest(int ts, int src) {
    updateLamportClock(ts);
    debug("Otrzymałem REQ od %d", src);

    // jeśli moje żądanie ma niższy priorytet to wysyłam ACK
    if (ts < lamportClock || (ts == lamportClock && src < rank)) {
        packet_t ack = {lamportClock, rank};
        sendPacket(&ack, src, ACK);
        debug("Wysłałem ACK do %d", src);
    } else { // jeśli moje żądanie ma większy priorytet to dodaje do kolejki oczekujacych
        addToWaitQueue(ts, src);
        debug("Dodałem %d do kolejki oczekujących", src);
    }
}

// Obsługa otrzymanego ACK
void handleAck() {
    ackCount++;
    debug("Otrzymałem ACK (ackCount=%d)", ackCount);
}

// Zwolnienie sekcji krytycznej i wysłanie ACK do procesów w kolejce
void releaseAccess() {
    debug("Zwalniam sekcję krytyczną");

    // Wysłanie ACK do procesów z kolejki
    for (int i = 0; i < waitQueue.size; i++) {
        packet_t ack = {lamportClock, rank};
        sendPacket(&ack, waitQueue.queue[i].src, ACK);
        debug("Wysłałem ACK do %d z kolejki", waitQueue.queue[i].src);
    }
    waitQueue.size = 0;
}

// TO-DO
void duel(int pair) {
    int perc = random()%100;
    int win;
    if (perc > 50) {
        win = 1;
        MPI_Send(&win, 1, MPI_INT, pair, DUEL, MPI_COMM_WORLD);
        wins++;
        debug("Wygrywam pojedynek i zabijam %d", pair);
    } else {
        win = 0;
        MPI_Send(&win, 1, MPI_INT, pair, DUEL, MPI_COMM_WORLD);
    }
}

void handleDuel(int pair) {
    int win;
    MPI_Status status;
    MPI_Recv(&win, 1, MPI_INT, pair, DUEL, MPI_COMM_WORLD, &status);
    if (!win) {
        debug("Wygrywam pojedynek i uciekam przed %d", pair);
        wins++;
    } else {
        debug("Przegrywam pojedynek");
    }
}

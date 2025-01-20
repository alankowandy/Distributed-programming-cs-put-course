#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

//int lamportClock = 0;
int pairValue = 0;
int release = 0;
int token[MAX_SIZE];
int killers[MAX_SIZE / 2];
int myPosition = -1;

int sentPacketsCount = 0;
struct SentPacketLog sentPacketsLog[MAX_LOG_SIZE];

int receivedPacketsCount = 0;
struct receivedPacket_t receivedPacketsLog[MAX_LOG_SIZE];

packet_t messageQueue[QUEUE_SIZE];
int queueStart = 0, queueEnd = 0;

WaitQueue waitQueue = { .size = 0 }; // Inicjalizacja kolejki

int ackLogCount = 0;
int ackLog[MAX_ACK_LOG_SIZE];

struct tagNames_t{
    const char *name;
    int tag;
} tagNames[] = { { "token do uzupełnienia", INITIAL_TOKEN }, 
                { "uzupełniony token", FINAL_TOKEN}, 
                { "REQ", REQ }, 
                { "ACK", ACK }, 
                { "DUEL", DUEL },
                { "RELEASE", RELEASE } };

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
    int       blocklengths[NITEMS] = {1,1,1,1,MAX_SIZE,1};
    MPI_Datatype typy[NITEMS] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint     offsets[NITEMS]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, role);
    offsets[3] = offsetof(packet_t, pair);
    offsets[4] = offsetof(packet_t, token);
    offsets[5] = offsetof(packet_t, win);

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
    //srand(time(NULL) + rank);
    if (cycle == 0) {
        srandom(time(NULL) + rank + rand()); // Unikalne ziarno generatora
    }
    localValue = random() % 1000; // Wylosowana wartość
    debug("my local value: %d", localValue);
    packet_t tokenValues; // Token z wartościami

    if (rank == 0) {
        // Proces 0 inicjuje token i zapisuje swoją wartość
        token[0] = localValue;
        debug("Dodałem swoją wartość %d do tokenu", localValue);
        for (int i = 1; i < size; i++) {
            token[i] = 0; // Inicjalizacja pustych miejsc
        }

        memcpy(tokenValues.token, token, MAX_SIZE * sizeof(int));
        incrementLamportClock();
        tokenValues.ts = lamportClock;
        tokenValues.src = rank;
        // Przekazanie tokenu do następnego procesu
        MPI_Send(&tokenValues, 1, MPI_PAKIET_T, 1, INITIAL_TOKEN, MPI_COMM_WORLD);
        debug("Wysłałem token do procesu 1");

        pthread_mutex_lock(&tokenMut);
        while (!tokenReady) {
            pthread_cond_wait(&tokenCond, &tokenMut); // Czekanie na gotowość tokenu
        }
        pthread_mutex_unlock(&tokenMut);
    } else {
        pthread_mutex_lock(&tokenMut);
        while (!tokenReady) {
            pthread_cond_wait(&tokenCond, &tokenMut); // Czekanie na gotowość tokenu
        }
        pthread_mutex_unlock(&tokenMut);
    }

    memcpy(tokenValues.token, token, MAX_SIZE * sizeof(int));
    for (int i = 0; i < size; i++)
    {
        debug("tokenvalues[%d] = %d", i, tokenValues.token[i]);
    }

    
    
    // Sortowanie wartości
    //debug("Sortuję wartości...");
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (token[i] > token[j]) {
                int temp = token[i];
                token[i] = token[j];
                token[j] = temp;
            }
        }
    }

    debug("my local value: %d", localValue);
    for (int i = 0; i < size; i++)
        {
            debug("token[%d] = %d", i, token[i]);
        }
    // for (int i = 0; i < size; i++)
    // {
    //     debug("tokenvalues[%d] = %d", i, token[i]);
    // }

    // Znalezienie swojej pozycji w posortowanej tablicy
    for (int i = 0; i < size; i++) {
        if (token[i] == localValue) {
            myPosition = i;
            debug("myposition: %d", myPosition);
            break;
        }
    }

    if (myPosition == -1) {
        debug("błąd w myposition");
    }

    // Przydzielenie roli i dobór par
    packet_t result;
    int half = size / 2;

    if (myPosition < half) {
        result.role = 1; // Zabójca
        pairValue = token[half + myPosition];
        debug("pair value: %d", pairValue);
        for (int i = 0; i < size; i++) {
            if (tokenValues.token[i] == pairValue) {
                result.pair = i;
            }
        }
        debug("Jestem zabójcą. Dobieram ofiarę - proces %d", result.pair);
    } else {
        result.role = 0; // Ofiara
        result.pair = -1; // Ofiara nie dobiera pary
        debug("Jestem ofiarą");
    }

    if (rank == 0) {
        debug("Finalna posortowana tablica:");
        for (int i = 0; i < size; i++) {
            debug("[%d] = %d", i, token[i]);
        }
    }

    int killerCount = 0; // Licznik zabójców

    // Przejdź przez posortowaną tablicę token
    for (int i = 0; i < half; i++) { // Pierwsza połowa to zabójcy
        for (int j = 0; j < size; j++) { // Znajdź rank dla wartości token[i]
            if (token[i] == tokenValues.token[j]) {
                killers[killerCount] = j;          // Rank zabójcy
                killerCount++;
                break;
            }
        }
    }

    // Wyświetlenie danych o zabójcach
    debug("Lista zabójców:");
    for (int i = 0; i < killerCount; i++) {
        debug("Zabójca %d: Proces %d", i + 1, killers[i]);
    }

    return result;
}

void requestAccess() {
    changeState(INWANT);
    int killerCount = size / 2;

    for (int i = 0; i < killerCount; i++) {
        if (killers[i] != rank) { // Nie wysyłaj do siebie
            int alreadyReceivedREQ = 0;
            int receivedREQTimestamp = -1;
            int currentAckCount;

            pthread_mutex_lock(&ackCountMut);
            currentAckCount = ackCount;
            pthread_mutex_unlock(&ackCountMut);

            if (currentAckCount >= size - pistols) {
                debug("Osiągnęliśmy ackCount=%d, nie wysyłamy REQ do procesu %d", currentAckCount, killers[i]);
                continue; // Skip sending REQ if the condition is met
            }

            pthread_mutex_lock(&reqLogMut);
            // Sprawdzanie, czy już otrzymaliśmy REQ od tego procesu
            for (int j = 0; j < receivedPacketsCount; j++) {
                if (receivedPacketsLog[j].source == killers[i]) {
                    alreadyReceivedREQ = 1;
                    receivedREQTimestamp = receivedPacketsLog[j].lamportClock;
                    break;
                }
            }
            pthread_mutex_unlock(&reqLogMut);

            // Jeżeli otrzymaliśmy już REQ od tego procesu i ma on mniejszy priorytet to nie wysyłamy REQ
            // ponieważ zakładamy że proces wysyłający REQ do nas automatycznie przypisał sobie ACK
            if (!alreadyReceivedREQ || lamportClock <= receivedREQTimestamp) {
                sendPacket(0, killers[i], REQ);
                pthread_mutex_lock(&sentLogMut);
                if (sentPacketsCount < MAX_LOG_SIZE) { // Założenie: MAX_LOG_SIZE to maksymalny rozmiar tablicy
                    sentPacketsLog[sentPacketsCount].destination = killers[i];
                    sentPacketsLog[sentPacketsCount].lamportClock = lamportClock;
                    sentPacketsCount++;
                    //printSentPacketsLog();
                } else {
                    debug("Tablica logów wysyłanych pakietów jest pełna!\n");
                }
                pthread_mutex_unlock(&sentLogMut);
            }
        }
    }
}

// Wysłanie REQ do wszystkich
// void requestAccess() {
//     changeState(INWANT);
//     int killerCount = size / 2;

//     //packet_t *req = malloc(sizeof(packet_t));

//     // for (int i = 0; i < killerCount; i++) {
//     //     if (killers[i] != rank) { // Nie wysyłaj do siebie
//     //         sendPacket(&req, killers[i], REQ);
//     //         //debug("Zabójca %d (rank %d) wysłał wiadomość do zabójcy %d", rank, rank, killers[i]);
//     //     }
//     // }

//     // TO-DO - dodać sprawdzanie przed każdym kolejnym wysłaniem czy już mamy wystarczającą ilość ACK

//     for (int i = 0; i < killerCount; i++) {
//         if (killers[i] != rank) { // Nie wysyłaj do siebie
//             int alreadyReceivedREQ = 0;
//             int receivedREQTimestamp = -1;

//             pthread_mutex_lock(&reqLogMut);
//             // Sprawdzanie, czy już otrzymaliśmy REQ od tego procesu
//             for (int j = 0; j < receivedPacketsCount; j++) {
//                 if (receivedPacketsLog[j].source == killers[i]) {
//                     alreadyReceivedREQ = 1;
//                     receivedREQTimestamp = receivedPacketsLog[j].lamportClock;
//                     break;
//                 }
//             }
//             pthread_mutex_unlock(&reqLogMut);

//             // Jeżeli otrzymaliśmy już REQ od tego procesu i ma on mniejszy priorytet to nie wysyłamy REQ
//             // ponieważ zakładamy że proces wysyłający REQ do nas automatycznie przypisał sobie ACK
//             if (!alreadyReceivedREQ || lamportClock <= receivedREQTimestamp) {
//                 sendPacket(0, killers[i], REQ);
//                 pthread_mutex_lock(&sentLogMut);
//                 if (sentPacketsCount < MAX_LOG_SIZE) { // Założenie: MAX_LOG_SIZE to maksymalny rozmiar tablicy
//                     sentPacketsLog[sentPacketsCount].destination = killers[i];
//                     sentPacketsLog[sentPacketsCount].lamportClock = lamportClock;
//                     sentPacketsCount++;
//                     //printSentPacketsLog();
//                 } else {
//                     debug("Tablica logów wysyłanych pakietów jest pełna!\n");
//                 }
//                 pthread_mutex_unlock(&sentLogMut); 
//             }
//         }
//     }
// }

int comparePriority(packet_t a, packet_t b) {
    if (a.ts < b.ts) return -1; // Mniejszy znacznik czasu = wyższy priorytet
    if (a.ts > b.ts) return 1;
    return (a.src < b.src) ? -1 : 1; // Przy równych znacznikach czasu niższy rank = wyższy priorytet
}

/* dodanie procesu do kolejki oczekujących na dostęp */
void addToWaitQueue(int ts, int src) {
    pthread_mutex_lock(&waitQueueMut);
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
    pthread_mutex_unlock(&waitQueueMut);
}

// Obsługa otrzymanego REQ
void handleRequest(int ts, int src) {

    // jeśli moje żądanie ma niższy priorytet to wysyłam ACK
    if (ts < lamportClock || (ts == lamportClock && src < rank)) {
        sendPacket(0, src, ACK);
    } else { // jeśli moje żądanie ma większy priorytet to dodaje do kolejki oczekujacych
        addToWaitQueue(ts, src);
        debug("Dodałem %d do kolejki oczekujących", src);
    }
}

// Obsługa otrzymanego ACK
void handleAck(int src) {
    pthread_mutex_lock(&ackCountMut);
    ackCount++;
    debug("Otrzymałem ACK od %d (ackCount=%d)", src, ackCount);
    pthread_cond_signal(&ackCond); // Powiadomienie wątku głównego by sprawdził czy warunek wejścia do sekcji jest spełniony

    if (ackLogCount < MAX_ACK_LOG_SIZE) {
        ackLog[ackLogCount] = src; // Zapis ID procesu, od którego otrzymano ACK
        ackLogCount++;
    } else {
        debug("Tablica ackLog jest pełna! Nie można zapisać procesu %d", src);
    }
    pthread_mutex_unlock(&ackCountMut);
    //printAckLog();
}

// Zwolnienie sekcji krytycznej i wysłanie ACK do procesów w kolejce
void releaseAccess() {
    pthread_mutex_lock(&waitQueueMut);
    debug("Zwalniam sekcję krytyczną");

    // Wysłanie ACK do procesów z kolejki
    for (int i = 0; i < waitQueue.size; i++) {
        sendPacket(0, waitQueue.queue[i].src, ACK);
        debug("Wysłałem ACK do %d z kolejki", waitQueue.queue[i].src);
    }
    waitQueue.size = 0;
    ackCount = 0;
    changeState(REST);
    pthread_mutex_unlock(&waitQueueMut);
}

void duel(int pair) {
    packet_t *pkt = malloc(sizeof(packet_t));
    int perc = random()%1000;
    debug("Atakuje proces %d", pair);
    if (perc > 500) {
        pkt->win = 1;
        pkt->pair = pair;
        sendPacket(pkt, pair, DUEL);
        wins++;
        debug("Wygrywam pojedynek i zabijam proces %d", pair);
        free(pkt);
    } else {
        pkt->win = 0;
        pkt->pair = pair;
        sendPacket(pkt, pair, DUEL);
        debug("Przegrywam pojedynek");
        free(pkt);
    }
}

void handleDuel(int pair, int win) {
    if (!win) {
        debug("Wygrywam pojedynek i uciekam przed procesem %d", pair);
        wins++;
    } else {
        debug("Przegrywam pojedynek");
    }
    changeState(REST);
}

void sleepThread(int milliseconds) {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec += milliseconds / 1000;
    ts.tv_nsec += (milliseconds % 1000) * 1000000;

    pthread_mutex_lock(&mutex);
    pthread_cond_timedwait(&cond, &mutex, &ts);
    pthread_mutex_unlock(&mutex);
}

void resetVariables() {
    sentPacketsCount = 0;
    receivedPacketsCount = 0;
    ackLogCount = 0;
    waitQueue.size = 0;
    tokenReady = 0;
    role = -1;
    myPosition = -1;
    ackCount = 0;
    //pair = -1;

    memset(sentPacketsLog, 0, sizeof(sentPacketsLog));
    memset(receivedPacketsLog, 0, sizeof(receivedPacketsLog));
    memset(messageQueue, 0, sizeof(messageQueue));
    memset(ackLog, 0, sizeof(ackLog));
    memset(killers, 0, sizeof(killers));
}

// Wypisanie logów wysłanych pakietów
void printSentPacketsLog() {
    debug("Log wysłanych pakietów (sentPacketsLog):");
    for (int i = 0; i < sentPacketsCount; i++) {
        debug("[%d] -> Cel: %d, Zegar Lamporta: %d", 
              i, sentPacketsLog[i].destination, sentPacketsLog[i].lamportClock);
    }
}

// Wypisanie logów odebranych pakietów
void printReceivedPacketsLog() {
    debug("Log odebranych pakietów (receivedPacketsLog):");
    for (int i = 0; i < receivedPacketsCount; i++) {
        debug("[%d] -> Źródło: %d, Zegar Lamporta: %d", 
              i, receivedPacketsLog[i].source, receivedPacketsLog[i].lamportClock);
    }
}

// Wypisanie logów ACK
void printAckLog() {
    debug("Log ACK (ackLog):");
    for (int i = 0; i < ackLogCount; i++) {
        debug("[%d] -> Proces ACK: %d", i, ackLog[i]);
    }
}
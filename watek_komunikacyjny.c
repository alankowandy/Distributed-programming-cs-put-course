#include "main.h"
#include "watek_komunikacyjny.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
    MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
    int nextProcess = (rank + 1) % size;
    int killerCount = size / 2; // Liczba zabójców
    
    while (stan != FINISHED) {
        MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        switch (status.MPI_TAG) {
            case INITIAL_TOKEN:
                if (rank != 0) {
                    updateLamportClock(pakiet.ts);
                    while (stan != PAIRING) {}
                    debug("Odebrałem token od procesu %d", pakiet.src);
                    for (int i = 0; i < size; i++) {
                        if (pakiet.token[i] == localValue) {
                            localValue = rand() % 1000;
                            debug("Zmieniam moją wylosowaną wartość na %d ze względu na duplikat.", localValue);
                        }
                    }
                    
                    // Dodanie swojej wartości do tokenu
                    pakiet.token[rank] = localValue;
                    debug("Dodałem swoją wartość %d do tokenu", localValue);

                    // Przekazanie tokenu do następnego procesu
                    incrementLamportClock();
                    pakiet.ts = lamportClock;
                    pakiet.src = rank;
                    if (rank == size - 1) {
                        MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, FINAL_TOKEN, MPI_COMM_WORLD);
                        debug("Wysłałem token do procesu %d", nextProcess);
                    } else {
                        MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, INITIAL_TOKEN, MPI_COMM_WORLD);
                        debug("Wysłałem token do procesu %d", nextProcess);
                    }
                } 
            break;
            case FINAL_TOKEN:
                updateLamportClock(pakiet.ts);
                debug("Odebrałem wypełniony token od procesu %d", pakiet.src);

                // Przekazanie wypełnionego tokenu do następnego procesu
                incrementLamportClock();
                pakiet.ts = lamportClock;
                pakiet.src = rank;
                if (rank != size - 1) {
                    MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, FINAL_TOKEN, MPI_COMM_WORLD);
                    debug("Wysłałem wypełniony token do procesu %d", nextProcess);
                }

                memcpy(token, pakiet.token, MAX_SIZE * sizeof(int));
                while (stan != PAIRING) {}
                
                pthread_mutex_lock(&tokenMut);
                tokenReady = 1;
                pthread_cond_signal(&tokenCond);
                pthread_mutex_unlock(&tokenMut);
                role = -1;
                changeState(WAIT);
            break;
            case REQ:
                updateLamportClock(pakiet.ts);
                debug("Otrzymałem REQ od %d", status.MPI_SOURCE);

                // Zapisanie otrzymanego REQ do tablicy
                pthread_mutex_lock(&reqLogMut);
                int found = 0;
                for (int i = 0; i < receivedPacketsCount; i++) {
                    if (receivedPacketsLog[i].source == status.MPI_SOURCE) {
                        receivedPacketsLog[i].lamportClock = pakiet.ts;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    receivedPacketsLog[receivedPacketsCount].source = status.MPI_SOURCE;
                    receivedPacketsLog[receivedPacketsCount].lamportClock = pakiet.ts;
                    receivedPacketsCount++;
                }
                pthread_mutex_unlock(&reqLogMut);

                // Sprawdzenie, czy wcześniej wysłano REQ do tego procesu
                int addedACK = 0;
                pthread_mutex_lock(&sentLogMut);
                for (int i = 0; i < sentPacketsCount; i++) {
                    if (sentPacketsLog[i].destination == pakiet.src && sentPacketsLog[i].tag == REQ) {
                        if (sentPacketsLog[i].lamportClock < pakiet.ts) {
                            // Sprawdzenie, czy nie otrzymano już ACK od tego procesu, jeżeli już wysłaliśmy REQ
                            // a jeżeli już otrzymaliśmy to nie dodajemy sobie z góry ACK przy założeniu że mamy wyższy priorytet
                            int alreadyReceivedACK = 0;
                            for (int j = 0; j < ackLogCount; j++) {
                                if (ackLog[j] == pakiet.src) {
                                    alreadyReceivedACK = 1;
                                    break;
                                }
                            }

                            int currentAckCount;
                            pthread_mutex_lock(&ackCountMut);
                            currentAckCount = ackCount;
                            pthread_mutex_unlock(&ackCountMut);

                            // Dodanie ACK z góry, jeśli jeszcze go nie otrzymano, a wysłaliśmy REQ i teraz dostaliśmy REQ od tego samego procesu
                            // o mniejszym priorytecie oraz nasze ackCount <= size/2 - pistols
                            if (!alreadyReceivedACK && currentAckCount < size/2 - pistols) {
                                addToWaitQueue(pakiet.ts, pakiet.src);
                                debug("Dodaję z góry ACK od procesu %d (ackCount=%d)", pakiet.src, currentAckCount);
                                handleAck(pakiet.src);
                                pthread_mutex_lock(&ackCountMut);
                                currentAckCount = ackCount;
                                pthread_mutex_unlock(&ackCountMut);
                                addedACK = 1;                            
                            }
                            
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&sentLogMut);

                if (stan == REST) {
                    sendPacket(0, pakiet.src, ACK);
                } else if (!addedACK) {
                    handleRequest(pakiet.ts, pakiet.src);
                }
                
            break;
            case ACK:
                updateLamportClock(pakiet.ts);
                if (stan != REST && stan != INSECTION) {
                    handleAck(pakiet.src);
                }
            break;
            case DUEL:
                updateLamportClock(pakiet.ts);
                handleDuel(pakiet.pair, pakiet.win);
            break;
            case RELEASE:
                changeState(FINISHED);
            break;
        }
    }
    //debug("skończyłem robotę - wątek komunikacyjny");
}

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
        //updateLamportClock(pakiet.ts);

        switch (status.MPI_TAG) {
            case INITIAL_TOKEN:
                if (rank != 0) {
                    updateLamportClock(pakiet.ts);
                    debug("Odebrałem token od procesu %d", pakiet.src);

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
                pthread_mutex_lock(&tokenMut);
                tokenReady = 1;
                pthread_cond_signal(&tokenCond);
                pthread_mutex_unlock(&tokenMut);
                role = -1;
                changeState(WAIT);
            break;
            case REQ:
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
                    //printReceivedPacketsLog();
                }
                pthread_mutex_unlock(&reqLogMut);
                debug("przeszedłem przez req mutexa");

                // Sprawdzenie, czy wcześniej wysłano REQ do tego procesu
                pthread_mutex_lock(&sentLogMut);
                for (int i = 0; i < sentPacketsCount; i++) {
                    if (sentPacketsLog[i].destination == pakiet.src) {
                        if (sentPacketsLog[i].lamportClock < pakiet.ts) {
                            // Sprawdzenie, czy nie otrzymano już ACK od tego procesu
                            int alreadyReceivedACK = 0;
                            for (int j = 0; j < ackLogCount; j++) {
                                if (ackLog[j] == pakiet.src) {
                                    alreadyReceivedACK = 1;
                                    break;
                                }
                            }

                            // Dodanie ACK, jeśli jeszcze go nie otrzymano
                            if (!alreadyReceivedACK) {
                                handleAck(pakiet.src);
                                debug("Dodano z góry ACK od procesu %d (ackCount=%d)", pakiet.src, ackCount);
                                pthread_cond_signal(&ackCond);                              
                            }
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&sentLogMut);
                debug("przeszedłem przez sent mutexa");

                handleRequest(pakiet.ts, pakiet.src);
                if (ackCount >= size/2 - pistols) {
                    changeState(INSECTION);
                }
                updateLamportClock(pakiet.ts);
            break;
            case ACK:
                updateLamportClock(pakiet.ts);
                handleAck(pakiet.src);
                if (ackCount >= size/2 - pistols) {
                    changeState(INSECTION);
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
    debug("skończyłem robotę - wątek komunikacyjny");

        // while (stan == REST || stan == PAIRING) {
        //     if (rank != 0) {
        //         debug("jestem w rest/pairing");
        //         MPI_Recv( &pakiet, 1, MPI_PAKIET_T, rank - 1, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        //         updateLamportClock(pakiet.ts);

        //         switch (status.MPI_TAG) {
        //             case INITIAL_TOKEN:
        //                 debug("Odebrałem token od procesu %d", rank - 1);

        //                 // Dodanie swojej wartości do tokenu
        //                 pakiet.token[rank] = localValue;
        //                 debug("Dodałem swoją wartość %d do tokenu", localValue);

        //                 // Przekazanie tokenu do następnego procesu
        //                 incrementLamportClock();
        //                 pakiet.ts = lamportClock;
        //                 pakiet.src = rank;
        //                 MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, INITIAL_TOKEN, MPI_COMM_WORLD);
        //                 debug("Wysłałem token do procesu %d", nextProcess);
        //             break;
        //             case FINAL_TOKEN:
        //                 debug("Odebrałem wypełniony token od procesu %d", rank - 1);

        //                 // Przekazanie wypełnionego tokenu do następnego procesu
        //                 incrementLamportClock();
        //                 pakiet.ts = lamportClock;
        //                 pakiet.src = rank;
        //                 if (rank != size - 1) {
        //                     MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, FINAL_TOKEN, MPI_COMM_WORLD);
        //                     debug("Wysłałem wypełniony token do procesu %d", nextProcess);
        //                 }

        //                 memcpy(token, pakiet.token, MAX_SIZE * sizeof(int));
        //                 pthread_mutex_lock(&tokenMut);
        //                 tokenReady = 1;
        //                 pthread_cond_signal(&tokenCond);
        //                 pthread_mutex_unlock(&tokenMut);
        //                 role = -1;
        //                 changeState(WAIT);
        //             break;    
        //         }
        //     }
        // }
        
        // if (role == 0) {
        //     debug("Czekam na pojedynek");
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, DUEL, MPI_COMM_WORLD, &status);
        //     updateLamportClock(pakiet.ts);
        //     handleDuel(pakiet.pair, pakiet.win);
        // }

        // while (stan == INWANT) {
        //     debug("jestem w inwant");
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        //     updateLamportClock(pakiet.ts);

        //     switch (status.MPI_TAG) {
        //     case REQ:
        //         debug("Otrzymałem REQ od %d", status.MPI_SOURCE);

        //         // Zapisanie otrzymanego REQ do tablicy
        //         int found = 0;
        //         for (int i = 0; i < receivedPacketsCount; i++) {
        //             if (receivedPacketsLog[i].source == status.MPI_SOURCE) {
        //                 receivedPacketsLog[i].lamportClock = pakiet.ts;
        //                 found = 1;
        //                 break;
        //             }
        //         }
        //         if (!found) {
        //             receivedPacketsLog[receivedPacketsCount].source = status.MPI_SOURCE;
        //             receivedPacketsLog[receivedPacketsCount].lamportClock = pakiet.ts;
        //             receivedPacketsCount++;
        //             printReceivedPacketsLog();
        //         }
        //         handleRequest(pakiet.ts, pakiet.src);

        //         // Sprawdzenie, czy wcześniej wysłano REQ do tego procesu
        //         for (int i = 0; i < sentPacketsCount; i++) {
        //             if (sentPacketsLog[i].destination == pakiet.src) {
        //                 if (sentPacketsLog[i].lamportClock < pakiet.ts) {
        //                     // Sprawdzenie, czy nie otrzymano już ACK od tego procesu
        //                     int alreadyReceivedACK = 0;
        //                     for (int j = 0; j < ackLogCount; j++) {
        //                         if (ackLog[j] == pakiet.src) {
        //                             alreadyReceivedACK = 1;
        //                             //handle = 0;
        //                             break;
        //                         }
        //                     }

        //                     // Dodanie ACK, jeśli jeszcze go nie otrzymano
        //                     if (!alreadyReceivedACK) {
        //                         pthread_mutex_lock(&ackCountMut);
        //                         handleAck(pakiet.src);
        //                         // ackCount++;
        //                         // ackLog[ackLogCount++] = pakiet.src; // Zapis procesu, od którego dodano ACK
        //                         // debug("Dodano ACK od procesu %d (ackCount=%d)", pakiet.src, ackCount);
        //                         // if (ackCount >= size/2 - pistols) {
        //                         //     changeState(INSECTION);
        //                         // }
        //                         pthread_cond_signal(&ackCond);
        //                         pthread_mutex_unlock(&ackCountMut);
        //                         //handleRequest(pakiet.ts, pakiet.src);                               
        //                     }
        //                 }
        //                 break;
        //             }
        //         }
        //         // if (handle){
        //         //     handleRequest(pakiet.ts, pakiet.src);
        //         // }
        //         handleRequest(pakiet.ts, pakiet.src);
        //         if (ackCount >= size/2 - pistols) {
        //             changeState(INSECTION);
        //         }
        //         //handleRequest(pakiet.ts, pakiet.src);
        //         break;
        //     case ACK:
        //         debug("Otrzymałem ACK od %d", status.MPI_SOURCE);
        //         handleAck(pakiet.src);
        //         pthread_mutex_lock(&ackCountMut);
        //         if (ackCount >= size/2 - pistols) {
        //             changeState(INSECTION);
        //         }
        //         pthread_mutex_unlock(&ackCountMut);
        //         break;
        //     case RELEASE:
        //         releaseAccess();
        //         changeState(REST);
        //         break;
        //     }
        // }

        // if (role == 1 && stan != INWANT && stan != INSECTION && stan != REST) {
        //     debug("jestem w nieinwant");
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        //     updateLamportClock(pakiet.ts);
        //     switch (status.MPI_TAG) {
        //     case REQ:
        //         debug("Otrzymałem REQ od %d", status.MPI_SOURCE);

        //         // Zapisanie otrzymanego REQ do tablicy
        //         int found = 0;
        //         for (int i = 0; i < receivedPacketsCount; i++) {
        //             if (receivedPacketsLog[i].source == status.MPI_SOURCE) {
        //                 receivedPacketsLog[i].lamportClock = pakiet.ts;
        //                 found = 1;
        //                 break;
        //             }
        //         }
        //         if (!found) {
        //             receivedPacketsLog[receivedPacketsCount].source = status.MPI_SOURCE;
        //             receivedPacketsLog[receivedPacketsCount].lamportClock = pakiet.ts;
        //             receivedPacketsCount++;
        //         }

        //         //updateLamportClock(pakiet.ts);
        //         //packet_t ack = {lamportClock, rank};
        //         sendPacket(0, pakiet.src, ACK);
        //         break;
        //     case ACK:
        //         debug("Otrzymałem ACK od %d", status.MPI_SOURCE);
        //         handleAck(pakiet.src);
        //         break;
        //     default:
        //         break;
        //     }
        // }

        // while (stan == INSECTION) {
        //     if (release) {
        //         debug("jestem w insection");
        //         releaseAccess();
        //         changeState(REST);
        //     }
            
        //     // debug("jestem w insection");
        //     // MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, END, MPI_COMM_WORLD, &status);
        //     // releaseAccess();
        //     // changeState(REST);
        // }
        
        
    // if (stan == REST) {
    //     debug("jestem w rest i czekam na synchro");
    //     while (!complete) {
    //         pthread_cond_wait(&endCond, &endMut);
    //     }
    // }
}

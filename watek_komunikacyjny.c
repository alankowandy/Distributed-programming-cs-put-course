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
        while (stan == REST || stan == PAIRING) {
            if (rank != 0) {
                MPI_Recv( &pakiet, 1, MPI_PAKIET_T, rank - 1, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                updateLamportClock(pakiet.ts);

                switch (status.MPI_TAG) {
                    case INITIAL_TOKEN:
                        debug("Odebrałem token od procesu %d", rank - 1);
                        // Dodanie swojej wartości do tokenu
                        pakiet.token[rank] = localValue;
                        debug("Dodałem swoją wartość %d do tokenu", localValue);
                        // Przekazanie tokenu do następnego procesu
                        //incrementLamportClock();
                        pakiet.ts = lamportClock;
                        pakiet.src = rank;
                        MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, INITIAL_TOKEN, MPI_COMM_WORLD);
                        debug("Wysłałem token do procesu %d", nextProcess);
                    break;
                    case FINAL_TOKEN:
                        debug("Odebrałem wypełniony token od procesu %d", rank - 1);
                        // Przekazanie wypełnionego tokenu do następnego procesu
                        //incrementLamportClock();
                        pakiet.ts = lamportClock;
                        pakiet.src = rank;
                        if (rank != size - 1) {
                            MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, FINAL_TOKEN, MPI_COMM_WORLD);
                            debug("Wysłałem wypełniony token do procesu %d", nextProcess);
                        }
                        memcpy(token, pakiet.token, MAX_SIZE * sizeof(int));
                        tokenReady = 1;
                        role = -1;
                        changeState(WAIT);
                    break;    
                }
            }
        }
        //debug("wychodzę z pairing %d", role);
        // while (role == -1) {
        //     sleepThread(1);
        // }
        // debug("wychodzę z role");
        
        if (role == 0) {
            debug("jestem w wait");
            MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, DUEL, MPI_COMM_WORLD, &status);
            updateLamportClock(pakiet.ts);
            handleDuel(pakiet.pair, pakiet.win);
        }

        while (stan == INWANT) {
            debug("jestem w inwant");
            MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            switch (status.MPI_TAG) {
            case REQ:
                debug("Otrzymałem REQ od %d", status.MPI_SOURCE);
                // Sprawdzenie, czy wcześniej wysłano REQ do tego procesu
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
                                pthread_mutex_lock(&ackCountMut);
                                ackCount++;
                                pthread_mutex_unlock(&ackCountMut);
                                ackLog[ackLogCount++] = pakiet.src; // Zapis procesu, od którego dodano ACK
                                debug("Dodano ACK od procesu %d (ackCount=%d)", pakiet.src, ackCount);
                            }
                        }
                        break;
                    }
                }
                handleRequest(pakiet.ts, pakiet.src);
                break;
            case ACK:
                debug("Otrzymałem ACK od %d", status.MPI_SOURCE);
                handleAck(pakiet.src);
                if (ackCount >= size - pistols) {
                    changeState(INSECTION);
                }
                break;
            }
        }

        if (role == 1 && stan != INWANT) {
            debug("jestem w nieinwant");
            MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            switch (status.MPI_TAG) {
            case REQ:
                updateLamportClock(pakiet.ts);
                packet_t ack = {lamportClock, rank};
                sendPacket(&ack, pakiet.src, ACK);
                break;
            case ACK:
                debug("Otrzymałem ACK od %d", status.MPI_SOURCE);
                handleAck(pakiet.src);
                break;
            default:
                break;
            }
        }

        while (stan == INSECTION) {
            MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, RELEASE, MPI_COMM_WORLD, &status);
            releaseAccess();
            changeState(FINISHED);
        }
        
        
        
        // switch (stan) {
        // case INWANT:
        //     debug("wchodzę w odbiór INWANT");
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        //     switch (status.MPI_TAG) {
        //     case REQ:
        //         // Sprawdzenie, czy wcześniej wysłano REQ do tego procesu
        //         for (int i = 0; i < sentPacketsCount; i++) {
        //             if (sentPacketsLog[i].destination == pakiet.src) {
        //                 if (sentPacketsLog[i].lamportClock < pakiet.ts) {
        //                     // Sprawdzenie, czy nie otrzymano już ACK od tego procesu
        //                     int alreadyReceivedACK = 0;
        //                     for (int j = 0; j < ackLogCount; j++) {
        //                         if (ackLog[j] == pakiet.src) {
        //                             alreadyReceivedACK = 1;
        //                             break;
        //                         }
        //                     }

        //                     // Dodanie ACK, jeśli jeszcze go nie otrzymano
        //                     if (!alreadyReceivedACK) {
        //                         pthread_mutex_lock(&ackCountMut);
        //                         ackCount++;
        //                         pthread_mutex_unlock(&ackCountMut);
        //                         ackLog[ackLogCount++] = pakiet.src; // Zapis procesu, od którego dodano ACK
        //                         debug("Dodano ACK od procesu %d (ackCount=%d)", pakiet.src, ackCount);
        //                     }
        //                 }
        //                 break;
        //             }
        //         }
        //         handleRequest(pakiet.ts, pakiet.src);
        //         break;
        //     case ACK:
        //         handleAck(pakiet.src);
        //         break;
        //     }
        //     break;
        // case WAIT:
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, DUEL, MPI_COMM_WORLD, &status);
        //     updateLamportClock(pakiet.ts);
        //     handleDuel(pakiet.pair, pakiet.win);
        //     break;
        // default:
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, REQ, MPI_COMM_WORLD, &status);
        //     updateLamportClock(pakiet.ts);
        //     packet_t ack = {lamportClock, rank};
        //     sendPacket(&ack, pakiet.src, ACK);
        //     break;
        // }

        // if (stan != INWANT) {
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, REQ, MPI_COMM_WORLD, &status);

        //     incrementLamportClock();
        //     pakiet.ts = lamportClock;
        //     pakiet.src = rank;
        //     MPI_Send(&pakiet, 1, MPI_PAKIET_T, status.MPI_SOURCE, ACK, MPI_COMM_WORLD);
        // }
        

        // if (stan == WAIT) {
        //     for (int i = 0; i < killerCount; i++) {
        //         if (killers[i] == rank) { // Proces jest zabójcą
        //             int nextKiller = killers[(i + 1) % killerCount]; // Następny zabójca
        //             int prevKiller = killers[(i - 1 + killerCount) % killerCount]; // Poprzedni zabójca
        //             // First Action
        //             if (i % 2 == 0) {
        //                 // Następnie odbiera wiadomość
        //                 MPI_Recv(&pakiet, 1, MPI_PAKIET_T, prevKiller, FIRST_ACTION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        //                 debug("Zabójca %d (rank %d) odebrał wiadomość od %d (First Action)", i, rank, prevKiller);
        //             } else {
        //                 // Zabójca z nieparzystym indeksem najpierw odbiera wiadomość
        //                 MPI_Recv(&pakiet, 1, MPI_PAKIET_T, prevKiller, FIRST_ACTION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        //                 debug("Zabójca %d (rank %d) odebrał wiadomość od %d (First Action)", i, rank, prevKiller);
        //             }

        //             // Second Action
        //             if (i % 2 != 0) {
        //                 // Następnie odbiera wiadomość
        //                 MPI_Recv(&pakiet, 1, MPI_PAKIET_T, nextKiller, SECOND_ACTION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        //                 debug("Zabójca %d (rank %d) odebrał wiadomość od %d (Second Action)", i, rank, nextKiller);
        //             } else {
        //                 // Zabójca z parzystym indeksem najpierw odbiera wiadomość
        //                 MPI_Recv(&pakiet, 1, MPI_PAKIET_T, nextKiller, SECOND_ACTION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        //                 debug("Zabójca %d (rank %d) odebrał wiadomość od %d (Second Action)", i, rank, nextKiller);

        //                 // Następnie wysyła wiadomość
        //                 MPI_Send(&pakiet, 1, MPI_PAKIET_T, prevKiller, SECOND_ACTION, MPI_COMM_WORLD);
        //                 debug("Zabójca %d (rank %d) wysłał wiadomość do %d (Second Action)", i, rank, prevKiller);
        //             }
        //         }
        //     }
        //     debug("czekam na recv");
        //     MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        //     enqueueMessage(&pakiet);
        //     debug("Dodano wiadomość do kolejki od %d z tagiem %d", status.MPI_SOURCE, status.MPI_TAG);
        // }

        // // Przetwarzanie wiadomości z kolejki
        // while (dequeueMessage(&pakiet)) {
        //     switch (status.MPI_TAG) {
        //         case REQ:
        //             debug("napierdalam handleRequest");
        //             handleRequest(pakiet.ts, status.MPI_SOURCE);
        //             break;
        //         case ACK:
        //             debug("Odebrałem ACK od %d", pakiet.src);
        //             handleAck();
        //             break;
        //         case DUEL:
        //             handleDuel(pakiet.pair, pakiet.win);
        //             break;
        //     }
        // }
        
        // while (stan == WAIT || stan == INSECTION) {
        //     //debug("wchodzę do pętli WAIT");
        //     MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        //     updateLamportClock(pakiet.ts);

        //     switch (status.MPI_TAG) {
        //         case REQ:
        //             handleRequest(pakiet.ts, status.MPI_SOURCE);
        //             break;
        //         case ACK:
        //             handleAck();
        //             break;
        //         case DUEL:
        //             handleDuel(pakiet.pair, pakiet.win);
        //             break;
        //     }
        // }
    }
}

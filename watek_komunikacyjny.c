#include "main.h"
#include "watek_komunikacyjny.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
    MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
    int nextProcess = (rank + 1) % size;
    
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
                        incrementLamportClock();
                        pakiet.ts = lamportClock;
                        pakiet.src = rank;
                        MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, INITIAL_TOKEN, MPI_COMM_WORLD);
                        debug("Wysłałem token do procesu %d", nextProcess);
                    break;
                    case FINAL_TOKEN:
                        debug("Odebrałem wypełniony token od procesu %d", rank - 1);
                        // Przekazanie wypełnionego tokenu do następnego procesu
                        incrementLamportClock();
                        pakiet.ts = lamportClock;
                        pakiet.src = rank;
                        if (rank != size - 1) {
                            MPI_Send(&pakiet, 1, MPI_PAKIET_T, nextProcess, FINAL_TOKEN, MPI_COMM_WORLD);
                            debug("Wysłałem wypełniony token do procesu %d", nextProcess);
                        }
                        memcpy(token, pakiet.token, MAX_SIZE * sizeof(int));
                        tokenReady = 1;
                    break;    
                }
            }
        }
        
        while (stan == WAIT || stan == INSECTION) {
            MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            updateLamportClock(pakiet.ts);

            switch (status.MPI_TAG) {
                case REQ:
                    handleRequest(pakiet.ts, status.MPI_SOURCE);
                    break;
                case ACK:
                    handleAck();
                    break;
                case DUEL:
                    handleDuel(pakiet.pair, pakiet.win);
                    break;
            }
        }
    }
}

#include "main.h"
#include "watek_komunikacyjny.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
    MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
    /* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( stan!=FINISHED ) {
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        updateLamportClock(pakiet.ts);

        switch (status.MPI_TAG) {
            case REQ:
                handleRequest(pakiet.ts, status.MPI_SOURCE);
                break;
                // TO-DO - obsługa ACK i RELEASE osobno?
            case ACK:
                handleAck();
                break;
            case DUEL:
                handleDuel(pakiet.pair);
                break;
        }
    }
}

#include "main.h"
#include "watek_komunikacyjny.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
    MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
    /* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( stan!=InFinish ) {
	//debug("czekam na recv");
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        updateLamportClock(pakiet.ts);

        switch (status.MPI_TAG) {
            case WEAPON_REQ:
                handleRequest(&pakiet, status.MPI_SOURCE);
                break;
            case WEAPON_ACK:
                handleReply();
                break;
            case RELEASE:
                handleRelease(&pakiet);
                break;
            case PAIR_REQ:
                sendPacket(NULL, pakiet.src, PAIR_ACK);
                break;
            case DUEL:
                duel();
                //debug("Proces %d rozpoczyna pojedynek z %d", rank, pakiet.src);
                break;
        }
    }
}

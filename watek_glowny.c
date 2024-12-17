#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    srandom(rank);
    int tag;
    int role = random() % 2;  // losowanie roli (0 = ofiara, 1 = zabójca)
    int pair = -1;            // identyfikator pary

    // wysłanie żądania sparowania przez zabójcę
    if (role == 1) {
        for (int i = 0; i < size; i++) {
            if (i != rank) {
                sendPacket(NULL, i, PAIR_REQ);
            }
        }
        MPI_Status status;
        packet_t pkt;
        MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, PAIR_ACK, MPI_COMM_WORLD, &status);
        updateLamportClock(pkt.ts);
        pair = pkt.src;
        debug("Jestem zabójcą, sparowano mnie z procesem %d", pair);

        requestAccess();

        while (ackCount < size - 1) {
            sleep(1);
        }

        debug("Proces %d wchodzi do sekcji krytycznej", rank);
        sendPacket(NULL, pair, DUEL);

        releaseAccess();

    } else {  // odbiór żądania sparowania przez ofiarę
        MPI_Status status;
        packet_t pkt;
        MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, PAIR_REQ, MPI_COMM_WORLD, &status);
        updateLamportClock(pkt.ts);
        if (pair == -1) {
            pair = pkt.src;
            sendPacket(NULL, pair, PAIR_ACK);
            debug("Jestem ofiarą, sparowano mnie z procesem %d", pair);
        }
    }
    
    while (stan != InFinish) {
        int perc = random()%100; 

        if (perc<STATE_CHANGE_PROB) {
            if (stan==InRun) {
		debug("Zmieniam stan na wysyłanie");
		changeState( InSend );
		packet_t *pkt = malloc(sizeof(packet_t));
		pkt->data = perc;
		perc = random()%100;
		tag = ( perc < 25 ) ? FINISH : APP_PKT;
		debug("Perc: %d", perc);
		
		sendPacket( pkt, (rank+1)%size, tag);
		changeState( InRun );
		debug("Skończyłem wysyłać");
            } else {
            }
        }
        sleep(SEC_IN_STATE);
    }
}

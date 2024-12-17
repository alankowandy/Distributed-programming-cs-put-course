#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    changeState(InRun);

    packet_t rolePair = assignRoleAndPair(); // Przypisanie roli i parowanie
    int role = rolePair.role;
    int pair = rolePair.pair;

    if (role == 1) { // Zabójca
        changeState(InRun);
        requestAccess();

        while (ackCount < size - 1) {
            sleep(SEC_IN_STATE);
        }

        changeState(InFight);
        debug("Proces %d atakuje proces %d", rank, pair);
        sendPacket(NULL, pair, DUEL);

        releaseAccess();
        changeState(InFinish);

    } else { // Ofiara
        MPI_Status status;
        packet_t pkt;
        changeState(InPairing);
        MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, DUEL, MPI_COMM_WORLD, &status);
        debug("Proces %d został zaatakowany przez proces %d", rank, pkt.src);
        changeState(InFinish);
    }
    
    while (stan != InFinish) {
        // int perc = random()%100; 

        // if (perc<STATE_CHANGE_PROB) {
        //     if (stan==InRun) {
		// debug("Zmieniam stan na wysyłanie");
		// changeState( InSend );
		// packet_t *pkt = malloc(sizeof(packet_t));
		// pkt->data = perc;
		// perc = random()%100;
		// tag = ( perc < 25 ) ? FINISH : APP_PKT;
		// debug("Perc: %d", perc);
		
		// sendPacket( pkt, (rank+1)%size, tag);
		// changeState( InRun );
		// debug("Skończyłem wysyłać");
        //     } else {
        //     }
        // }
        sleep(SEC_IN_STATE);
    }
}

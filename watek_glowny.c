#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    changeState(REST);

    packet_t rolePair = assignRoleAndPair(); // Przypisanie roli i parowanie
    int role = rolePair.role;
    int pair = rolePair.pair;

    if (role == 1) { // Zabójca
        requestAccess();

        while (ackCount < size - pistols) {
            sleep(SEC_IN_STATE); // Proces czeka dopoki nie uzyska odpowiedniej ilosci ACK
            // MPI_Status status;
            // packet_t pkt;
            // MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            // switch (status.MPI_TAG) {
            //     case REQ:
            //         handleRequest(pkt.ts, status.MPI_SOURCE);
            //         break;
            //     case ACK:
            //         handleAck();
            //         break;
            // }
        }

        changeState(INSECTION);
        debug("Atakuje proces %d", pair);
        //sendPacket(NULL, pair, DUEL);
        duel(pair);

        releaseAccess();
        changeState(FINISHED);

    } else { // Ofiara
        changeState(WAIT);
        // while (stan == WAIT) {
        //     MPI_Status status;
        //     packet_t pkt;
        //     MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        //     // switch (status.MPI_TAG) {
        //     //     case REQ:
        //     //         handleRequest(pkt.ts, status.MPI_SOURCE);
        //     //         break;
        //     //     case DUEL:
        //     //         handleDuel(pair);
        //     //         break;
        //     // }
        // }
    }
    
    while (stan != FINISHED) {
        sleep(SEC_IN_STATE);
    }
}

#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    changeState(InRun);

    packet_t rolePair = assignRoleAndPair(); // Przypisanie roli i parowanie
    int role = rolePair.role;
    int pair = rolePair.pair;

    if (role == 1) { // Zabójca
        requestAccess();

        while (ackCount < size - pistols) {
        MPI_Status status;
        packet_t pkt;
        MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        switch (status.MPI_TAG) {
            case REQ:
                handleRequest(pkt.ts, status.MPI_SOURCE);
                break;
            case ACK:
                handleAck();
                break;
        }
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
        sleep(SEC_IN_STATE);
    }
}

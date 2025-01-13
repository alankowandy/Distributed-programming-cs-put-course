#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    changeState(REST);

    packet_t rolePair = assignRoleAndPair(); // Przypisanie roli i parowanie
    int role = rolePair.role;
    int pair = rolePair.pair;

    changeState(WAIT);

    MPI_Barrier(MPI_COMM_WORLD);

    if (role == 1) { // Zabójca
        requestAccess();

        while (ackCount < size - pistols) {
            sleepThread(1000); // Proces czeka dopoki nie uzyska odpowiedniej ilosci ACK
        }

        changeState(INSECTION);
        debug("Wchodzę do sekcji krytycznej");
        duel(pair);

        releaseAccess();
        //changeState(FINISHED);

    } else { // Ofiara
        //changeState(WAIT);
        //debug("wchodzę tu gdzie ofiara powinna");
    }

    MPI_Barrier(MPI_COMM_WORLD);

    changeState(FINISHED);
    
    // while (stan != FINISHED) {
    //     sleepThread(1000);
    //     //sleep(SEC_IN_STATE);
    // }
}

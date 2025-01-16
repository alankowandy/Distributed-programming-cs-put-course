#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    changeState(REST);

    packet_t rolePair = assignRoleAndPair(); // Przypisanie roli i parowanie
    role = rolePair.role;
    int pair = rolePair.pair;

    //changeState(WAIT);

    //MPI_Barrier(MPI_COMM_WORLD);

    if (role == 1) { // Zabójca
        requestAccess();

        pthread_mutex_lock(&ackCountMut);
        while (ackCount < size - pistols) {
            pthread_cond_wait(&ackCond, &ackCountMut);
            //sleepThread(1000); // Proces czeka dopoki nie uzyska odpowiedniej ilosci ACK
            //requestAccess();
        }
        pthread_mutex_unlock(&ackCountMut);

        //changeState(INSECTION);
        debug("Wchodzę do sekcji krytycznej");
        sleepThread(5000);
        duel(pair);

        //releaseAccess();
        //changeState(FINISHED);

    } else { // Ofiara
        while (stan != FINISHED)
        {
            sleepThread(500);
        }
        
        //changeState(WAIT);
        //debug("wchodzę tu gdzie ofiara powinna");
    }

    MPI_Barrier(MPI_COMM_WORLD);
    
    // while (stan != FINISHED) {
    //     sleepThread(1000);
    //     //sleep(SEC_IN_STATE);
    // }
}

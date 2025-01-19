#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    packet_t rolePair = assignRoleAndPair(); // Przypisanie roli i parowanie
    role = rolePair.role;
    int pair = rolePair.pair;

    MPI_Barrier(MPI_COMM_WORLD);

    if (role == 1) { // Zabójca
        requestAccess();

        pthread_mutex_lock(&ackCountMut);
        while (ackCount < size / 2 - pistols) {
            pthread_cond_wait(&ackCond, &ackCountMut);
        }
        pthread_mutex_unlock(&ackCountMut);

        println("Wchodzę do sekcji krytycznej");
        //sleepThread(2000);
        duel(pair);
        releaseAccess();

    } else { // Ofiara
        while (stan == WAIT)
        {
            sleepThread(500);
        }
    }
}

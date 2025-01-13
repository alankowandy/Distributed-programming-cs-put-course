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
        }

        changeState(INSECTION);
        debug("Atakuje proces %d", pair);
        //sendPacket(NULL, pair, DUEL);
        duel(pair);

        releaseAccess();
        changeState(FINISHED);

    } else { // Ofiara
        changeState(WAIT);
        handleDuel(pair);
        changeState(FINISHED);
    }
    
    while (stan != FINISHED) {
        sleep(SEC_IN_STATE);
    }
}

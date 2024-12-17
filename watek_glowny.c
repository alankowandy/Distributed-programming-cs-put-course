#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    changeState(InRun);
    srandom(time(NULL) + rank); // Unikalne ziarno generatora
    int localValue = random() % 1000; // Wylosowana wartość
    int values[size]; // Tablica do przechowywania wartości od wszystkich procesów
    values[rank] = localValue; // Zapis własnej wartości

    // Wysłanie własnej wartości do wszystkich innych procesów
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            MPI_Send(&localValue, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }

    // Odbieranie wartości od innych procesów
    for (int i = 0; i < size - 1; i++) { // size - 1, bo własna wartość już jest zapisana
        MPI_Status status;
        int receivedValue;
        MPI_Recv(&receivedValue, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        values[status.MPI_SOURCE] = receivedValue;
    }

    // Sortowanie wartości lokalnie
    int sortedRanks[size];
    for (int i = 0; i < size; i++) sortedRanks[i] = i; // Inicjalizacja tablicy ranków

    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (values[sortedRanks[i]] > values[sortedRanks[j]]) {
                int temp = sortedRanks[i];
                sortedRanks[i] = sortedRanks[j];
                sortedRanks[j] = temp;
            }
        }
    }

    // Określenie roli: pierwsza połowa to zabójcy, druga połowa to ofiary
    int role = -1; // 1 = zabójca, 0 = ofiara
    for (int i = 0; i < size / 2; i++) {
        if (sortedRanks[i] == rank) {
            role = 1; // Zabójca
            break;
        }
    }
    if (role == -1) role = 0; // Ofiara, jeśli nie przypisano roli

    debug("Proces %d wylosował %d i jest %s", rank, localValue, (role == 1) ? "zabójcą" : "ofiarą");

    int pair = -1; // identyfikator pary

    // wysłanie żądania sparowania przez zabójcę
    if (role == 1) {
        changeState(InPairing);
        for (int i = 0; i < size; i++) {
            if (i != rank) {
                sendPacket(NULL, i, PAIR_REQ);
            }
        }
        MPI_Status status;
        packet_t pkt;
        MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, PAIR_ACK, MPI_COMM_WORLD, &status);
        pair = pkt.src;
        updateLamportClock(pkt.ts);
        debug("Jestem zabójcą, sparowano mnie z procesem %d", pair);
        changeState(InRun);

        requestAccess();

        while (ackCount < size - 1) {
            sleep(SEC_IN_STATE);
        }

        changeState(InFight);
        debug("Proces %d wchodzi do sekcji krytycznej", rank);
        sendPacket(NULL, pair, DUEL);

        releaseAccess();
        changeState(InFinish);

    } else {  // odbiór żądania sparowania przez ofiarę
        MPI_Status status;
        packet_t pkt;
        changeState(InPairing);
        MPI_Recv(&pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, PAIR_REQ, MPI_COMM_WORLD, &status);
        updateLamportClock(pkt.ts);
        if (pair == -1) {
            pair = pkt.src;
            sendPacket(NULL, pair, PAIR_ACK);
            debug("Jestem ofiarą, sparowano mnie z procesem %d", pair);
            changeState(InFinish);
        }
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

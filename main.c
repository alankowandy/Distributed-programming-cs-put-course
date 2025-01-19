#include "main.h"
#include "watek_glowny.h"
#include "watek_komunikacyjny.h"

int rank, size, localValue, role;
int lamportClock = 0;
int ackCount = 0;

int cycles = 2;
int pistols = 2;

int cycle = 0;

int tokenReady = 0; // Gotowość tokenu

int complete = 0;

int wins = 0;

int pairingReady = 0;

state_t stan=REST;
pthread_t threadKom, threadMon;

pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lamportMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t tokenMut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tokenCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t pairingMut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pairingCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t ackCountMut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ackCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t endMut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t endCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t reqLogMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t sentLogMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t waitQueueMut = PTHREAD_MUTEX_INITIALIZER;

void finalizuj()
{
    pthread_mutex_destroy( &stateMut);
    /* Czekamy, aż wątek potomny się zakończy */
    println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(threadKom,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}

void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: chcemy %d. Co otrzymamy?\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	    break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n"); /* tego chcemy. Wszystkie inne powodują problemy */
	    break;
        default: printf("Nikt nic nie wie\n");
    }
}


int main(int argc, char **argv)
{
    MPI_Status status;
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);
    srand(time(NULL) + rank); // inicjalizacja generatora liczb losowych
    inicjuj_typ_pakietu(); // tworzy typ pakietu
    packet_t pkt;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    pthread_create( &threadKom, NULL, startKomWatek , 0);

    // implementacja pętli, która będzie wykonywała się przez określoną liczbę cykli
    for (int i = 0; i < cycles; i++) {
        cycle = i;
        complete = 0;
        println("Cykl %d z %d", i + 1, cycles);
        mainLoop();
        //debug("przed pierwszą barierą w main");
        MPI_Barrier(MPI_COMM_WORLD);
        //debug("za pierwszą barierą w main");
        // if (i != cycles - 1) {
        //     pthread_mutex_lock(&endMut);
        //     complete = 1;
        //     pthread_cond_signal(&endCond);
        //     pthread_mutex_unlock(&endMut);
        // }
        debug("Zaraz nowy cykl!!!!!!!!!!!!!!!");
        //MPI_Barrier(MPI_COMM_WORLD);
        resetVariables();
        //debug("Zaraz nowy cykl!!!!!!!!!!!!!!!");
    }

    //sleepThread(5500);
    debug("kończę");
    sendPacket(0, rank, RELEASE);
    // pthread_mutex_lock(&endMut);
    // changeState(FINISHED);
    // complete = 1;
    // pthread_cond_signal(&endCond);
    // pthread_mutex_unlock(&endMut);

    debug("Zakończyłem z wynikiem: %d wygrane\n", wins);

    finalizuj();
    return 0;
}


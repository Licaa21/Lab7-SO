/*// Solutie pentru Linux - Fire Albe si Negre cu protectie anti-starvation
// Compilare: g++ -pthread -o linux_solution linux_solution.cpp

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <queue>

// Structura pentru a gestiona accesul la resursa
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int white_count;      // Numar de fire albe care folosesc resursa
    int black_count;      // Numar de fire negre care folosesc resursa
    int white_waiting;    // Numar de fire albe in asteptare
    int black_waiting;    // Numar de fire negre in asteptare

    // Pentru evitarea starvation - sistem de ture
    int current_turn;     // 0 = alb, 1 = negru, -1 = liber
    int turn_count;       // Cate fire au folosit resursa in tura curenta
    int max_per_turn;     // Numar maxim de fire per tura (pentru fairness)
} ResourceManager;

ResourceManager rm;

void init_resource_manager(ResourceManager* rm, int max_per_turn) {
    pthread_mutex_init(&rm->mutex, NULL);
    pthread_cond_init(&rm->cond, NULL);
    rm->white_count = 0;
    rm->black_count = 0;
    rm->white_waiting = 0;
    rm->black_waiting = 0;
    rm->current_turn = -1;  // Initial liber
    rm->turn_count = 0;
    rm->max_per_turn = max_per_turn;
}

void destroy_resource_manager(ResourceManager* rm) {
    pthread_mutex_destroy(&rm->mutex);
    pthread_cond_destroy(&rm->cond);
}

// Functie pentru a verifica daca un fir poate accesa resursa
int can_access(ResourceManager* rm, int is_white) {
    int my_type = is_white ? 0 : 1;
    int other_count = is_white ? rm->black_count : rm->white_count;
    int other_waiting = is_white ? rm->black_waiting : rm->white_waiting;

    // Daca tipul opus foloseste resursa, nu putem intra
    if (other_count > 0) {
        return 0;
    }

    // Daca resursa e libera (nimeni nu o foloseste)
    if (rm->white_count == 0 && rm->black_count == 0) {
        // Daca nu e tura nimanui sau e tura noastra, putem intra
        if (rm->current_turn == -1 || rm->current_turn == my_type) {
            return 1;
        }
        // Daca e tura celuilalt tip dar nu asteapta nimeni de acel tip
        if (other_waiting == 0) {
            return 1;
        }
        return 0;
    }

    // Resursa e folosita de tipul nostru
    // Verificam daca nu am depasit limita per tura (anti-starvation)
    if (rm->turn_count >= rm->max_per_turn && other_waiting > 0) {
        return 0;  // Trebuie sa dam voie celuilalt tip
    }

    return 1;
}

void acquire_resource(ResourceManager* rm, int is_white, int thread_id) {
    pthread_mutex_lock(&rm->mutex);

    // Incrementam contorul de asteptare
    if (is_white) {
        rm->white_waiting++;
    }
    else {
        rm->black_waiting++;
    }

    // Asteptam pana putem accesa resursa
    while (!can_access(rm, is_white)) {
        pthread_cond_wait(&rm->cond, &rm->mutex);
    }

    // Decrementam contorul de asteptare
    if (is_white) {
        rm->white_waiting--;
        rm->white_count++;
    }
    else {
        rm->black_waiting--;
        rm->black_count++;
    }

    // Actualizam tura
    int my_type = is_white ? 0 : 1;
    if (rm->current_turn != my_type) {
        rm->current_turn = my_type;
        rm->turn_count = 1;
    }
    else {
        rm->turn_count++;
    }

    printf("[%s %d] A obtinut accesul la resursa (albe: %d, negre: %d)\n",
        is_white ? "ALB" : "NEGRU", thread_id, rm->white_count, rm->black_count);

    pthread_mutex_unlock(&rm->mutex);
}

void release_resource(ResourceManager* rm, int is_white, int thread_id) {
    pthread_mutex_lock(&rm->mutex);

    if (is_white) {
        rm->white_count--;
    }
    else {
        rm->black_count--;
    }

    printf("[%s %d] A eliberat resursa (albe: %d, negre: %d)\n",
        is_white ? "ALB" : "NEGRU", thread_id, rm->white_count, rm->black_count);

    // Daca nu mai e nimeni de tipul nostru, resetam tura
    int my_count = is_white ? rm->white_count : rm->black_count;
    if (my_count == 0) {
        int other_waiting = is_white ? rm->black_waiting : rm->white_waiting;
        if (other_waiting > 0) {
            // Dam tura celuilalt tip
            rm->current_turn = is_white ? 1 : 0;
            rm->turn_count = 0;
        }
        else {
            rm->current_turn = -1;  // Liber
        }
    }

    // Notificam toate firele care asteapta
    pthread_cond_broadcast(&rm->cond);

    pthread_mutex_unlock(&rm->mutex);
}

// Structura pentru argumentele firelor
typedef struct {
    int thread_id;
    int is_white;
} ThreadArgs;

void* thread_function(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int id = args->thread_id;
    int is_white = args->is_white;

    printf("[%s %d] Pornit, incearca sa obtina resursa...\n",
        is_white ? "ALB" : "NEGRU", id);

    // Incearca sa obtina resursa
    acquire_resource(&rm, is_white, id);

    // Foloseste resursa (simulare)
    printf("[%s %d] Foloseste resursa...\n", is_white ? "ALB" : "NEGRU", id);
    usleep((rand() % 500 + 100) * 1000);  // 100-600ms

    // Elibereaza resursa
    release_resource(&rm, is_white, id);

    free(args);
    return NULL;
}

int main() {
    srand(time(NULL));

    // Initializam managerul de resurse (max 3 fire per tura pentru fairness)
    init_resource_manager(&rm, 3);

    const int NUM_WHITE = 5;
    const int NUM_BLACK = 5;
    pthread_t threads[NUM_WHITE + NUM_BLACK];

    printf("=== Pornire test cu %d fire albe si %d fire negre ===\n\n", NUM_WHITE, NUM_BLACK);

    // Cream fire in ordine mixta pentru a testa fairness
    int thread_idx = 0;
    for (int i = 0; i < NUM_WHITE || i < NUM_BLACK; i++) {
        if (i < NUM_WHITE) {
            ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
            args->thread_id = i + 1;
            args->is_white = 1;
            pthread_create(&threads[thread_idx++], NULL, thread_function, args);
            usleep(50000);  // Mic delay intre crearea firelor
        }
        if (i < NUM_BLACK) {
            ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
            args->thread_id = i + 1;
            args->is_white = 0;
            pthread_create(&threads[thread_idx++], NULL, thread_function, args);
            usleep(50000);
        }
    }

    // Asteptam toate firele
    for (int i = 0; i < NUM_WHITE + NUM_BLACK; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\n=== Toate firele au terminat ===\n");

    destroy_resource_manager(&rm);
    return 0;
}*/

// Solutie pentru Windows - Fire Albe si Negre cu protectie anti-starvation
// Compilare: cl /EHsc windows_solution.cpp
// sau cu MinGW: g++ -o windows_solution.exe windows_solution.cpp

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// Structura pentru a gestiona accesul la resursa
typedef struct {
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;

    int white_count;      // Numar de fire albe care folosesc resursa
    int black_count;      // Numar de fire negre care folosesc resursa
    int white_waiting;    // Numar de fire albe in asteptare
    int black_waiting;    // Numar de fire negre in asteptare

    // Pentru evitarea starvation - sistem de ture
    int current_turn;     // 0 = alb, 1 = negru, -1 = liber
    int turn_count;       // Cate fire au folosit resursa in tura curenta
    int max_per_turn;     // Numar maxim de fire per tura (pentru fairness)
} ResourceManager;

ResourceManager rm;

void init_resource_manager(ResourceManager* rm, int max_per_turn) {
    InitializeCriticalSection(&rm->cs);
    InitializeConditionVariable(&rm->cv);
    rm->white_count = 0;
    rm->black_count = 0;
    rm->white_waiting = 0;
    rm->black_waiting = 0;
    rm->current_turn = -1;  // Initial liber
    rm->turn_count = 0;
    rm->max_per_turn = max_per_turn;
}

void destroy_resource_manager(ResourceManager* rm) {
    DeleteCriticalSection(&rm->cs);
    // Condition variables nu necesita cleanup explicit pe Windows
}

// Functie pentru a verifica daca un fir poate accesa resursa
int can_access(ResourceManager* rm, int is_white) {
    int my_type = is_white ? 0 : 1;
    int other_count = is_white ? rm->black_count : rm->white_count;
    int other_waiting = is_white ? rm->black_waiting : rm->white_waiting;

    // Daca tipul opus foloseste resursa, nu putem intra
    if (other_count > 0) {
        return 0;
    }

    // Daca resursa e libera (nimeni nu o foloseste)
    if (rm->white_count == 0 && rm->black_count == 0) {
        // Daca nu e tura nimanui sau e tura noastra, putem intra
        if (rm->current_turn == -1 || rm->current_turn == my_type) {
            return 1;
        }
        // Daca e tura celuilalt tip dar nu asteapta nimeni de acel tip
        if (other_waiting == 0) {
            return 1;
        }
        return 0;
    }

    // Resursa e folosita de tipul nostru
    // Verificam daca nu am depasit limita per tura (anti-starvation)
    if (rm->turn_count >= rm->max_per_turn && other_waiting > 0) {
        return 0;  // Trebuie sa dam voie celuilalt tip
    }

    return 1;
}

void acquire_resource(ResourceManager* rm, int is_white, int thread_id) {
    EnterCriticalSection(&rm->cs);

    // Incrementam contorul de asteptare
    if (is_white) {
        rm->white_waiting++;
    }
    else {
        rm->black_waiting++;
    }

    // Asteptam pana putem accesa resursa
    while (!can_access(rm, is_white)) {
        SleepConditionVariableCS(&rm->cv, &rm->cs, INFINITE);
    }

    // Decrementam contorul de asteptare
    if (is_white) {
        rm->white_waiting--;
        rm->white_count++;
    }
    else {
        rm->black_waiting--;
        rm->black_count++;
    }

    // Actualizam tura
    int my_type = is_white ? 0 : 1;
    if (rm->current_turn != my_type) {
        rm->current_turn = my_type;
        rm->turn_count = 1;
    }
    else {
        rm->turn_count++;
    }

    printf("[%s %d] A obtinut accesul la resursa (albe: %d, negre: %d)\n",
        is_white ? "ALB" : "NEGRU", thread_id, rm->white_count, rm->black_count);

    LeaveCriticalSection(&rm->cs);
}

void release_resource(ResourceManager* rm, int is_white, int thread_id) {
    EnterCriticalSection(&rm->cs);

    if (is_white) {
        rm->white_count--;
    }
    else {
        rm->black_count--;
    }

    printf("[%s %d] A eliberat resursa (albe: %d, negre: %d)\n",
        is_white ? "ALB" : "NEGRU", thread_id, rm->white_count, rm->black_count);

    // Daca nu mai e nimeni de tipul nostru, resetam tura
    int my_count = is_white ? rm->white_count : rm->black_count;
    if (my_count == 0) {
        int other_waiting = is_white ? rm->black_waiting : rm->white_waiting;
        if (other_waiting > 0) {
            // Dam tura celuilalt tip
            rm->current_turn = is_white ? 1 : 0;
            rm->turn_count = 0;
        }
        else {
            rm->current_turn = -1;  // Liber
        }
    }

    // Notificam toate firele care asteapta
    WakeAllConditionVariable(&rm->cv);

    LeaveCriticalSection(&rm->cs);
}

// Structura pentru argumentele firelor
typedef struct {
    int thread_id;
    int is_white;
} ThreadArgs;

DWORD WINAPI thread_function(LPVOID arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int id = args->thread_id;
    int is_white = args->is_white;

    printf("[%s %d] Pornit, incearca sa obtina resursa...\n",
        is_white ? "ALB" : "NEGRU", id);

    // Incearca sa obtina resursa
    acquire_resource(&rm, is_white, id);

    // Foloseste resursa (simulare)
    printf("[%s %d] Foloseste resursa...\n", is_white ? "ALB" : "NEGRU", id);
    Sleep((rand() % 500) + 100);  // 100-600ms

    // Elibereaza resursa
    release_resource(&rm, is_white, id);

    free(args);
    return 0;
}

int main() {
    srand((unsigned int)GetTickCount());

    // Initializam managerul de resurse (max 3 fire per tura pentru fairness)
    init_resource_manager(&rm, 3);

    const int NUM_WHITE = 5;
    const int NUM_BLACK = 5;
    HANDLE threads[NUM_WHITE + NUM_BLACK];

    printf("=== Pornire test cu %d fire albe si %d fire negre ===\n\n", NUM_WHITE, NUM_BLACK);

    // Cream fire in ordine mixta pentru a testa fairness
    int thread_idx = 0;
    for (int i = 0; i < NUM_WHITE || i < NUM_BLACK; i++) {
        if (i < NUM_WHITE) {
            ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
            args->thread_id = i + 1;
            args->is_white = 1;
            threads[thread_idx++] = CreateThread(NULL, 0, thread_function, args, 0, NULL);
            Sleep(50);  // Mic delay intre crearea firelor
        }
        if (i < NUM_BLACK) {
            ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
            args->thread_id = i + 1;
            args->is_white = 0;
            threads[thread_idx++] = CreateThread(NULL, 0, thread_function, args, 0, NULL);
            Sleep(50);
        }
    }

    // Asteptam toate firele
    WaitForMultipleObjects(NUM_WHITE + NUM_BLACK, threads, TRUE, INFINITE);

    // Inchidem handle-urile
    for (int i = 0; i < NUM_WHITE + NUM_BLACK; i++) {
        CloseHandle(threads[i]);
    }

    printf("\n=== Toate firele au terminat ===\n");

    destroy_resource_manager(&rm);
    return 0;
}
/*// Solutie Refactorizata Linux - C++ Modern cu Pthreads

#include <pthread.h>
#include <unistd.h> // Pentru usleep
#include <iostream>
#include <vector>
#include <cstdlib>  // Pentru rand, srand

// Definim culorile explicit pentru lizibilitate
enum class ThreadColor {
    White,
    Black
};

// Clasa care gestioneaza sincronizarea si accesul echitabil
class FairResourceController {
private:
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int white_count = 0;      // Fire albe active
    int black_count = 0;      // Fire negre active
    int white_waiting = 0;    // Fire albe in asteptare
    int black_waiting = 0;    // Fire negre in asteptare

    // Variabile pentru controlul anti-starvation (echitate)
    ThreadColor current_turn_color = ThreadColor::White;
    bool has_turn = false;    // Indica daca sistemul de ture este activ
    int turn_counter = 0;     // Cate fire au trecut in tura curenta
    int max_per_turn;         // Limita maxima inainte de schimbarea fortata a turei

    // Logica interna: Verifica permisiunea de acces
    bool CanAccess(ThreadColor color) {
        bool is_white = (color == ThreadColor::White);
        int my_active = is_white ? white_count : black_count;
        int other_active = is_white ? black_count : white_count;
        int other_waiting = is_white ? black_waiting : white_waiting;

        // 1. Excluziune intre culori (daca opulul e inauntru, asteptam)
        if (other_active > 0) {
            return false;
        }

        // 2. Daca resursa e libera
        if (my_active == 0) {
            // Daca nu e randul nimanui sau e randul meu, intru
            if (!has_turn || current_turn_color == color) {
                return true;
            }
            // Daca e randul celuilalt, dar nu e nimeni acolo (asteptare 0), intru
            if (other_waiting == 0) {
                return true;
            }
            return false;
        }

        // 3. Resursa e folosita deja de culoarea mea
        // Anti-Starvation: Daca am depasit limita si celalalt asteapta, ma opresc
        if (turn_counter >= max_per_turn && other_waiting > 0) {
            return false; 
        }

        return true;
    }

public:
    FairResourceController(int max_threads_per_turn) : max_per_turn(max_threads_per_turn) {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
        has_turn = false;
    }

    ~FairResourceController() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    // Prevenim copierea obiectului (mutex-urile nu se copiaza)
    FairResourceController(const FairResourceController&) = delete;
    FairResourceController& operator=(const FairResourceController&) = delete;

    void RequestAccess(ThreadColor color, int thread_id) {
        pthread_mutex_lock(&mutex);

        bool is_white = (color == ThreadColor::White);

        // Anuntam intentia de a intra
        if (is_white) white_waiting++; else black_waiting++;

        // Loop de asteptare
        while (!CanAccess(color)) {
            pthread_cond_wait(&cond, &mutex);
        }

        // Am primit acces
        if (is_white) {
            white_waiting--;
            white_count++;
        } else {
            black_waiting--;
            black_count++;
        }

        // Gestionam tura
        if (!has_turn || current_turn_color != color) {
            current_turn_color = color;
            has_turn = true;
            turn_counter = 1;
        } else {
            turn_counter++;
        }

        printf("[%s %d] -> ACCESS (Activ: A=%d, N=%d)\n", 
            is_white ? "ALB" : "NEGRU", thread_id, white_count, black_count);

        pthread_mutex_unlock(&mutex);
    }

    void ReleaseAccess(ThreadColor color, int thread_id) {
        pthread_mutex_lock(&mutex);

        bool is_white = (color == ThreadColor::White);

        if (is_white) white_count--; else black_count--;

        printf("[%s %d] <- FREE   (Activ: A=%d, N=%d)\n", 
            is_white ? "ALB" : "NEGRU", thread_id, white_count, black_count);

        // Verificam daca trebuie sa schimbam tura
        int my_active = is_white ? white_count : black_count;
        if (my_active == 0) {
            int other_waiting = is_white ? black_waiting : white_waiting;
            
            if (other_waiting > 0) {
                // Fortam schimbarea turei catre culoarea opusa
                current_turn_color = is_white ? ThreadColor::Black : ThreadColor::White;
                has_turn = true;
                turn_counter = 0;
            } else {
                // Nimeni nu asteapta, resursa devine complet libera
                has_turn = false;
            }
        }

        // Notificam toate firele pentru a reevalua conditia CanAccess
        pthread_cond_broadcast(&cond);

        pthread_mutex_unlock(&mutex);
    }
};

// Contextul pasat fiecarui thread
struct ThreadContext {
    int id;
    ThreadColor color;
    FairResourceController* controller;
};

// Functia executata de thread
void* worker_thread(void* arg) {
    ThreadContext* ctx = static_cast<ThreadContext*>(arg);

    printf("[%s %d] Pornit...\n", ctx->color == ThreadColor::White ? "ALB" : "NEGRU", ctx->id);

    // 1. Request
    ctx->controller->RequestAccess(ctx->color, ctx->id);

    // 2. Critical Section (Simulare munca)
    int sleep_time = (rand() % 400 + 100) * 1000; // microsecunde
    usleep(sleep_time);

    // 3. Release
    ctx->controller->ReleaseAccess(ctx->color, ctx->id);

    delete ctx; // Curatam memoria alocata pentru context
    return NULL;
}

int main() {
    srand(time(NULL));

    const int NUM_WHITE = 6;
    const int NUM_BLACK = 6;
    const int FAIRNESS_LIMIT = 3; // Maxim 3 fire consecutive de aceeasi culoare daca exista opozitie

    FairResourceController controller(FAIRNESS_LIMIT);
    std::vector<pthread_t> threads;

    printf("=== Start Simulare Linux: %d Albe, %d Negre (Fairness: %d) ===\n\n", 
           NUM_WHITE, NUM_BLACK, FAIRNESS_LIMIT);

    int max_threads = std::max(NUM_WHITE, NUM_BLACK);

    // Lansam firele intercalat
    for (int i = 0; i < max_threads; ++i) {
        if (i < NUM_WHITE) {
            ThreadContext* ctx = new ThreadContext{i + 1, ThreadColor::White, &controller};
            pthread_t t;
            pthread_create(&t, NULL, worker_thread, ctx);
            threads.push_back(t);
        }
        
        usleep(10000); // Mic delay pentru realism

        if (i < NUM_BLACK) {
            ThreadContext* ctx = new ThreadContext{i + 1, ThreadColor::Black, &controller};
            pthread_t t;
            pthread_create(&t, NULL, worker_thread, ctx);
            threads.push_back(t);
        }
    }

    // Asteptam terminarea tuturor firelor (Join)
    for (pthread_t& t : threads) {
        pthread_join(t, NULL);
    }

    printf("\n=== Toate firele au terminat ===\n");

    return 0;
}*/
// Solutie - C++ Modern cu Windows API

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>

// Definim culorile pentru claritate
enum class ThreadColor {
    White,
    Black
};

// Clasa care gestioneaza sincronizarea si accesul la resursa
class FairResourceController {
private:
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;

    int white_count = 0;      // Fire albe active
    int black_count = 0;      // Fire negre active
    int white_waiting = 0;    // Fire albe in asteptare
    int black_waiting = 0;    // Fire negre in asteptare

    // Variabile pentru controlul starvation (echitate)
    ThreadColor current_turn_color = ThreadColor::White; // Culoarea care are "tura" curenta
    bool has_turn = false;    // Daca tura este atribuita activ cuiva
    int turn_counter = 0;     // Cate fire au trecut in tura curenta
    int max_per_turn;         // Limita pentru switch

    // Logica interna: Verifica daca un fir de o anumita culoare poate intra
    bool CanAccess(ThreadColor color) {
        bool is_white = (color == ThreadColor::White);
        int my_active = is_white ? white_count : black_count;
        int other_active = is_white ? black_count : white_count;
        int other_waiting = is_white ? black_waiting : white_waiting;

        // 1. Regula de baza: Excluziune reciproca intre culori
        if (other_active > 0) {
            return false;
        }

        // 2. Daca resursa e libera
        if (my_active == 0) {
            // Daca nu e tura nimanui sau e tura mea, intru
            if (!has_turn || current_turn_color == color) {
                return true;
            }
            // Daca e tura celuilalt, dar nu e nimeni acolo sa o foloseasca, intru (fur tura)
            if (other_waiting == 0) {
                return true;
            }
            return false;
        }

        // 3. Resursa e folosita deja de culoarea mea
        // Verificam Anti-Starvation: am depasit limita si celalalt asteapta?
        if (turn_counter >= max_per_turn && other_waiting > 0) {
            return false; // Cedam locul
        }

        return true;
    }

public:
    FairResourceController(int max_threads_per_turn) : max_per_turn(max_threads_per_turn) {
        InitializeCriticalSection(&cs);
        InitializeConditionVariable(&cv);
        has_turn = false;
    }

    ~FairResourceController() {
        DeleteCriticalSection(&cs);
    }

    // Nu permitem copierea controlerului
    FairResourceController(const FairResourceController&) = delete;
    FairResourceController& operator=(const FairResourceController&) = delete;

    void RequestAccess(ThreadColor color, int thread_id) {
        EnterCriticalSection(&cs);
        
        bool is_white = (color == ThreadColor::White);

        // Incrementam contorul de asteptare
        if (is_white) white_waiting++; else black_waiting++;

        // Loop de asteptare
        while (!CanAccess(color)) {
            SleepConditionVariableCS(&cv, &cs, INFINITE);
        }

        // Am primit acces: actualizam contoarele
        if (is_white) {
            white_waiting--;
            white_count++;
        } else {
            black_waiting--;
            black_count++;
        }

        // Actualizam logica de tura
        if (!has_turn || current_turn_color != color) {
            current_turn_color = color;
            has_turn = true;
            turn_counter = 1;
        } else {
            turn_counter++;
        }

        printf("[%s %d] -> ACCESS (Activ: A=%d, N=%d)\n", 
            is_white ? "ALB" : "NEGRU", thread_id, white_count, black_count);

        LeaveCriticalSection(&cs);
    }

    void ReleaseAccess(ThreadColor color, int thread_id) {
        EnterCriticalSection(&cs);

        bool is_white = (color == ThreadColor::White);

        if (is_white) white_count--; else black_count--;

        printf("[%s %d] <- FREE   (Activ: A=%d, N=%d)\n", 
            is_white ? "ALB" : "NEGRU", thread_id, white_count, black_count);

        // Daca ultimul fir de culoarea mea a iesit
        if ((is_white && white_count == 0) || (!is_white && black_count == 0)) {
            int other_waiting = is_white ? black_waiting : white_waiting;
            
            // Daca cineva de culoare opusa asteapta, schimbam tura fortat
            if (other_waiting > 0) {
                current_turn_color = is_white ? ThreadColor::Black : ThreadColor::White;
                has_turn = true;
                turn_counter = 0;
            } else {
                has_turn = false; // Resursa e complet libera
            }
        }

        // Trezim toti firele sa verifice conditiile
        WakeAllConditionVariable(&cv);

        LeaveCriticalSection(&cs);
    }
};

// Structura context pentru a pasa date catre thread
struct ThreadContext {
    int id;
    ThreadColor color;
    FairResourceController* controller;
};

// Functia thread-ului
DWORD WINAPI WorkerThread(LPVOID lpParam) {
    // Preluam contextul si il stergem automat la final (simulat, aici il folosim manual)
    ThreadContext* ctx = static_cast<ThreadContext*>(lpParam);
    
    // 1. Simulare pregatire
    Sleep(rand() % 100);

    // 2. Cere acces
    ctx->controller->RequestAccess(ctx->color, ctx->id);

    // 3. Sectiune Critica (Lucru cu resursa)
    Sleep((rand() % 400) + 100); 

    // 4. Elibereaza acces
    ctx->controller->ReleaseAccess(ctx->color, ctx->id);

    // Curatenie memorie context
    delete ctx; 
    return 0;
}

int main() {
    // Setup random
    srand((unsigned int)GetTickCount());

    // Configurare
    const int NUM_WHITE = 6;
    const int NUM_BLACK = 6;
    const int MAX_FAIRNESS = 3; // Max 3 fire consecutive de aceeasi culoare daca ceilalti asteapta

    FairResourceController controller(MAX_FAIRNESS);
    std::vector<HANDLE> handles;

    printf("=== Start Simulare: %d Albe, %d Negre (Fairness max: %d) ===\n\n", 
           NUM_WHITE, NUM_BLACK, MAX_FAIRNESS);

    // Lansam firele intercalat pentru a testa contention-ul
    int max_threads = max(NUM_WHITE, NUM_BLACK);
    
    for (int i = 0; i < max_threads; ++i) {
        if (i < NUM_WHITE) {
            ThreadContext* ctx = new ThreadContext{ i + 1, ThreadColor::White, &controller };
            handles.push_back(CreateThread(NULL, 0, WorkerThread, ctx, 0, NULL));
        }
        
        // Mic delay ca sa nu porneasca toate exact in aceeasi milisecunda (simulare realista)
        Sleep(10); 

        if (i < NUM_BLACK) {
            ThreadContext* ctx = new ThreadContext{ i + 1, ThreadColor::Black, &controller };
            handles.push_back(CreateThread(NULL, 0, WorkerThread, ctx, 0, NULL));
        }
    }

    // Asteptam terminarea tuturor
    WaitForMultipleObjects((DWORD)handles.size(), handles.data(), TRUE, INFINITE);

    // Inchidem handle-urile
    for (HANDLE h : handles) {
        CloseHandle(h);
    }

    printf("\n=== Simulare Finalizata ===\n");

    return 0;
}

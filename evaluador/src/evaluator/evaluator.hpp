// evaluator.hpp - El proceso evaluador (lo arranca `evaluator init`).
//
// Hilos (guía §3.2.4 y doc/04):
//   - i hilos de ENTRADA: uno por cola de entrada. Sacan muestras de la memoria
//     compartida y las despachan a la cola interna de su tipo.
//   - 3 hilos ANALIZADORES: uno por tipo (B, D, S). Toman de su cola interna,
//     consumen reactivo, "procesan" (duermen p segundos) y publican el resultado.
//   - 1 hilo ROBOT: único, recarga reactivo de un tipo por viaje.
//   - 1 hilo VIGILANTE: avisa de muestras que llevan demasiado tiempo esperando.
#pragma once

#include "queues/internal_queue.hpp"
#include "shared/shm.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace evaluator {

class Evaluator {
public:
    explicit Evaluator(SharedRegion& region);
    ~Evaluator();

    void start();    // crea todos los hilos
    void stop();     // pide a todos los hilos que terminen y los despierta
    void join();     // espera a que terminen

    void log(const std::string& who, const std::string& message);

private:
    void inputWorker(int k);
    void analyzerWorker(int t);
    void robotWorker();
    void watchdogWorker();

    // Duerme `seconds` segundos simulados (divididos por EVALUATOR_SPEED); se interrumpe con stop().
    bool sleepFor(double seconds);
    // Copia el contenido de la cola interna t al espejo de la memoria compartida (con mutexBoard tomado).
    void publishMirror(int t, const std::deque<Sample>& items);
    bool cancelled() const { return stop_.load(); }

    SharedRegion& region_;
    SharedHeader* h_;
    std::atomic<bool> stop_{false};

    std::array<InternalQueue, kTypeCount> internal_;
    // "Máquina" de cada analizador: la tiene el analizador mientras procesa y el
    // robot mientras carga reactivo. Así, mientras se carga, no se procesa (guía §2).
    std::array<std::mutex, kTypeCount> machine_;

    std::mutex sleepMutex_;
    std::condition_variable sleepCv_;
    std::mutex logMutex_;

    std::vector<std::thread> threads_;
};

} // namespace evaluator

// internal_queue.hpp - Cola interna de un tipo de muestra (guía §3.2.4, nota 3).
//
// Vive SOLO en la memoria del proceso init y la comparten sus hilos:
// productores = hilos de entrada, consumidor = el analizador de ese tipo.
// Como no cruza procesos basta con std::mutex + std::condition_variable.
// Es acotada (-q): si está llena, el hilo de entrada espera.
#pragma once

#include "shared/model.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>

namespace evaluator {

class InternalQueue {
public:
    // Callback que se ejecuta con el candado tomado, recibiendo el contenido actual
    // (se usa para publicar el "espejo" en la memoria compartida para ctrl).
    using Observer = std::function<void(const std::deque<Sample>&)>;

    void setCapacity(int capacity) { capacity_ = capacity; }

    // Espera espacio e inserta. false si se pidió detener.
    bool push(const Sample& sample, const std::atomic<bool>& stop, const Observer& observer) {
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait(lock, [&] { return stop.load() || static_cast<int>(items_.size()) < capacity_; });
        if (stop.load()) return false;
        items_.push_back(sample);
        if (observer) observer(items_);
        changed_.notify_all();
        return true;
    }

    // Espera a que haya algo y copia el primero SIN sacarlo.
    bool front(Sample& sample, const std::atomic<bool>& stop) {
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait(lock, [&] { return stop.load() || !items_.empty(); });
        if (stop.load()) return false;
        sample = items_.front();
        return true;
    }

    // Saca el primero (el que se obtuvo con front).
    void popFront(const Observer& observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!items_.empty()) items_.pop_front();
        if (observer) observer(items_);
        changed_.notify_all();
    }

    void wakeAll() { std::lock_guard<std::mutex> lock(mutex_); changed_.notify_all(); }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<Sample> items_;
    int capacity_{kDefaultInternalCap};
};

} // namespace evaluator

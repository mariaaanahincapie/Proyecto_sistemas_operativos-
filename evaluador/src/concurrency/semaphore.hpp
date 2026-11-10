// semaphore.hpp - Semáforos entre procesos.
//
// Linux:   el semáforo (sem_t) vive DENTRO de la memoria compartida y se
//          inicializa con sem_init(&s, 1, valor)  (pshared = 1).
// Windows: un semáforo del kernel no puede vivir en memoria compartida; en la
//          región solo se guarda su número (index) y el objeto real se crea con
//          nombre "Local\<segmento>_sem<index>". Cada proceso abre los handles.
//
// El resto del programa solo ve SemSlot (lo que está en la región) y
// SemContext (los handles propios de cada proceso).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <semaphore.h>
#endif

namespace evaluator {

struct SemSlot {
#ifndef _WIN32
    sem_t sem;
#endif
    std::uint32_t index;
};

class SemContext {
public:
    SemContext() = default;
    ~SemContext() { closeAll(); }
    SemContext(const SemContext&) = delete;
    SemContext& operator=(const SemContext&) = delete;

    void setPrefix(const std::string& prefix) { prefix_ = prefix; }

    // Lo usa solo init al crear la región. Devuelve false si falla.
    bool create(SemSlot& slot, int initial);
    // Lo usan los demás procesos: abre los `count` semáforos ya creados.
    bool attach(std::uint32_t count);
    std::uint32_t created() const { return next_; }
    void closeAll();

    // timeoutMs < 0: espera indefinida. true = obtuvo el semáforo.
    bool wait(SemSlot& slot, int timeoutMs = -1);
    void post(SemSlot& slot);

private:
    std::string prefix_;
    std::uint32_t next_{0};
#ifdef _WIN32
    std::string nameOf(std::uint32_t index) const;
    std::vector<HANDLE> handles_;
#endif
};

// Candado RAII sobre un semáforo binario (mutex entre procesos).
class SemLock {
public:
    SemLock(SemContext& ctx, SemSlot& slot) : ctx_(ctx), slot_(slot) { ctx_.wait(slot_); }
    ~SemLock() { ctx_.post(slot_); }
    SemLock(const SemLock&) = delete;
    SemLock& operator=(const SemLock&) = delete;
private:
    SemContext& ctx_;
    SemSlot& slot_;
};

} // namespace evaluator

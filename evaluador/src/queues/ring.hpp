// ring.hpp - Operaciones sobre las colas circulares de la memoria compartida
// (colas de entrada y cola de salida). Es el clásico productor-consumidor con
// tres semáforos:  empty (casillas libres), full (casillas ocupadas), mutex.
//
//   productor:  wait(empty); wait(mutex); escribir en tail; post(mutex); post(full)
//   consumidor: wait(full);  wait(mutex); leer de head;     post(mutex); post(empty)
//
// Regla anti-interbloqueo: primero se espera el contador (empty/full) y DESPUÉS
// se toma el mutex; nunca se duerme esperando espacio con el mutex tomado.
#pragma once

#include "shared/shm.hpp"

#include <functional>
#include <vector>

namespace evaluator {

// Devuelve true cuando hay que abandonar la espera (apagado, init muerto...).
using CancelFn = std::function<bool()>;
// Se ejecuta con el mutex de la cola tomado (para asignar id, actualizar tablero...).
using SampleFn = std::function<void(Sample&)>;

constexpr int kPollMs = 200;   // cada cuánto se revisa CancelFn mientras se espera

// Inserta `sample` en la cola. Si está llena, ESPERA. false = cancelado.
bool ringPush(SharedRegion& region, RingHeader& ring, Sample* slots, Sample sample,
              const CancelFn& cancelled, const SampleFn& underLock = nullptr);

// Extrae la muestra más antigua. Si está vacía, espera. false = cancelado.
// timeoutMs >= 0 limita la espera total (false si se vence).
bool ringPop(SharedRegion& region, RingHeader& ring, Sample* slots, Sample& out,
             const CancelFn& cancelled, const SampleFn& underLock = nullptr, int timeoutMs = -1);

// Copia del contenido actual sin extraer nada (para ctrl). El llamador decide si toma el mutex.
std::vector<Sample> ringContents(const RingHeader& ring, const Sample* slots);

} // namespace evaluator

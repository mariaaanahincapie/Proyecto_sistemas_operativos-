#include "queues/ring.hpp"

#include <algorithm>
#include <chrono>

namespace evaluator {

bool ringPush(SharedRegion& region, RingHeader& ring, Sample* slots, Sample sample,
              const CancelFn& cancelled, const SampleFn& underLock) {
    auto& sem = region.sem();
    // 1. Esperar una casilla libre (si la cola está llena, aquí se bloquea).
    while (!sem.wait(ring.empty, kPollMs)) {
        if (cancelled && cancelled()) return false;
    }
    // 2. Sección crítica corta.
    {
        SemLock lock(sem, ring.mutex);
        if (underLock) underLock(sample);
        slots[ring.tail] = sample;
        ring.tail = (ring.tail + 1) % ring.capacity;
        ++ring.count;
    }
    // 3. Avisar al consumidor.
    sem.post(ring.full);
    return true;
}

bool ringPop(SharedRegion& region, RingHeader& ring, Sample* slots, Sample& out,
             const CancelFn& cancelled, const SampleFn& underLock, int timeoutMs) {
    auto& sem = region.sem();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs);
    for (;;) {
        int wait = kPollMs;
        if (timeoutMs >= 0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) return false;
            wait = static_cast<int>(std::min<long long>(left, kPollMs));
        }
        if (sem.wait(ring.full, wait)) break;
        if (cancelled && cancelled()) return false;
    }
    {
        SemLock lock(sem, ring.mutex);
        out = slots[ring.head];
        ring.head = (ring.head + 1) % ring.capacity;
        --ring.count;
        if (underLock) underLock(out);
    }
    sem.post(ring.empty);   // libera una casilla para los productores
    return true;
}

std::vector<Sample> ringContents(const RingHeader& ring, const Sample* slots) {
    std::vector<Sample> items;
    items.reserve(static_cast<std::size_t>(ring.count));
    for (int i = 0; i < ring.count; ++i) items.push_back(slots[(ring.head + i) % ring.capacity]);
    return items;
}

} // namespace evaluator

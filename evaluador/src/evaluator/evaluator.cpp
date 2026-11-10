#include "evaluator/evaluator.hpp"

#include "queues/ring.hpp"
#include "shared/platform.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <sstream>

namespace evaluator {

namespace {

std::mt19937 makeRandom(int salt) {
    std::random_device device;
    std::seed_seq seq{device(), static_cast<unsigned>(std::time(nullptr)),
                      static_cast<unsigned>(platform::currentPid()), static_cast<unsigned>(salt)};
    return std::mt19937(seq);
}

// Tiempo de procesamiento uniforme en [min, max] según el tipo (§3.2.7).
int randomProcTime(std::mt19937& rng, int t) {
    return std::uniform_int_distribution<int>(kProcMin[t], kProcMax[t])(rng);
}

// Resultado: P 50 %, N 35 %, ? 15 % (§3.2.8).
char randomResult(std::mt19937& rng) {
    const int r = std::uniform_int_distribution<int>(0, 99)(rng);
    if (r < 50) return 'P';
    if (r < 85) return 'N';
    return '?';
}

std::string describe(const Sample& s) {
    std::ostringstream out;
    out << "muestra " << s.id << " (" << s.type << ", cant " << s.qty << ", cola " << s.queue << ")";
    return out.str();
}

} // namespace

Evaluator::Evaluator(SharedRegion& region) : region_(region), h_(region.hdr()) {
    for (auto& queue : internal_) queue.setCapacity(h_->cfg.internalCap);
}

Evaluator::~Evaluator() { stop(); join(); }

void Evaluator::start() {
    for (int k = 0; k < h_->cfg.inputs; ++k) threads_.emplace_back(&Evaluator::inputWorker, this, k);
    for (int t = 0; t < kTypeCount; ++t) threads_.emplace_back(&Evaluator::analyzerWorker, this, t);
    threads_.emplace_back(&Evaluator::robotWorker, this);
    threads_.emplace_back(&Evaluator::watchdogWorker, this);
}

void Evaluator::stop() {
    stop_.store(true);
    for (auto& queue : internal_) queue.wakeAll();
    std::lock_guard<std::mutex> lock(sleepMutex_);
    sleepCv_.notify_all();
}

void Evaluator::join() {
    for (auto& thread : threads_) if (thread.joinable()) thread.join();
    threads_.clear();
}

void Evaluator::log(const std::string& who, const std::string& message) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::lock_guard<std::mutex> lock(logMutex_);
    std::cout << '[' << std::put_time(&local, "%H:%M:%S") << "] " << std::left << std::setw(13) << who << message << std::endl;
}

bool Evaluator::sleepFor(double seconds) {
    const auto ms = std::chrono::milliseconds(static_cast<long long>(seconds * 1000.0 / platform::speedFactor()));
    std::unique_lock<std::mutex> lock(sleepMutex_);
    sleepCv_.wait_for(lock, ms, [&] { return stop_.load(); });
    return !stop_.load();
}

void Evaluator::publishMirror(int t, const std::deque<Sample>& items) {
    Sample* mirror = region_.internalMirror(t);
    const int n = static_cast<int>(std::min<std::size_t>(items.size(), static_cast<std::size_t>(h_->cfg.internalCap)));
    for (int i = 0; i < n; ++i) mirror[i] = items[static_cast<std::size_t>(i)];
    h_->internalCount[t] = n;
}

// ---------------------------------------------------------------------------
// Hilo de ENTRADA k: consumidor de la cola de entrada k (memoria compartida)
// y productor de las colas internas.
// ---------------------------------------------------------------------------
void Evaluator::inputWorker(int k) {
    auto& sem = region_.sem();
    const std::string who = "entrada " + std::to_string(k);
    while (!stop_.load()) {
        Sample s{};
        // Saca la muestra y, en la misma sección crítica, la marca "en mano" para que
        // ctrl nunca la pierda de vista.  Orden de candados: cola -> tablero.
        const bool got = ringPop(region_, region_.inRing(k), region_.inSlots(k), s,
            [this] { return cancelled(); },
            [&](Sample& taken) {
                SemLock board(sem, h_->mutexBoard);
                region_.holding(k).sample = taken;
                region_.holding(k).busy = 1;
            });
        if (!got) break;

        const int t = typeIndex(s.type);
        if (t < 0) continue;   // imposible: reg valida el tipo
        log(who, describe(s) + " -> cola interna " + s.type);

        // Despacha a la cola interna de su tipo. Si está llena, este hilo espera.
        const bool pushed = internal_[static_cast<std::size_t>(t)].push(s, stop_, [&](const std::deque<Sample>& items) {
            SemLock board(sem, h_->mutexBoard);
            publishMirror(t, items);
            region_.holding(k).busy = 0;
        });
        if (!pushed) break;
    }
}

// ---------------------------------------------------------------------------
// Hilo ANALIZADOR del tipo t.
// ---------------------------------------------------------------------------
void Evaluator::analyzerWorker(int t) {
    auto& sem = region_.sem();
    auto rng = makeRandom(100 + t);
    const std::string who = std::string("analizador ") + typeCode(t);
    auto& queue = internal_[static_cast<std::size_t>(t)];

    while (!stop_.load()) {
        Sample s{};
        if (!queue.front(s, stop_)) break;   // la muestra sigue visible como "waiting"

        // 1. Conseguir reactivo suficiente (o vencer por espera máxima).
        std::unique_lock<std::mutex> machine(machine_[static_cast<std::size_t>(t)], std::defer_lock);
        bool expired = false;
        bool announced = false;
        for (;;) {
            if (stop_.load()) return;
            const std::int64_t age = platform::nowSeconds() - s.enqueuedAt;
            if (age > h_->cfg.maxWait) { expired = true; break; }

            machine.lock();   // si el robot está cargando este analizador, aquí se espera
            bool enough = false, askRobot = false;
            int level = 0;
            {
                SemLock reactive(sem, h_->mutexReactive);
                level = h_->reactive[t];
                enough = level >= s.qty;
                if (enough) {
                    h_->reactive[t] -= s.qty;
                } else if (!h_->reqPending[t]) {
                    h_->reqPending[t] = 1;
                    h_->reqTicket[t] = h_->nextTicket++;   // turno FIFO para el robot
                    askRobot = true;
                }
            }
            if (enough) break;   // se conserva `machine` mientras se procesa
            machine.unlock();
            if (askRobot) sem.post(h_->robotReq);
            if (!announced) {
                log(who, "reactivo insuficiente para " + describe(s) + " (hay " + std::to_string(level) +
                         ", necesita " + std::to_string(s.qty) + "): espera al robot");
                announced = true;
            }
            sem.wait(h_->recharged[t], 1000);   // dormido; lo despierta el robot o ctrl update
        }

        // 2. Sacarla de la cola interna y, en la misma sección, marcarla "processing".
        if (!expired) s.procTime = randomProcTime(rng, t);
        queue.popFront([&](const std::deque<Sample>& items) {
            SemLock board(sem, h_->mutexBoard);
            publishMirror(t, items);
            if (!expired) {
                h_->processing[t] = s;
                h_->busy[t] = 1;
                h_->procStartedAt[t] = platform::nowSeconds();
            }
        });

        // 3. Procesar.
        if (expired) {
            s.procTime = 0;
            s.result = '?';
            log(who, describe(s) + " superó la espera máxima (" + std::to_string(h_->cfg.maxWait) +
                     " s): se reporta '?' sin gastar reactivo");
        } else {
            log(who, "procesando " + describe(s) + " durante " + std::to_string(s.procTime) + " s");
            if (!sleepFor(s.procTime)) return;
            s.result = randomResult(rng);
            machine.unlock();   // el analizador queda libre (el robot ya podría cargarlo)
            log(who, "terminó muestra " + std::to_string(s.id) + " -> " + s.result);
        }

        // 4. Depositar en la cola de salida. Si está llena, ESPERA (no se excede la capacidad).
        const bool deposited = ringPush(region_, region_.outRing(), region_.outSlots(), s,
            [this] { return cancelled(); },
            [&](Sample&) {
                SemLock board(sem, h_->mutexBoard);
                h_->busy[t] = 0;
                if (expired) ++h_->statExpired; else ++h_->statProcessed;
            });
        if (!deposited) break;
    }
}

// ---------------------------------------------------------------------------
// Hilo ROBOT: único. Atiende los pedidos en orden de llegada, un tipo por viaje.
// ---------------------------------------------------------------------------
void Evaluator::robotWorker() {
    auto& sem = region_.sem();
    while (!stop_.load()) {
        if (!sem.wait(h_->robotReq, kPollMs)) continue;   // dormido hasta que alguien pida

        int t = -1;
        {
            SemLock reactive(sem, h_->mutexReactive);
            std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
            for (int i = 0; i < kTypeCount; ++i) {
                if (h_->reqPending[i] && h_->reqTicket[i] < best) { best = h_->reqTicket[i]; t = i; }
            }
            if (t >= 0) { h_->robotState = RobotToStore; h_->robotType = t; h_->robotLoad = 0; }
        }
        if (t < 0) continue;
        const char code = typeCode(t);

        log("robot", std::string("va al almacén por reactivo ") + code);
        if (!sleepFor(kRobotTripSeconds)) return;
        {
            SemLock reactive(sem, h_->mutexReactive);
            h_->robotLoad = h_->cfg.robotCap;   // el almacén nunca se agota; lleva su capacidad (-ee)
            h_->robotState = RobotToAnalyzer;
        }
        log("robot", "regresa con " + std::to_string(h_->cfg.robotCap) + " unidades de " + code);
        if (!sleepFor(kRobotTripSeconds)) return;

        int added = 0, level = 0;
        {
            // Toma la "máquina": si el analizador está procesando, espera a que termine
            // esa muestra; mientras carga, el analizador no puede procesar otra.
            std::lock_guard<std::mutex> machine(machine_[static_cast<std::size_t>(t)]);
            { SemLock reactive(sem, h_->mutexReactive); h_->robotState = RobotLoading; }
            log("robot", std::string("cargando el analizador ") + code + " (no procesa mientras tanto)");
            if (!sleepFor(kRobotLoadSeconds)) return;
            SemLock reactive(sem, h_->mutexReactive);
            added = std::min(h_->robotLoad, kMaxReactive - h_->reactive[t]);
            if (added < 0) added = 0;
            h_->reactive[t] += added;
            level = h_->reactive[t];
            h_->reqPending[t] = 0;
            h_->robotState = RobotIdle;
            h_->robotType = -1;
            h_->robotLoad = 0;
        }
        sem.post(h_->recharged[t]);   // despierta al analizador
        log("robot", "entregó " + std::to_string(added) + " unidades de " + code + " (nivel " + std::to_string(level) + ")");
    }
}

// ---------------------------------------------------------------------------
// Hilo VIGILANTE: cada 5 s busca muestras que llevan más de la mitad de la espera
// máxima y lo avisa en la consola (una vez por muestra).
// ---------------------------------------------------------------------------
void Evaluator::watchdogWorker() {
    auto& sem = region_.sem();
    std::set<std::uint64_t> warned;
    const std::int64_t threshold = std::max(1, h_->cfg.maxWait / 2);
    while (sleepFor(kWatchdogPeriod)) {
        std::vector<Sample> waiting;
        for (int k = 0; k < h_->cfg.inputs; ++k) {
            SemLock lock(sem, region_.inRing(k).mutex);
            for (const auto& s : ringContents(region_.inRing(k), region_.inSlots(k))) waiting.push_back(s);
        }
        {
            SemLock board(sem, h_->mutexBoard);
            for (int k = 0; k < h_->cfg.inputs; ++k) if (region_.holding(k).busy) waiting.push_back(region_.holding(k).sample);
            for (int t = 0; t < kTypeCount; ++t)
                for (int i = 0; i < h_->internalCount[t]; ++i) waiting.push_back(region_.internalMirror(t)[i]);
        }
        const std::int64_t now = platform::nowSeconds();
        std::set<std::uint64_t> seen;
        for (const auto& s : waiting) {
            seen.insert(s.id);
            const std::int64_t age = now - s.enqueuedAt;
            if (age >= threshold && warned.insert(s.id).second) {
                log("vigilante", describe(s) + " lleva " + std::to_string(age) + " s esperando (límite " +
                                 std::to_string(h_->cfg.maxWait) + " s)");
            }
        }
        for (auto it = warned.begin(); it != warned.end();) it = seen.count(*it) ? std::next(it) : warned.erase(it);
    }
}

} // namespace evaluator

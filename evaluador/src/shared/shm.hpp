// shm.hpp - La única región de memoria compartida del sistema (guía §3.2.1).
//
// Distribución (los offsets se calculan en init y se guardan en la cabecera):
//
//   [ SharedHeader ]
//   [ RingHeader de la cola de entrada 0 .. i-1 ]
//   [ casillas de las colas de entrada: i * ie muestras ]
//   [ RingHeader de la cola de salida ]
//   [ casillas de la cola de salida: oe muestras ]
//   [ "en mano" de cada hilo de entrada: i HoldingSlot ]
//   [ espejo de las colas internas: 3 * q muestras ]
//
// El tamaño depende de -i, -ie, -oe y -q: init crea la región exacta.
#pragma once

#include "concurrency/semaphore.hpp"
#include "shared/model.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace evaluator {

constexpr std::uint32_t kMagic   = 0x4556414Cu;   // "EVAL"
constexpr std::uint32_t kVersion = 3;

struct Config {
    std::int32_t inputs{kDefaultInputs};           // -i
    std::int32_t inputCap{kDefaultInputCap};       // -ie
    std::int32_t outputCap{kDefaultOutputCap};     // -oe
    std::int32_t internalCap{kDefaultInternalCap}; // -q
    std::int32_t reactive[kTypeCount]{kDefaultReactive, kDefaultReactive, kDefaultReactive}; // -b -d -s
    std::int32_t robotCap{kDefaultRobotCap};       // -ee
    std::int32_t maxWait{kDefaultMaxWait};         // EVALUATOR_MAX_WAIT
};

// Cola circular productor-consumidor que vive en la memoria compartida.
struct RingHeader {
    std::int32_t capacity;
    std::int32_t head;    // próxima posición a LEER
    std::int32_t tail;    // próxima posición a ESCRIBIR
    std::int32_t count;   // elementos actuales
    SemSlot mutex;        // exclusión mutua sobre head/tail/count/casillas (inicia en 1)
    SemSlot empty;        // casillas libres   (inicia en capacity)
    SemSlot full;         // casillas ocupadas (inicia en 0)
};

// Muestra que un hilo de entrada ya sacó de su cola y aún no logra dejar en la cola interna.
struct HoldingSlot {
    std::int32_t busy;
    std::int32_t pad;
    Sample sample;
};

// Estado del robot (para ctrl list reactive).
enum RobotState : std::int32_t { RobotIdle = 0, RobotToStore = 1, RobotToAnalyzer = 2, RobotLoading = 3 };

struct SharedHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t totalSize;
    std::int64_t  initPid;               // para saber si init sigue vivo
    std::int64_t  startedAt;
    std::atomic<std::int32_t> ready;     // 1 cuando init terminó de inicializar
    std::atomic<std::int32_t> shuttingDown;
    Config        cfg;
    std::uint32_t semCount;              // cuántos semáforos hay (Windows los abre por número)

    std::uint64_t offInRings, offInSlots, offOutRing, offOutSlots, offHolding, offInternal;

    // --- Identificadores únicos ---------------------------------------------
    SemSlot       mutexId;
    std::uint64_t nextId;
    std::uint64_t statRegistered;

    // --- Reactivos y robot ----------------------------------------------------
    SemSlot       mutexReactive;
    std::int32_t  reactive[kTypeCount];  // nivel actual (0..100)
    std::int32_t  reqPending[kTypeCount];// 1 = el analizador pidió recarga
    std::uint64_t reqTicket[kTypeCount]; // orden de llegada del pedido (FIFO)
    std::uint64_t nextTicket;
    std::int32_t  robotState;            // RobotState
    std::int32_t  robotType;             // tipo que transporta (-1 = ninguno)
    std::int32_t  robotLoad;             // unidades que lleva
    std::int32_t  pad0;
    SemSlot       robotReq;              // pedidos pendientes para el robot
    SemSlot       recharged[kTypeCount]; // el analizador t duerme aquí esperando reactivo

    // --- Tablero de estado (lo lee ctrl) -------------------------------------
    SemSlot       mutexBoard;
    Sample        processing[kTypeCount];
    std::int32_t  busy[kTypeCount];
    std::int32_t  internalCount[kTypeCount];
    std::int64_t  procStartedAt[kTypeCount];
    std::uint64_t statProcessed;
    std::uint64_t statExpired;
};

class SharedRegion {
public:
    SharedRegion() = default;
    ~SharedRegion() { close(); }
    SharedRegion(const SharedRegion&) = delete;
    SharedRegion& operator=(const SharedRegion&) = delete;

    // Nombre válido: letras, dígitos, '_' o '-', de 1 a 40 caracteres.
    static bool validName(const std::string& name);

    // init: crea la región. `alreadyRunning` = true si otro init vivo usa el nombre.
    bool create(const std::string& name, const Config& cfg, bool& alreadyRunning, std::string& error);
    // reg / ctrl / rep: abre una región existente.
    bool open(const std::string& name, std::string& error);
    void close();
    void unlink();   // borra el nombre (solo init, al terminar)

    SharedHeader* hdr() const { return hdr_; }
    SemContext& sem() { return sem_; }

    RingHeader& inRing(int k)  const { return at<RingHeader>(hdr_->offInRings)[k]; }
    Sample*     inSlots(int k) const { return at<Sample>(hdr_->offInSlots) + static_cast<std::size_t>(k) * hdr_->cfg.inputCap; }
    RingHeader& outRing()      const { return *at<RingHeader>(hdr_->offOutRing); }
    Sample*     outSlots()     const { return at<Sample>(hdr_->offOutSlots); }
    HoldingSlot& holding(int k) const { return at<HoldingSlot>(hdr_->offHolding)[k]; }
    Sample*     internalMirror(int t) const { return at<Sample>(hdr_->offInternal) + static_cast<std::size_t>(t) * hdr_->cfg.internalCap; }

    // ¿El evaluador sigue en pie? (no se está cerrando y el proceso init vive)
    bool systemAlive() const;

private:
    template <typename T>
    T* at(std::uint64_t offset) const { return reinterpret_cast<T*>(reinterpret_cast<char*>(hdr_) + offset); }
    bool mapCreate(std::size_t size, bool& exists, std::string& error);

    std::string name_;
    SharedHeader* hdr_{nullptr};
    std::size_t size_{0};
    SemContext sem_;
#ifdef _WIN32
    HANDLE mapping_{nullptr};
#else
    int fd_{-1};
#endif
};

} // namespace evaluator

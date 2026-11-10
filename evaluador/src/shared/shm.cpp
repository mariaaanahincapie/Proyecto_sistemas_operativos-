#include "shared/shm.hpp"
#include "shared/platform.hpp"

#include <cctype>
#include <cstring>
#include <new>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace evaluator {

namespace {
std::uint64_t alignUp(std::uint64_t value) { return (value + 63u) & ~std::uint64_t{63u}; }

#ifdef _WIN32
std::string osName(const std::string& name) { return "Local\\" + name; }
#else
std::string osName(const std::string& name) { return "/" + name; }   // queda en /dev/shm/<name>
#endif
} // namespace

bool SharedRegion::validName(const std::string& name) {
    if (name.empty() || name.size() > 40) return false;
    for (unsigned char c : name) {
        if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

bool SharedRegion::systemAlive() const {
    return hdr_ && hdr_->shuttingDown.load() == 0 && platform::processAlive(hdr_->initPid);
}

// ---------------------------------------------------------------------------
// Creación (init)
// ---------------------------------------------------------------------------

bool SharedRegion::mapCreate(std::size_t size, bool& exists, std::string& error) {
    exists = false;
#ifdef _WIN32
    const auto high = static_cast<DWORD>(static_cast<std::uint64_t>(size) >> 32);
    const auto low  = static_cast<DWORD>(size & 0xffffffffu);
    mapping_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, high, low, osName(name_).c_str());
    if (!mapping_) { error = "CreateFileMapping falló"; return false; }
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(mapping_); mapping_ = nullptr; exists = true; return false; }
    void* view = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!view) { error = "MapViewOfFile falló"; CloseHandle(mapping_); mapping_ = nullptr; return false; }
#else
    fd_ = shm_open(osName(name_).c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd_ < 0) {
        if (errno == EEXIST) exists = true; else error = std::string("shm_open: ") + std::strerror(errno);
        return false;
    }
    if (ftruncate(fd_, static_cast<off_t>(size)) != 0) {
        error = std::string("ftruncate: ") + std::strerror(errno);
        ::close(fd_); fd_ = -1; shm_unlink(osName(name_).c_str());
        return false;
    }
    void* view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (view == MAP_FAILED) {
        error = std::string("mmap: ") + std::strerror(errno);
        ::close(fd_); fd_ = -1; shm_unlink(osName(name_).c_str());
        return false;
    }
#endif
    std::memset(view, 0, size);
    hdr_ = static_cast<SharedHeader*>(view);
    size_ = size;
    return true;
}

bool SharedRegion::create(const std::string& name, const Config& cfg, bool& alreadyRunning, std::string& error) {
    name_ = name;
    alreadyRunning = false;
    sem_.setPrefix(name);

    // 1. Calcular el tamaño y los offsets de cada parte.
    const auto inputs = static_cast<std::uint64_t>(cfg.inputs);
    std::uint64_t offset = alignUp(sizeof(SharedHeader));
    const std::uint64_t offInRings  = offset; offset = alignUp(offset + inputs * sizeof(RingHeader));
    const std::uint64_t offInSlots  = offset; offset = alignUp(offset + inputs * cfg.inputCap * sizeof(Sample));
    const std::uint64_t offOutRing  = offset; offset = alignUp(offset + sizeof(RingHeader));
    const std::uint64_t offOutSlots = offset; offset = alignUp(offset + static_cast<std::uint64_t>(cfg.outputCap) * sizeof(Sample));
    const std::uint64_t offHolding  = offset; offset = alignUp(offset + inputs * sizeof(HoldingSlot));
    const std::uint64_t offInternal = offset; offset = alignUp(offset + static_cast<std::uint64_t>(kTypeCount) * cfg.internalCap * sizeof(Sample));
    const auto size = static_cast<std::size_t>(offset);

    // 2. Crear la región. Si ya existe, ver si su init sigue vivo.
    bool exists = false;
    if (!mapCreate(size, exists, error)) {
        if (!exists) return false;
#ifdef _WIN32
        // En Windows el objeto existe mientras algún proceso lo tenga abierto.
        alreadyRunning = true;
        error = "ya existe un sistema con el nombre \"" + name + "\" (o quedan procesos conectados a él)";
        return false;
#else
        SharedRegion old;
        std::string ignored;
        if (old.open(name, ignored) && platform::processAlive(old.hdr()->initPid) && old.hdr()->shuttingDown.load() == 0) {
            alreadyRunning = true;
            error = "el sistema \"" + name + "\" ya está iniciado (pid " + std::to_string(old.hdr()->initPid) + ")";
            return false;
        }
        old.close();
        shm_unlink(osName(name).c_str());   // huérfano de un init que murió: se borra
        if (!mapCreate(size, exists, error)) {
            if (exists) error = "no se pudo reemplazar el segmento huérfano";
            return false;
        }
#endif
    }

    // 3. Construir la cabecera (placement new) y escribir valores iniciales.
    auto* h = new (hdr_) SharedHeader();
    h->magic = kMagic;
    h->version = kVersion;
    h->totalSize = size;
    h->initPid = platform::currentPid();
    h->startedAt = platform::nowSeconds();
    h->cfg = cfg;
    h->offInRings = offInRings; h->offInSlots = offInSlots;
    h->offOutRing = offOutRing; h->offOutSlots = offOutSlots;
    h->offHolding = offHolding; h->offInternal = offInternal;
    h->nextId = 1;
    h->nextTicket = 1;
    h->robotState = RobotIdle;
    h->robotType = -1;
    for (int t = 0; t < kTypeCount; ++t) h->reactive[t] = cfg.reactive[t];

    // 4. Semáforos (todos viven en la región).
    bool ok = sem_.create(h->mutexId, 1) && sem_.create(h->mutexReactive, 1) &&
              sem_.create(h->robotReq, 0) && sem_.create(h->mutexBoard, 1);
    for (int t = 0; t < kTypeCount && ok; ++t) ok = sem_.create(h->recharged[t], 0);
    for (int k = 0; k < cfg.inputs && ok; ++k) {
        RingHeader& ring = inRing(k);
        ring.capacity = cfg.inputCap;
        ok = sem_.create(ring.mutex, 1) && sem_.create(ring.empty, cfg.inputCap) && sem_.create(ring.full, 0);
    }
    if (ok) {
        RingHeader& ring = outRing();
        ring.capacity = cfg.outputCap;
        ok = sem_.create(ring.mutex, 1) && sem_.create(ring.empty, cfg.outputCap) && sem_.create(ring.full, 0);
    }
    if (!ok) { error = "no se pudieron crear los semáforos"; unlink(); close(); return false; }
    h->semCount = sem_.created();
    h->ready.store(1);   // desde aquí reg/ctrl/rep pueden conectarse
    return true;
}

// ---------------------------------------------------------------------------
// Apertura (reg, ctrl, rep)
// ---------------------------------------------------------------------------

bool SharedRegion::open(const std::string& name, std::string& error) {
    name_ = name;
    sem_.setPrefix(name);
#ifdef _WIN32
    mapping_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, osName(name).c_str());
    if (!mapping_) { error = "el sistema \"" + name + "\" no está iniciado (ejecute evaluator init)"; return false; }
    void* view = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, 0);   // 0 = toda la región
    if (!view) { error = "MapViewOfFile falló"; close(); return false; }
    MEMORY_BASIC_INFORMATION info{};
    VirtualQuery(view, &info, sizeof(info));
    size_ = info.RegionSize;
#else
    fd_ = shm_open(osName(name).c_str(), O_RDWR, 0600);
    if (fd_ < 0) { error = "el sistema \"" + name + "\" no está iniciado (ejecute evaluator init)"; return false; }
    struct stat st{};
    if (fstat(fd_, &st) != 0 || static_cast<std::size_t>(st.st_size) < sizeof(SharedHeader)) {
        error = "el segmento \"" + name + "\" está incompleto"; close(); return false;
    }
    size_ = static_cast<std::size_t>(st.st_size);
    void* view = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (view == MAP_FAILED) { error = "mmap falló"; close(); return false; }
#endif
    hdr_ = static_cast<SharedHeader*>(view);
    if (hdr_->magic != kMagic || hdr_->version != kVersion || hdr_->ready.load() != 1 || hdr_->totalSize > size_) {
        error = "el segmento \"" + name + "\" no pertenece a esta versión del evaluador o no terminó de iniciar";
        close();
        return false;
    }
    if (!sem_.attach(hdr_->semCount)) { error = "no se pudieron abrir los semáforos"; close(); return false; }
    return true;
}

void SharedRegion::close() {
    sem_.closeAll();
#ifdef _WIN32
    if (hdr_) UnmapViewOfFile(hdr_);
    if (mapping_) CloseHandle(mapping_);
    mapping_ = nullptr;
#else
    if (hdr_) munmap(hdr_, size_);
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
#endif
    hdr_ = nullptr;
    size_ = 0;
}

void SharedRegion::unlink() {
#ifndef _WIN32
    // Los sem_t viven dentro de la región: desaparecen con ella. No se llama a
    // sem_destroy porque algún reg/rep podría estar en sem_timedwait sobre ellos;
    // la memoria se libera cuando el último proceso hace munmap.
    if (!name_.empty()) shm_unlink(osName(name_).c_str());
#endif
}

} // namespace evaluator

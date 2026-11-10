#include "concurrency/semaphore.hpp"

#ifndef _WIN32
#include <cerrno>
#include <ctime>
#endif

namespace evaluator {

#ifdef _WIN32

std::string SemContext::nameOf(std::uint32_t index) const {
    return "Local\\" + prefix_ + "_sem" + std::to_string(index);
}

bool SemContext::create(SemSlot& slot, int initial) {
    slot.index = next_++;
    HANDLE handle = CreateSemaphoreA(nullptr, initial, 0x7fffffff, nameOf(slot.index).c_str());
    if (!handle) return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(handle); return false; }
    handles_.push_back(handle);
    return true;
}

bool SemContext::attach(std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i) {
        HANDLE handle = OpenSemaphoreA(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, FALSE, nameOf(i).c_str());
        if (!handle) { closeAll(); return false; }
        handles_.push_back(handle);
    }
    next_ = count;
    return true;
}

void SemContext::closeAll() {
    for (HANDLE handle : handles_) CloseHandle(handle);
    handles_.clear();
}

bool SemContext::wait(SemSlot& slot, int timeoutMs) {
    const DWORD ms = timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs);
    return WaitForSingleObject(handles_[slot.index], ms) == WAIT_OBJECT_0;
}

void SemContext::post(SemSlot& slot) { ReleaseSemaphore(handles_[slot.index], 1, nullptr); }

#else  // ---------------------------------------------------------------- Linux

bool SemContext::create(SemSlot& slot, int initial) {
    slot.index = next_++;
    return sem_init(&slot.sem, /*pshared=*/1, static_cast<unsigned>(initial)) == 0;
}

bool SemContext::attach(std::uint32_t count) { next_ = count; return true; }  // nada que abrir

void SemContext::closeAll() {}

bool SemContext::wait(SemSlot& slot, int timeoutMs) {
    if (timeoutMs < 0) {
        while (sem_wait(&slot.sem) != 0) {
            if (errno != EINTR) return false;   // una señal interrumpió: reintentar
        }
        return true;
    }
    timespec deadline{};
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeoutMs / 1000;
    deadline.tv_nsec += static_cast<long>(timeoutMs % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec += 1; deadline.tv_nsec -= 1000000000L; }
    for (;;) {
        if (sem_timedwait(&slot.sem, &deadline) == 0) return true;
        if (errno == EINTR) continue;
        return false;   // ETIMEDOUT
    }
}

void SemContext::post(SemSlot& slot) { sem_post(&slot.sem); }

#endif

} // namespace evaluator

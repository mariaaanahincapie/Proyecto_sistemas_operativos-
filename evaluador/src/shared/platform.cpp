#include "shared/platform.hpp"

#include <csignal>
#include <cstdlib>
#include <ctime>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <cstdio>
#else
#include <cerrno>
#include <climits>
#include <unistd.h>
#include <sys/types.h>
#endif

namespace evaluator::platform {

namespace {
std::atomic<bool>* gStopFlag = nullptr;

#ifdef _WIN32
// La consola de Windows usa por omisión otra página de códigos: sin esto los
// acentos de los mensajes salen dañados en cmd/PowerShell.
struct Utf8Console {
    Utf8Console() { SetConsoleOutputCP(CP_UTF8); }
} gUtf8Console;
#endif

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
        if (gStopFlag) gStopFlag->store(true);
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int) {
    if (gStopFlag) gStopFlag->store(true);   // std::atomic<bool> es lock-free: seguro en una señal
}
#endif
} // namespace

std::int64_t currentPid() {
#ifdef _WIN32
    return static_cast<std::int64_t>(GetCurrentProcessId());
#else
    return static_cast<std::int64_t>(getpid());
#endif
}

bool processAlive(std::int64_t pid) {
    if (pid <= 0) return false;
#ifdef _WIN32
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
    const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    CloseHandle(process);
    return alive;
#else
    return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

std::int64_t nowSeconds() { return static_cast<std::int64_t>(std::time(nullptr)); }

bool stdinIsTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

std::string executableDir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    std::string path(buffer, length);
    const auto slash = path.find_last_of("\\/");
#else
    char buffer[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) return ".";
    std::string path(buffer, static_cast<std::size_t>(length));
    const auto slash = path.find_last_of('/');
#endif
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

void installStopHandler(std::atomic<bool>* flag) {
    gStopFlag = flag;
#ifdef _WIN32
    SetConsoleCtrlHandler(nullptr, FALSE);   // por si se heredó "ignorar Ctrl+C"
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    struct sigaction action{};
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;   // sin SA_RESTART: las esperas bloqueantes se interrumpen
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
#endif
}

int envPositiveInt(const char* name, int fallback) {
    const char* text = std::getenv(name);
    if (!text || !*text) return fallback;
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (*end != '\0' || value <= 0 || value > 1000000) return fallback;
    return static_cast<int>(value);
}

int speedFactor() {
    static const int factor = envPositiveInt("EVALUATOR_SPEED", 1);
    return factor;
}

} // namespace evaluator::platform

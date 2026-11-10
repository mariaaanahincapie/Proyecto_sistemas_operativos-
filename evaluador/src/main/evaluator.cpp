// evaluator.cpp - El envoltorio (wrapper) que usa el usuario (guía §3).
//
//   evaluator {init | reg | ctrl | rep} [opciones...]
//
// Solo decide qué programa real ejecutar y le pasa los argumentos:
//   evaluator init ... -> <dir>/evaluator-init ...
//   evaluator reg  ... -> <dir>/evaluator-reg  ...
//   evaluator ctrl ... -> <dir>/evaluator-ctrl ...
//   evaluator rep  ... -> <dir>/evaluator-rep  ...
// <dir> es EVALUATOR_BIN si está definida; si no, la carpeta del propio wrapper.
//
// Linux:   execv reemplaza al wrapper por el programa real (mismo pid, misma consola,
//          las señales como Ctrl+C llegan directo al programa real).
// Windows: _spawnv(_P_WAIT) lanza el programa real, espera y devuelve su código.
#include "shared/model.hpp"
#include "shared/platform.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <cerrno>
#include <unistd.h>
#endif

using namespace evaluator;

namespace {
void usage() {
    std::cerr << "uso: evaluator <comando> [opciones]\n"
                 "  init [-i n] [-ie n] [-oe n] [-n nombre] [-b n] [-d n] [-ee n] [-s n] [-q n]\n"
                 "  reg  [-n nombre] { <archivo> ... | - }\n"
                 "  ctrl [-s nombre]\n"
                 "  rep  [-s nombre] { -i segundos | -m cantidad }\n";
}
#ifdef _WIN32
// El wrapper ignora Ctrl+C para no morir antes que el programa real (que sí lo atiende).
BOOL WINAPI ignoreCtrlC(DWORD event) { return event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT; }
#endif
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return kExitBadArgs; }
    const std::string command = argv[1];
    if (command != "init" && command != "reg" && command != "ctrl" && command != "rep") {
        std::cerr << "evaluator: comando desconocido '" << command << "'\n";
        usage();
        return kExitBadArgs;
    }

    std::string dir = platform::executableDir();
    if (const char* env = std::getenv("EVALUATOR_BIN"); env && *env) dir = env;
#ifdef _WIN32
    const std::string program = dir + "\\evaluator-" + command + ".exe";
#else
    const std::string program = dir + "/evaluator-" + command;
#endif

    // argv del programa real: [programa, opciones...] (se quita el nombre del comando).
    std::vector<char*> args;
    args.push_back(const_cast<char*>(program.c_str()));
    for (int i = 2; i < argc; ++i) args.push_back(argv[i]);
    args.push_back(nullptr);

#ifdef _WIN32
    // _spawnv pasa los argumentos a la línea de comandos: hay que entrecomillar los que tengan espacios.
    std::vector<std::string> quoted;
    for (int i = 0; args[i]; ++i) {
        std::string a = args[i];
        quoted.push_back(a.find(' ') != std::string::npos ? "\"" + a + "\"" : a);
    }
    std::vector<const char*> wargs;
    for (auto& q : quoted) wargs.push_back(q.c_str());
    wargs.push_back(nullptr);
    SetConsoleCtrlHandler(ignoreCtrlC, TRUE);
    const intptr_t code = _spawnv(_P_WAIT, program.c_str(), wargs.data());
    if (code == -1) { std::cerr << "evaluator: no se pudo ejecutar " << program << '\n'; return kExitInternal; }
    return static_cast<int>(code);
#else
    execv(program.c_str(), args.data());
    std::cerr << "evaluator: no se pudo ejecutar " << program << ": " << std::strerror(errno) << '\n';
    return kExitInternal;
#endif
}

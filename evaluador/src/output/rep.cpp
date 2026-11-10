// rep.cpp - Programa real de `evaluator rep` (bin/evaluator-rep).
//
//   evaluator rep [-s <string>] { -i <segundos> | -m <cantidad> }
//
// Consumidor de la cola de salida. Cada resultado que toma lo RETIRA de la
// salida (libera el reporte), así que dos rep nunca reciben el mismo.
//   -i s : modo interactivo; durante s segundos imprime cada resultado que llegue.
//   -m n : obtiene exactamente n resultados; si no hay, espera hasta completarlos.
#include "output/format.hpp"
#include "queues/ring.hpp"
#include "shared/cli.hpp"
#include "shared/shm.hpp"

#include <chrono>
#include <iostream>
#include <string>

using namespace evaluator;

namespace {
void usage() { std::cerr << "uso: evaluator rep [-s <string>] { -i <segundos> | -m <cantidad> }\n"; }
}

int main(int argc, char** argv) {
    std::string name, mode;
    int amount = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        if (i + 1 >= argc) { usage(); return kExitBadArgs; }
        const std::string value = argv[++i];
        if (opt == "-s") name = value;
        else if ((opt == "-i" || opt == "-m") && mode.empty()) {
            mode = opt;
            if (!cli::parseInt(value, 1, 1000000, amount)) {
                std::cerr << "evaluator rep: " << opt << " requiere un entero positivo\n"; return kExitBadArgs;
            }
        } else { usage(); return kExitBadArgs; }
    }
    if (mode.empty()) { usage(); return kExitBadArgs; }
    name = cli::resolveName(name);

    SharedRegion region;
    std::string error;
    if (!SharedRegion::validName(name) || !region.open(name, error)) {
        std::cerr << "evaluator rep: " << (error.empty() ? "nombre inválido" : error) << '\n';
        return kExitNotRunning;
    }

    auto& ring = region.outRing();
    // Si init se cae, se sale solo cuando ya no quede nada en la salida.
    auto cancelled = [&region, &ring] { return !region.systemAlive() && ring.count == 0; };

    if (mode == "-m") {
        for (int got = 0; got < amount; ++got) {
            Sample s{};
            if (!ringPop(region, ring, region.outSlots(), s, cancelled)) {
                std::cerr << "evaluator rep: el sistema se detuvo; se obtuvieron " << got << " de " << amount << '\n';
                return kExitNotRunning;
            }
            std::cout << format::report(s) << std::endl;
        }
        return kExitOk;
    }

    // -i: espera hasta `amount` segundos imprimiendo lo que llegue.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(amount);
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) break;
        Sample s{};
        if (ringPop(region, ring, region.outSlots(), s, cancelled, nullptr, static_cast<int>(left))) {
            std::cout << format::report(s) << std::endl;
        } else if (cancelled()) {
            std::cerr << "evaluator rep: el sistema se detuvo\n";
            return kExitNotRunning;
        }
    }
    return kExitOk;
}
